#ifndef PNG_HPP
#define PNG_HPP

#include"config.hpp"
#include<zlib.h>

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
    void get_next_chunk_length(RunningStatus &status);
    size_t fetch_raw_data_length(){ return raw_data.size();}


    void get_next_chunk_type(RunningStatus &status);
    void update_chunk_crc_with_swap();
    //swap to rawdata
    void swap_copy_to_raw();
    // rawdata to filtered_data
    void de_comp(RunningStatus &status);
    // filtered_data to pixel_data
    void de_filter();

    void depack(RunningStatus &status, bool & IsEnd, bool &type_readed, uint32_t &crc);
};

#endif // PNG_HPP
