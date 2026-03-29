#ifndef IO_HPP
#define IO_HPP

#include"config.hpp"
#include <fstream>
#include <algorithm>


class IO
{
private:
    struct {
    uint8_t buffer[BUFFER_SIZE];
    size_t used_size=0;
    size_t rd_offset=0;
    size_t wr_offset=0;
    std::mutex buffer_mutex;
    std::condition_variable cv_disk_hang;
    std::condition_variable cv_buffer_full;
    bool swap_request=false;
    }RingBuffer;

    const char* filename;
    std::ifstream infile;
    size_t file_length=0;
    bool end_of_file=false;
public:
    void wake_up_all();
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


    IO() { }

    void write_to_buffer(RunningStatus& status);
    uint32_t copy_to_swap(size_t len, Swap& swap, RunningStatus& status);

    void write_to_file(const char* fname, const std::vector<uint8_t>& data, RunningStatus& status) {
        std::ofstream outfile(fname, std::ios::out | std::ios::binary);
        if (!outfile.is_open()) {
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::FILE_NOT_FOUND, "Failed to open output file for writing");
            return;
        }
        outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!outfile.good()) {
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, "Failed to write data to output file");
        } else {
            SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Data written to output file successfully");
        }
        outfile.close();
    }
};


#endif // IO_HPP