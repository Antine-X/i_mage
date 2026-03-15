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
    bool check_colorInfo();
    bool verify_png(RunningStatus &status);

    //tool for depack

    size_t next_chunk_length(RunningStatus &status);
    //swap to rawdata
    void swap_copy_to_raw(size_t offset);
    // rawdata to filtered_data
    void de_comp();
    // filtered_data to pixel_data
    void de_filter();

    void depack(RunningStatus &status, bool & IsEnd, bool &type_readed, uint32_t &crc);
};

#endif // PNG_HPP
