#ifndef PNG_HPP
#define PNG_HPP

#include"config.hpp"
#include<zlib.h>

/*
GRAYSCALE= rgb, a=255
TRUE_COLOR= rgb, a=255
INDEXED_COLOR= rgb, a=255
GRAYSCALE_WITH_ALPHA= rgb, a
TRUE_COLOR_WITH_ALPHA= rgba, a
*/
//to create pixel instance
const uint8_t MAX_CHANNEL_COUNT= 4;
class Pixel{
    uint8_t channel_count;
    uint16_t channels[MAX_CHANNEL_COUNT];
    uint8_t* data;
    RunningStatus& status;
public:
    
    uint8_t bytes_per_pixel;
    uint8_t bytes_per_channel;
    Pixel(uint8_t * data, uint8_t bytes_per_pixel, uint8_t bytes_per_channel, RunningStatus& status):
    data(data), bytes_per_pixel(bytes_per_pixel), bytes_per_channel(bytes_per_channel), status(status){
        channel_count=bytes_per_pixel/bytes_per_channel;
        uint8_t offset=0;
        while(offset<channel_count){
            if(data==nullptr){
                SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::ERROR_PTR_NULL, "Null pointer for pixel data");
                return;
            }
            uint8_t* cur_ptr= reinterpret_cast<uint8_t*>(channels+offset);
            memcpy(cur_ptr, data+offset*bytes_per_channel, bytes_per_channel);
            if(bytes_per_channel==2) channels[offset]= net_to_host(channels[offset]);
            offset++;
        }
    }
    uint8_t get_channel_count(){ return channel_count; }
    uint16_t read(uint8_t index);
    //write current data class member to the pixel_data
    void write(uint8_t index, uint16_t val);
};



class PNG{
private:
    uint32_t width;
    uint32_t height;
    PNG_BitDepth bit_depth;
    PNG_ColorType color_type;
    PNGCompressionMethod comp_method;
    PNGFilterMethod filter_method;
    PNGInterlaceMethod interlace_method;
    std::vector<uint8_t> raw_data;
    std::vector<uint8_t> filtered_data;
    std::vector<uint8_t> pixel_data;
    struct{
        size_t chunk_length{0};// 0 stands for un-read length
        PNGChunkType type{PNGChunkType::DEFAULT};// DEFAULT stands for un-read type
        uint32_t crc{0xFFFFFFFF};//you know it, crc_32
    }ChunkStatus;
    size_t bytes_per_channel;
    size_t bytes_per_pixel;


public:
    Swap PNG_swap;
    void reset(){
        width=0;
        height=0;
        bit_depth=PNG_BitDepth::BIT_DEFAULT;
        color_type=PNG_ColorType::COLOR_DEFAULT;
        comp_method=PNGCompressionMethod::DEFLATE;
        filter_method=PNGFilterMethod::ADAPTIVE;
        interlace_method=PNGInterlaceMethod::NONE;
        raw_data.clear();
        filtered_data.clear();
        pixel_data.clear();
    }
    PNG() { reset(); }
    void reset_chunk_status(){
        ChunkStatus.chunk_length=0;
        ChunkStatus.type=PNGChunkType::DEFAULT;
        ChunkStatus.crc=0xFFFFFFFF;
    }
    bool check_colorInfo();
    void verify_png(RunningStatus &status);
    void Print_png_info();
    //tool for depack

    //outie socket for manager
    uint32_t fetch_next_chunk_length(){ return ChunkStatus.chunk_length;}
    PNGChunkType fetch_next_chunk_type(){ return ChunkStatus.type;}
    uint32_t fetch_final_chunk_crc(){ return ~ChunkStatus.crc;}
    uint8_t fetch_bit_depth(){ return static_cast<uint8_t>(bit_depth);}
    PNG_ColorType fetch_color_type(){ return color_type;}
    uint8_t fetch_bytes_per_channel(){ return bytes_per_channel;}
    uint8_t fetch_bytes_per_pixel(){ return bytes_per_pixel;}
    void get_next_chunk_length(RunningStatus &status);
    size_t fetch_raw_data_length(){ return raw_data.size();}


    void get_next_chunk_type(RunningStatus &status);
    void update_chunk_crc_with_swap();
    //swap to rawdata
    void swap_copy_to_raw();
    // rawdata to filtered_data
    void de_comp(RunningStatus &status);
    uint8_t get_channel_count(PNG_ColorType color_type);
    // filtered_data to pixel_data
    void de_filter(RunningStatus &status);
    uint8_t byte_de_filter(uint8_t* data, size_t pos, size_t byte_width, uint8_t filter_type);
    uint8_t* get_pixel(size_t x, size_t y, RunningStatus &status);

    void Print_3();
};

#endif // PNG_HPP
