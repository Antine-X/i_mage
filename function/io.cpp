#include"io.hpp"

void IO::wake_up_all()
{
    RingBuffer.cv_buffer_full.notify_all();
    RingBuffer.cv_disk_hang.notify_all();
    return;
}

size_t IO::calc_file_length()
{
    if(!infile.is_open()){
        return 0;
    }
    auto current_pos=infile.tellg();
    infile.seekg(0, std::ios::end);
    size_t length=infile.tellg();
    infile.seekg(current_pos, std::ios::beg);
    return length;
}


constexpr size_t ONE_CHUNK_COPY=4096;
//read 4096 bytes once, exit_type wake up and kill 
void IO::write_to_buffer( RunningStatus &status)
{   
    while(!status.stop_flag){
    if(!infile.is_open()){
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::FILE_NOT_FOUND, "Input file is not open");
        return;
    }
    //lock the RingBuffer
    std::unique_lock<std::mutex> lock(RingBuffer.buffer_mutex);

    //swap priority
    if(RingBuffer.swap_request) RingBuffer.cv_disk_hang.wait(lock, [this, &status]{ return !RingBuffer.swap_request||status.stop_flag; });
    if(status.stop_flag) return;

    //copy!
    size_t to_copy=std::min(ONE_CHUNK_COPY,(size_t)(file_length-infile.tellg()));
    if(NEXT_N(RingBuffer.wr_offset,to_copy)>=RingBuffer.rd_offset) RingBuffer.cv_disk_hang.wait(lock, [this, &status]{ return !RingBuffer.swap_request||status.stop_flag; });
    if(status.stop_flag) return;
    if(to_copy==0){
        end_of_file=true;
        lock.unlock();
        RingBuffer.cv_disk_hang.notify_one();
        return;
    }
    size_t first_part_len=std::min(to_copy, BUFFER_SIZE-RingBuffer.wr_offset);
    infile.read(reinterpret_cast<char*>(RingBuffer.buffer+RingBuffer.wr_offset), first_part_len);
    size_t second_part_len=to_copy-first_part_len;
    if(second_part_len>0){
        infile.read(reinterpret_cast<char*>(RingBuffer.buffer), second_part_len);
    }
    //renew buffer offset and used count
    RingBuffer.wr_offset=NEXT_N(RingBuffer.wr_offset, to_copy);
    RingBuffer.used_size+=to_copy;

    //hang up if the buffer is full
    if(BUFFER_SIZE-RingBuffer.used_size<=ONE_CHUNK_COPY)
    {
    //    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Not enough space in buffer to write data, waiting……");//log and back
        RingBuffer.cv_buffer_full.wait(lock, [this,&status]{ return (BUFFER_SIZE-RingBuffer.used_size)>=ONE_CHUNK_COPY||status.stop_flag;} );
    }
    if(status.stop_flag) return;
    //when one loop ends, there will be timing for swap to flag request
    lock.unlock();

    //SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Data written to buffer successfully, length:"+std::to_string(to_copy));
    }
    return;
}
//need while(), exit_type break loop
uint32_t IO::copy_to_swap(size_t len, Swap& swap, RunningStatus &status)
{   
        if(status.stop_flag) return 0;
            std::unique_lock<std::mutex> lock(RingBuffer.buffer_mutex);
            RingBuffer.swap_request=true;
        //disk task go on if data not enough
        if(len>RingBuffer.used_size) {
            if(end_of_file){
                SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::ERROR_INSUFFICIENT_DATA,
                    "Unexpected EOF while reading required bytes from buffer");
                return 0;
            }
            SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, 
                "Not enough data in buffer to copy, handing over to buffer,write and will try again……");
            RingBuffer.swap_request=false;
            if(end_of_file) RingBuffer.cv_disk_hang.notify_one();
            return 0;
        }

        //copy
        len= std::min(len, SWAP_SIZE);
        size_t first_part_len=std::min(len, BUFFER_SIZE-RingBuffer.rd_offset);
        memcpy(swap.swap_buffer, RingBuffer.buffer+RingBuffer.rd_offset, first_part_len);
        size_t second_part_len=len-first_part_len;
        if(second_part_len>0){
            memcpy(swap.swap_buffer+first_part_len, RingBuffer.buffer, second_part_len);
        }
        RingBuffer.rd_offset=NEXT_N(RingBuffer.rd_offset, len);

        RingBuffer.used_size-=len;
        swap.datalen_in_buffer=len;

        //release the buffer, notify the buffer_write
        RingBuffer.swap_request=false;
        lock.unlock();
        if(!end_of_file){
            RingBuffer.cv_buffer_full.notify_one();
            RingBuffer.cv_disk_hang.notify_one();
        }
        if(len>0){
            //SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Data copied to swap successfully length:"+std::to_string(len));
        }
        return len;
}

