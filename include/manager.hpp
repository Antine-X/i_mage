#ifndef MANAGER_HPP
#define MANAGER_HPP

#include"io.hpp"
#include"png.hpp"


class manager
{
private:
    IO file_io;
    PNG png_parser;
    RunningStatus status;
public:
    void reset_status(){
        status={
            .error_info={0, nullptr, "Running Well"},
            .png_error_code=PNGErrorCode::SUCCESS,
            .io_error_code=IOErrorCode::SUCCESS
        };
    }
    manager() { reset_status(); }

    void load_file_rd(const char* fname);
    void close_file(){ file_io.close_file(); }

    bool buffer_write(size_t len);
    bool copy_to_png_swap(size_t len);

    void verify_png();

    size_t get_next_chunk_len();
    void depack_data();
};





#endif // MANAGER_HPP