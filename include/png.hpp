#ifndef PNG_HPP
#define PNG_HPP

#include"config.hpp"
#include<crc32intrin.h>

class PNG{
private:
    uint32_t width;
    uint32_t height;
    PNG_BitDepth bit_depth;
    PNG_ColorType color_type;
    PNGCompressionMethod comp_method;
    PNGFilterMethod filter_method;
    PNGInterlaceMethod interlace_method;
public:
    void reset(){
        width=0;
        height=0;
        bit_depth=PNG_BitDepth::BIT_DEFAULT;
        color_type=PNG_ColorType::COLOR_DEFAULT;
        comp_method=PNGCompressionMethod::DEFLATE;
        filter_method=PNGFilterMethod::ADAPTIVE;
        interlace_method=PNGInterlaceMethod::NONE;
    }
    PNG() { reset(); }
    Swap PNG_swap;
    bool check_colorInfo();
    bool verify_png(RunningStatus &status);
};

#endif // PNG_HPP
