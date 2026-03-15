#ifndef PNG_HPP
#define PNG_HPP

#include"config.hpp"

class PNG{
private:
    uint32_t width;
    uint32_t height;
    PNG_BitDepth bit_depth;
    PNG_ColorType color_type;
    PNGCompressionMethod comp_method;
    PNGFilterMethod filter_method;
    PNGInterlaceMethod interlace_method;
    std::vector<uint8_t> image_data;
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
        image_data.clear();
    }
    PNG() { reset(); }
    bool check_colorInfo();
    bool verify_png(RunningStatus &status);
};

#endif // PNG_HPP
