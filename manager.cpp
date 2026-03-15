#include "manager.hpp"

void manager::load_file_rd(const char *fname){
        file_io.load_file_rd(fname, status);
        if(status.io_error_code!=IOErrorCode::SUCCESS){
            LOG_ERROR(status);
            return;
        }
        LOG_INFO(status);
        return;
    }

bool manager::buffer_write(size_t len){
    file_io.write_to_buffer(len, status);
    if(status.io_error_code!=IOErrorCode::SUCCESS){
        LOG_ERROR(status);
        return false;
    }
    LOG_INFO(status);
    return true;
}

bool manager::copy_to_png_swap(size_t len){
    file_io.copy_to_swap(len, png_parser.PNG_swap, status);
    if(status.io_error_code!=IOErrorCode::SUCCESS){
        LOG_ERROR(status);
        return false;
    }
    LOG_INFO(status);
    return true;
}

void manager::verify_png(){
    if (!buffer_write(PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH)) return;
    if (!copy_to_png_swap(PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH)) return;
    if (!png_parser.verify_png(status)) LOG_ERROR(status);
    else LOG_INFO(status);
    
    return;
}
