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

    void load_file_rd(const char* fname){
        file_io.load_file_rd(fname, status);
        if(status.io_error_code!=IOErrorCode::SUCCESS){
            LOG_ERROR(status);
            return;
        }
        LOG_INFO(status);
        return;
    }

    void close_file(){
        file_io.close_file();
    }

    void verify_png(){
        file_io.copy_to_swap(IHDR_DATA_LENGTH+CHUNK_COMPULSORY_LENGTH+PNG_SIGNATURE_LENGTH, png_parser.PNG_swap, status);
        if(status.io_error_code!=IOErrorCode::SUCCESS){
            LOG_ERROR(status);
            return ;
        }
        LOG_INFO(status);
        bool val = png_parser.verify_png(status);
        LOG_ERROR(status);
        return;
    }

};





#endif // MANAGER_HPP