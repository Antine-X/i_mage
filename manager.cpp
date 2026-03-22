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
        file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status);
        png_parser.get_next_chunk_length(status);
        file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status);
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
        file_io.copy_to_swap(sizeof(uint32_t), png_parser.PNG_swap, status);
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

    
