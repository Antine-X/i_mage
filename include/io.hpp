#ifndef IO_HPP
#define IO_HPP

#include"config.hpp"
#include <fstream>
#include <algorithm>


class IO
{
private:
    uint8_t buffer[BUFFER_SIZE];
    size_t used_size=0;
    size_t rd_offset=0;
    size_t wr_offset=0;
    const char* filename;
    std::ifstream infile;
    size_t file_length=0;
public:

    size_t calc_file_length();
    void load_file_rd(const char* fname, RunningStatus &status){ 
        filename = fname;
        infile.close();
        infile.open(fname, std::ios::in | std::ios::binary);
        if (infile.is_open()) {
            file_length = calc_file_length();
            SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "File opened successfully");
        } 
        else {
            file_length = 0;
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::FILE_NOT_FOUND, "Failed to open file");
        }
        return;
    }
    void close_file(){ 
        if(infile.is_open()) infile.close();
    }

    void reset_buffer(){
        used_size=0;
        rd_offset=0;
        wr_offset=0;
        file_length=0;
        memset(buffer, 0, sizeof(buffer));
    }
    IO() { 
        reset_buffer(); 
    }
    void write_to_buffer( size_t len, RunningStatus& status);
    void copy_to_swap(size_t len, Swap& swap, RunningStatus& status);

};


#endif // IO_HPP