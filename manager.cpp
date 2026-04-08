#include "manager.hpp"


//wake up all and stop flag
void manager::suffocate()
    {
        status.stop_flag=true;
        file_io.wake_up_all();
        status.cv_wake_monitor.notify_all();
        if(disk_thread.joinable()) {
            close_file();
            disk_thread.join();
            std::cout<<"disk_thread exits"<<std::endl;
        }
        if(parse_thread.joinable()) 
        {
            parse_thread.join();
            std::cout<<"parse_thread exits"<<std::endl;
        }
        // avoid joining monitor_thread from itself
        if(monitor_thread.joinable() && monitor_thread.get_id()!=std::this_thread::get_id()) {
            monitor_thread.join();
            std::cout<<"monitor_thread exits"<<std::endl;
        }
    }

void manager::stock_main_thread()
{
    if(parse_thread.joinable()){
        parse_thread.join();
    }
}

// LOG writing and suffocate all threads when fatal error occurs, exit type break loop
void manager::monitor()
{
    while(!status.stop_flag){
        std::unique_lock<std::mutex> lock(status.status_mutex);//lock up status
        status.cv_wake_monitor.wait(lock, [this ]{ return status.has_new_update||status.stop_flag;});//avoid "fake wake message" from task thread
        if(status.stop_flag) break;
        std::cout<<"Logging one message"<<std::endl;
        if(status.png_error_code==PNGErrorCode::SUCCESS&& status.io_error_code==IOErrorCode::SUCCESS) LOG_INFO(status);
        //else it will log error and stop all
        else{
            LOG_ERROR(status);
            status.stop_flag=true;
            return;
        }
        status.has_new_update=false;// reset
    }
}

void manager::load_file_rd(const char *fname)
{
    file_io.load_file_rd(fname, status);
    return;
}

void manager::launch_disk_thread()
{
    disk_thread= std::thread([this](){
        std::cout<<"disk_thread starts, id:"<< std::this_thread::get_id()<<std::endl;
        file_io.write_to_buffer(status);
    });
}

void manager::launch_parse_thread()
{
    parse_thread= std::thread([this](){
        std::cout<<"parse_thread starts, id:"<< std::this_thread::get_id()<<std::endl;
        verify_png();
        depack_data();
        de_comp();
        de_filter();
    });
}

void manager::launch_monitor_thread()
{   
    monitor_thread= std::thread([this](){
        std::cout<<"monitor_thread starts, id:"<< std::this_thread::get_id()<<std::endl;
        monitor();
    });
}

void manager::buffer_write()
{
    file_io.write_to_buffer(status);
    return;
}

void manager::copy_to_png_swap(size_t len){
    //read untill end
    while (!status.stop_flag)
    {
        bool success = file_io.copy_to_swap(len, png_parser.PNG_swap, status);
        if (success)
        {
            break;
        }
        //give out CPU resource
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return ;
}

void manager::verify_png(){
    copy_to_png_swap(PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH);
    png_parser.verify_png(status);
    return;
}

void manager::Print_png_info()
{
    png_parser.Print_png_info();
}

// size_t manager::get_next_chunk_len()
// {   
//     buffer_write(sizeof(uint32_t));
//     copy_to_png_swap(sizeof(uint32_t));
//     size_t next_chunk_length=png_parser.next_chunk_length(status);
//     buffer_write(next_chunk_length);
//     if(next_chunk_length==-1) { LOG_ERROR(status); return -1;}
//         LOG_INFO(status);
//         return next_chunk_length;
// }

void manager::depack_data()
{   
    //exit at IEND
    while(true){
        if(status.stop_flag) return;
        if(!file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status)) return;
        png_parser.get_next_chunk_length(status);
        if(!file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status)) return;
        png_parser.get_next_chunk_type(status);
        if(png_parser.fetch_next_chunk_type()==PNGChunkType::IEND) break;
        // PNG chunk length field stores only data bytes (excludes type and CRC).
        size_t remaining=png_parser.fetch_next_chunk_length();
        while(remaining>0)
        {   
            if(status.stop_flag) return;
            uint32_t step=file_io.copy_to_swap(std::min(remaining, (size_t)SWAP_SIZE), png_parser.PNG_swap, status);
            if(step==0){
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            png_parser.update_chunk_crc_with_swap();
            if(png_parser.fetch_next_chunk_type()==PNGChunkType::IDAT) png_parser.swap_copy_to_raw();
            remaining-=step;
        }//read the next chunk 
        //last four is CRC
        if (!file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status)) return;
        uint32_t net_crc;
        memcpy(&net_crc, png_parser.PNG_swap.swap_buffer, sizeof(uint32_t));
        uint32_t file_crc = net_to_host(net_crc);
        uint32_t calc_crc = png_parser.fetch_final_chunk_crc();
        if(file_crc!=calc_crc) {
            SET_ERROR(status, PNGErrorCode::INCORR_CRC, IOErrorCode::DEFAULT_ERROR,
                "CRC check failed in depack, type="
                + std::to_string(static_cast<uint32_t>(png_parser.fetch_next_chunk_type()))
                + ", len=" + std::to_string(png_parser.fetch_next_chunk_length())
                + ", file_crc=" + std::to_string(file_crc)
                + ", calc_crc=" + std::to_string(calc_crc));
            return;
        }
        png_parser.reset_chunk_status();
    }
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "PNG file depacked successfully, now "
        + std::to_string(png_parser.fetch_raw_data_length())+" bytes in raw_data to be de-comped and de-filtered");
    return;
}

void manager::de_comp()
{
    png_parser.de_comp(status);
}

void manager::de_filter()
{
    png_parser.de_filter(status);
}

void manager::rewrite_png(const char *fname, PNG &png_parser)
{   
    //get compressed data
    std::vector<uint8_t> compressed_data;
    std::vector<uint8_t> data;//what will be written into the file
    std::vector<uint8_t> chunk;//what will be added to the tail of data, need clear up per step
    std::vector<uint8_t> chunk_data;//what will be packed to be chunk, need clear up per step, cut from compressed_data
    png_parser.compress(compressed_data,status);
    chunk_data.clear();
    chunk.clear();

    //add IHDR
    png_parser.pack_chunk(PNGChunkType::IHDR, chunk_data, chunk, status);
    data.insert(data.end(), chunk.data(), chunk.data()+ chunk.size());
    chunk_data.clear();
    chunk.clear();

    //devide and pack IDAT
    constexpr size_t MAX_IDAT= 16384;
    size_t remaining= compressed_data.size();
    size_t origin= compressed_data.size();
    while(remaining>0){
        chunk_data.clear();
        chunk.clear();
        size_t next_chunk_size= std::min(MAX_IDAT, remaining);
        chunk_data.insert(chunk_data.end(), compressed_data.data()+(origin-remaining), 
        compressed_data.data()+(origin-remaining)+next_chunk_size);
        png_parser.pack_chunk(PNGChunkType::IDAT, chunk_data, chunk, status);
        data.insert(data.end(), chunk.data(), chunk.data()+ chunk.size());
        remaining-=next_chunk_size;
    }

    //add IEND
    png_parser.pack_chunk(PNGChunkType::IEND, chunk_data, chunk, status);
    data.insert(data.end(), chunk.data(), chunk.data()+ chunk.size());
    chunk_data.clear();
    chunk.clear();

    const char* output_name = (fname != nullptr) ? fname : "new_png.png";
    file_io.write_to_file(output_name, data, status);
}

Pixel manager::pixel_visit(size_t x, size_t y)
{   
    uint8_t* pixel_ptr=png_parser.get_pixel(x, y, status);
    uint8_t bytes_per_channel= png_parser.fetch_bytes_per_channel();
    uint8_t bytes_per_pixel= png_parser.fetch_bytes_per_pixel();
    return Pixel(pixel_ptr, bytes_per_pixel, bytes_per_channel, status);
}

void manager::get_pixelVec()
{   
    // In modifying mode, keep existing snapshot but still allow first-time load.
    if(if_fft_modifying && !pixelVector.empty()) return;
    pixelVector.clear();
    size_t h=png_parser.fetch_height();
    size_t w=png_parser.fetch_width();
    for(size_t i=0; i<h; i++){
        for(size_t j=0; j<w; j++){
            Pixel pixel=pixel_visit(j, i);
            Vec5 pixelvec={i, j, pixel.read(0), pixel.read(1), pixel.read(2)};
            pixelVector.push_back(pixelvec);
        }
    }
    return;
}

void manager::fft_for_channel(uint8_t channel_index)
{
    get_pixelVec();
    if(pixelVector.empty()) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR,
            "Pixel vector is empty, cannot perform FFT");
        return;
    }
    fft_set_up(png_parser.fetch_width(), png_parser.fetch_height(), pixelVector);
    fft_processor.forward(channel_index);
}
//u:-padW/2~padW/2 v:-padH/2~padH/2
void manager::get_fft_at_freq(int u, int v, std::pair<float, float> &fft_freq)
{   
    const size_t padW = fft_processor.fetch_padW();
    const size_t total = fft_processor.fetch_total();
    const size_t padH = total / padW;
    const int halfW = static_cast<int>(padW / 2);
    const int halfH = static_cast<int>(padH / 2);
    const float normalization = 1.0f / static_cast<float>(padH * padW);
    if (u <= -halfW || u > halfW || v > halfH || v <= -halfH) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, 
            "FFT frequency index out of range,got u="+std::to_string(u)+", v="+std::to_string(v)+
            ", but should be in range of u: (-"+std::to_string(halfW)+","+std::to_string(halfW)+
            "] and v: (-"+std::to_string(halfH)+","+std::to_string(halfH)+"]");
        return;
    }

    const size_t abs_v = static_cast<size_t>(std::abs(v));
    const size_t abs_u = static_cast<size_t>(std::abs(u));
    if (u == 0) {
        if (v == 0) {
            const size_t index = 0;
            fft_freq.first = fft_processor.complexData.realp[index] * normalization;
            fft_freq.second = 0.0f;
            return;
        }
        if (v == halfH) {
            const size_t index = abs_v * halfW;
            fft_freq.first = fft_processor.complexData.realp[index] * normalization;
            fft_freq.second = 0.0f;
            return;
        }
        const size_t index = abs_v * halfW;
        fft_freq.first = fft_processor.complexData.realp[index] * normalization;
        fft_freq.second = ((v > 0) ? fft_processor.complexData.imagp[index] : -fft_processor.complexData.imagp[index])
                        * normalization;
        return;
    }

    if (u == halfW) {
        if (v == 0) {
            const size_t index = 0;
            fft_freq.first = fft_processor.complexData.imagp[index] * normalization;
            fft_freq.second = 0.0f;
            return;
        }
        if (v == halfH) {
            const size_t index = abs_v * halfW;
            fft_freq.first = fft_processor.complexData.imagp[index] * normalization;
            fft_freq.second = 0.0f;
            return;
        }
        const size_t index = (padH / 2 + abs_v) * halfW;
        fft_freq.first = fft_processor.complexData.realp[index] * normalization;
        fft_freq.second = ((v > 0) ? fft_processor.complexData.imagp[index] : -fft_processor.complexData.imagp[index])
                        * normalization;
        return;
    }
    //commom way
    const size_t pos_index = abs_v * halfW + abs_u;
    const float real_part = (fft_processor.complexData.realp[pos_index]) * 0.5f * normalization;
    float imag_part = (fft_processor.complexData.imagp[pos_index]) * 0.5f * normalization;
    if (v < 0) imag_part = -imag_part;
    fft_freq.first = real_part;
    fft_freq.second = imag_part;
    return;
}

//accept frequency  u:-padW/2~padW/2 v:-padH/2~padH/2 (index=-1 required), or index when it >=0
void manager::edit_at_freq(int index ,int u, int v, float real_part, float imag_part)
{   
    if(index>=0){
        const size_t total = fft_processor.fetch_total();
        if(static_cast<size_t>(index)>=total){
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, 
                "FFT frequency index out of range, got index="+std::to_string(index)+
                ", but should be in range of [0,"+std::to_string(total)+")");
            return;
        }
        fft_processor.complexData.realp[index] = real_part;
        fft_processor.complexData.imagp[index] = imag_part;
        return;
    }
    else
    {
        const size_t padW = fft_processor.fetch_padW();
        const size_t total = fft_processor.fetch_total();
        const size_t padH = total / padW;
        const int halfW = static_cast<int>(padW / 2);
        const int halfH = static_cast<int>(padH / 2);
        const float normalization = 1.0f / static_cast<float>(padH * padW);
        if (u <= -halfW || u > halfW || v > halfH || v <= -halfH) {
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, 
                "FFT frequency index out of range,got u="+std::to_string(u)+", v="+std::to_string(v)+
                ", but should be in range of u: (-"+std::to_string(halfW)+","+std::to_string(halfW)+
                "] and v: (-"+std::to_string(halfH)+","+std::to_string(halfH)+"]");
            return;
        }

        const size_t abs_v = static_cast<size_t>(std::abs(v));
        const size_t abs_u = static_cast<size_t>(std::abs(u));
        if (u == 0) {
            if (v == 0) {
                //imagp can't be edited
                const size_t index = 0;
                fft_processor.complexData.realp[index] = real_part*normalization;
                return;
            }
            if (v == halfH) {
                //imagp can't be edited
                const size_t index = abs_v * halfW;
                fft_processor.complexData.realp[index] = real_part*normalization;
                return;
            }
            const size_t index = abs_v * halfW;
            fft_processor.complexData.realp[index] = real_part*normalization;
            fft_processor.complexData.imagp[index]= ((v > 0) ? imag_part : -imag_part) * normalization;
            return;
        }

        if (u == halfW) {
            if (v == 0) {
                const size_t index = 0;
                //realp can't be edited
                fft_processor.complexData.imagp[index] = imag_part * normalization;
                return;
            }
            if (v == halfH) {
                const size_t index = abs_v * halfW;
                //realp can't be edited
                fft_processor.complexData.imagp[index] = imag_part * normalization;
                return;
            }
            const size_t index = (padH / 2 + abs_v) * halfW;
            fft_processor.complexData.realp[index] = real_part * normalization;
            fft_processor.complexData.imagp[index]= ((v > 0) ? imag_part : -imag_part) * normalization;
            return;
        }
        //commom way
        const size_t pos_index = abs_v * halfW + abs_u;
        int abs_imag_part = abs(imag_part);
        fft_processor.complexData.realp[pos_index] = real_part * 2.0f * normalization;
        fft_processor.complexData.imagp[pos_index] = abs_imag_part * 2.0f * normalization;
        return;
    }
}

PNG manager::create_empty_png(const char *fname, size_t width, size_t height, PNG_BitDepth bit_depth, PNG_ColorType color_type)
{
    PNG temp_png;
    temp_png.set_empty(width, height, bit_depth, color_type, PNGCompressionMethod::DEFLATE, PNGFilterMethod::ADAPTIVE, PNGInterlaceMethod::NONE);
    return temp_png;
}


//called after fft_for_channel, data in complexData, save as a graph with intensity representing amplitude
void manager::generate_fft_graph(const char* fname)
{   
    const size_t padW = fft_processor.fetch_padW();
    const size_t padH = fft_processor.fetch_total() / padW;
    const int halfW = static_cast<int>(padW / 2);
    const int halfH = static_cast<int>(padH / 2);
    std::vector<float> display_values;
    display_values.reserve(padW * padH);
    float max_display = 0.0f;

    for(int i=-halfW+1; i<=halfW; i++){
        for(int j=-halfH+1; j<=halfH; j++){
            std::pair<float, float> freq;
            get_fft_at_freq(i, j, freq);
            const float mag = std::sqrt(freq.first * freq.first + freq.second * freq.second);
            const float display_val = std::log1p(mag);
            if (display_val > max_display) max_display = display_val;
            display_values.push_back(display_val);
        }
    }

    std::vector<uint8_t> pixelVector;
    pixelVector.reserve(display_values.size());
    if (max_display <= 0.0f) {
        pixelVector.assign(display_values.size(), 0);
    } 
    else {
        const float scale = 255.0f / max_display;
        for (float val : display_values) {
            pixelVector.push_back(static_cast<uint8_t>(std::min(255.0f, val * scale)));
        }
    }

    PNG graph_png= create_empty_png(fname, padW, padH, PNG_BitDepth::BIT_DEPTH_8, PNG_ColorType::GRAYSCALE);
    graph_png.load_pixel_data(status, pixelVector);
    rewrite_png(fname, graph_png);
}

void manager::modify_and_inverse(uint8_t channel_index)
{   
    fft_for_channel(channel_index);
    const size_t total = fft_processor.fetch_total();
    const float attenuation = 0.90f - 0.05f * static_cast<float>(channel_index);
    for(size_t i=1; i<total; i++){
        fft_processor.complexData.realp[i] *= attenuation;
        fft_processor.complexData.imagp[i] *= attenuation;
    }
    fft_processor.inverse_to_pixel(channel_index);
}

void manager::generate_from_vec5(const char *fname)
{
    std::vector<uint8_t> pixelData;
    png_parser.generate_pixelData_from_Vec5(pixelVector, pixelData, status);
    PNG new_png= create_empty_png(fname, get_width(), get_height(), static_cast<PNG_BitDepth>(png_parser.fetch_bit_depth())
                , static_cast<PNG_ColorType>(png_parser.fetch_color_type()));
    new_png.load_pixel_data(status, pixelData);
    rewrite_png(fname, new_png);
}
