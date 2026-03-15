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

size_t manager::get_next_chunk_len()
{   
    buffer_write(sizeof(uint32_t));
    copy_to_png_swap(sizeof(uint32_t));
    size_t next_chunk_length=png_parser.next_chunk_length(status);
    buffer_write(next_chunk_length);
    if(next_chunk_length==-1) { LOG_ERROR(status); return -1;}
        LOG_INFO(status);
        return next_chunk_length;
}

void manager::depack_data()
{
    bool IsPngEnds=false;
    while(!IsPngEnds){
        size_t rest_length= get_next_chunk_len();
        if(rest_length==-1) return;
        bool type_readed=false;
        uint32_t crc= 0xFFFFFFFF;
        while(rest_length>4){
            size_t to_copy=rest_length>SWAP_SIZE ? SWAP_SIZE:rest_length;
            copy_to_png_swap(to_copy);//fetch data, buffer to swap
            png_parser.depack(status, IsPngEnds, type_readed ,crc);//depack flow
            rest_length-=to_copy;
        }
        Swap crc_get;
        file_io.copy_to_swap(sizeof(uint32_t), crc_get, status);
        if(status.io_error_code!=IOErrorCode::SUCCESS){
            LOG_ERROR(status);
            return;
        }
        else LOG_INFO(status);
        if(memcmp(crc_get.swap_buffer, &crc, sizeof(uint32_t))!=0){ 
            SET_ERROR(status, PNGErrorCode::INCORR_CRC, IOErrorCode::DEFAULT_ERROR, "CRC check failed");
            LOG_ERROR(status);
        }
    }
}
