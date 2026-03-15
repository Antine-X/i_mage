#include"io.hpp"

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

void IO::write_to_buffer(size_t len, RunningStatus &status)
{   

    if(!infile.is_open()){
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::FILE_NOT_FOUND, "Input file is not open");
        return;
    }
    if(used_size+len>BUFFER_SIZE) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::ERROR_OVERFLOW, "Not enough space in buffer to write data");
        return;
    }
    size_t first_part_len=std::min(len, BUFFER_SIZE-used_size);
    infile.read(reinterpret_cast<char*>(buffer+wr_offset), first_part_len);
    size_t second_part_len=len-first_part_len;
    if(second_part_len>0){
        infile.read(reinterpret_cast<char*>(buffer), second_part_len);
    }
    wr_offset=NEXT_N(wr_offset, len);
    used_size+=len;
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Data written to buffer successfully length:"+std::to_string(len));
    return;
}

void IO::copy_to_swap(size_t len, Swap& swap, RunningStatus &status)
{
    if(len>used_size) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::ERROR_INSUFFICIENT_DATA, "Not enough data in buffer to copy");
        return;
    }
    size_t first_part_len=std::min(len, BUFFER_SIZE-rd_offset);
    memcpy(swap.swap_buffer, buffer+rd_offset, first_part_len);
    size_t second_part_len=len-first_part_len;
    if(second_part_len>0){
        memcpy(swap.swap_buffer+first_part_len, buffer, second_part_len);
    }
    rd_offset=NEXT_N(rd_offset, len);
    used_size-=len;
    swap.datalen_in_buffer=len;
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Data copied to swap successfully length:"+std::to_string(len));
    return;
}

