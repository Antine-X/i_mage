#include"png.hpp"
//LSB first for crc32, for hardware send data in little-endian order
static void crc32_make_list(uint32_t crc_list[256]){
    for(uint32_t i=0; i<256; ++i){
        uint32_t crc=i;
        for(int j=0; j<8; ++j){
            if(crc&1) crc=(crc>>1)^0xEDB88320;
            else crc>>=1;
        }
        crc_list[i]=crc;
    }
}

uint32_t calc_crc32(const uint8_t* data, size_t length){
    uint32_t crc=0xFFFFFFFF;
    static uint32_t list[256];
    static bool is_table_init=false;
    if(!is_table_init) {
        crc32_make_list(list);
        is_table_init=true;
    }
    for(int i=0; i<length; i++){
        crc=(crc>>8)^list[((crc^data[i])&0xFF)];
    }
    return ~crc;
}

//Color type and bit depth combinations validation 
bool PNG::check_colorInfo()
{
    switch (color_type)
    {
    case PNG_ColorType::GRAYSCALE:
        return bit_depth==PNG_BitDepth::BIT_DEPTH_1 || 
                bit_depth==PNG_BitDepth::BIT_DEPTH_2 || 
                bit_depth==PNG_BitDepth::BIT_DEPTH_4 || 
                bit_depth==PNG_BitDepth::BIT_DEPTH_8 || 
                bit_depth==PNG_BitDepth::BIT_DEPTH_16;
    case PNG_ColorType::TRUE_COLOR:
    case PNG_ColorType::GRAYSCALE_WITH_ALPHA:
    case PNG_ColorType::TRUE_COLOR_WITH_ALPHA:
        return bit_depth==PNG_BitDepth::BIT_DEPTH_8 ||
                bit_depth==PNG_BitDepth::BIT_DEPTH_16;
    case PNG_ColorType::INDEXED_COLOR:
        return bit_depth==PNG_BitDepth::BIT_DEPTH_1 ||
                bit_depth==PNG_BitDepth::BIT_DEPTH_2 ||
                bit_depth==PNG_BitDepth::BIT_DEPTH_4 ||
                bit_depth==PNG_BitDepth::BIT_DEPTH_8;
        break;
    default:
        return false;
        break;
    }
}

//Verify PNG signature and IHDR chunk, and extract image information
bool PNG::verify_png(RunningStatus &status)
{   
    size_t offset=0;
    if(PNG_swap.datalen_in_buffer<PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH){
        SET_ERROR(status, PNGErrorCode::INVALID_IHDR_LENGTH, IOErrorCode::DEFAULT_ERROR, "Not enough data for PNG signature and IHDR chunk, get: "+std::to_string(PNG_swap.datalen_in_buffer));
        return false;
    }
    //check signature
    if(memcmp(PNG_swap.swap_buffer+offset, PNG_SIGNATURE, PNG_SIGNATURE_LENGTH)!=0) {
        char temp_signature[PNG_SIGNATURE_LENGTH];
        memcpy(temp_signature, PNG_swap.swap_buffer+offset, PNG_SIGNATURE_LENGTH);
        SET_ERROR(status, PNGErrorCode::INVALID_SIGNATURE, IOErrorCode::DEFAULT_ERROR, "Invalid PNG signature");
        return false;
    }
    offset+=PNG_SIGNATURE_LENGTH;

    //check chunk lenth
    uint32_t len;
    memcpy(&len, PNG_swap.swap_buffer+offset, sizeof(uint32_t));
    offset+=sizeof(uint32_t);
    uint32_t IHDR_len=net_to_host(len);
    if(IHDR_len!=IHDR_DATA_LENGTH) {
        SET_ERROR(status, PNGErrorCode::INVALID_IHDR_LENGTH, IOErrorCode::DEFAULT_ERROR, "Invalid IHDR chunk length");
        return false;
    }

    //check IHDR-type
    if(memcmp(PNG_swap.swap_buffer+offset, "IHDR", 4)!=0) {std::cout<<"[ERROR] not valid IHDR(type)"<<std::endl; return false; }
    offset+=4;

    //get IHDR-width
    uint32_t be_width;
    memcpy(&be_width, PNG_swap.swap_buffer+offset, sizeof(uint32_t));
    offset+=sizeof(uint32_t);
    width=net_to_host(be_width);

    //get IHDR-height
    uint32_t be_height;
    memcpy(&be_height, PNG_swap.swap_buffer+offset, sizeof(uint32_t));
    offset+=sizeof(uint32_t);
    height=net_to_host(be_height);

    //get IHDR-Bit depth
    bit_depth=get_bit_depth(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);

    //get IHDR-color type
    color_type=get_color_type(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);

    if(!check_colorInfo()) {
        SET_ERROR(status, PNGErrorCode::INVALID_COLOR_TYPE, IOErrorCode::DEFAULT_ERROR, "Invalid color type and Bit depth combination");
        return false;
    }
    //get IHDR-compression method
    PNGCompressionMethod comp_m;
    comp_m=static_cast<PNGCompressionMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(static_cast<uint8_t>(comp_m)==0) comp_method=comp_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_COMPRESSION_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid compression method"); return false;}
    
    //get IHDR-filter method
    PNGFilterMethod filter_m;
    filter_m=static_cast<PNGFilterMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(static_cast<uint8_t>(filter_m)==0) filter_method=filter_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_FILTER_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid filter method"); return false;}
    
    //get IHDR-interlace method
    PNGInterlaceMethod interl_m;
    interl_m=static_cast<PNGInterlaceMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(interl_m==PNGInterlaceMethod::NONE || interl_m==PNGInterlaceMethod::ADAM7) interlace_method=interl_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_FILTER_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid interlace method"); return false;}
    
    //check CRC
    uint32_t be_crc;
    memcpy(&be_crc, PNG_swap.swap_buffer + offset, sizeof(be_crc));
    uint32_t file_crc = net_to_host(be_crc);
    //begin to calculate crc from chunk type, chunk data, but not include length and crc field itself(ignore signature-4 bytes and length field-4 bytes)
    if (file_crc != calc_crc32(PNG_swap.swap_buffer+PNG_SIGNATURE_LENGTH+4, CHUNK_COMPULSORY_LENGTH-8+IHDR_DATA_LENGTH)) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, "CRC check failed");
        return false;
    }
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "PNG file verified successfully");
    return true;
}