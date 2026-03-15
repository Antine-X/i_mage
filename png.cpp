#include"png.hpp"
//LSB first for crc32, for hardware send data in little-endian order

static uint32_t list[256];
static bool is_table_init=false;

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

void calc_crc32_step(uint8_t data, uint32_t &crc){
    if(!is_table_init) {
        crc32_make_list(list);
        is_table_init=true;
    }
    crc=(crc>>8)^list[((crc^data)&0xFF)];
}

uint32_t calc_crc32(const uint8_t* data, size_t length){
    uint32_t crc=0xFFFFFFFF;
    for(int i=0; i<length; i++){
        calc_crc32_step(data[i], crc);
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

//Verify PNG signature and IHDR chunk, and extract image information, need 33bytes in swap
bool PNG::verify_png(RunningStatus &status)
{   
    size_t offset=0;
    if(PNG_swap.datalen_in_buffer<PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH){
        SET_ERROR(status, PNGErrorCode::INSUFF_SWAP, IOErrorCode::DEFAULT_ERROR, "Not enough data for PNG signature and IHDR chunk, get: "+std::to_string(PNG_swap.datalen_in_buffer));
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
        SET_ERROR(status, PNGErrorCode::INCORR_CRC, IOErrorCode::DEFAULT_ERROR, "CRC check failed");
        return false;
    }
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "PNG file verified successfully");
    return true;
}



// need next 4 bytes in swap
size_t PNG::next_chunk_length(RunningStatus &status)
{   
    if(PNG_swap.datalen_in_buffer< sizeof(size_t)){
        SET_ERROR(status, PNGErrorCode::INSUFF_SWAP, IOErrorCode::DEFAULT_ERROR, "Insufficient length data in swap!");
        return 0;
    }
    size_t net_chunk_length;
    memcpy(&net_chunk_length, PNG_swap.swap_buffer, sizeof(size_t));
    size_t host_chunk_length= net_to_host(net_chunk_length);
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Next chunk is "+std::to_string(host_chunk_length)+"-byte-long");
    return host_chunk_length;
}


//1024 bytes more at a time
void Allocate_more_Invec(std::vector<uint8_t> &vec, size_t len)
{   
    if(vec.size()+len>vec.capacity()) vec.reserve(vec.capacity()+1024);
    else return;
}

//pushback one by one and calc the CRC
void PNG::swap_copy_to_raw(size_t offset)
{   
    size_t to_copy= PNG_swap.datalen_in_buffer-offset;
    memcpy(
        raw_data.data()+raw_data.size(),
        PNG_swap.swap_buffer+offset,
        to_copy
    );
    raw_data.resize(raw_data.size()+to_copy);
}


void PNG::de_comp()
{   

    return;
}

void PNG::de_filter()
{
    return;
}


//sure there will be enough data in swap, except the last 4 bytes, namely the CRC
void PNG::depack(RunningStatus &status, bool &IsEnd, bool &type_readed ,uint32_t &crc)
{   
    size_t offset=0;
    //calc the CRC flow
    for(int i=0;i<PNG_swap.datalen_in_buffer;i++){
        calc_crc32_step(PNG_swap.swap_buffer[i], crc);
    }
    if(memcmp(PNG_swap.swap_buffer+offset, reinterpret_cast<uint8_t*>(PNGChunkType::IEND), sizeof(uint32_t))==0){
        IsEnd=true;
        return;
    }
    //read type-bytes in swap if not
    else if(!type_readed){
        uint32_t type;
        memcpy(&type, PNG_swap.swap_buffer+offset, sizeof(uint32_t));
        type_readed=true;
        if(memcmp(&type, reinterpret_cast<uint8_t*>(PNGChunkType::IDAT), sizeof(uint32_t))!=0){
            SET_ERROR(status,PNGErrorCode::INVALID_CHUNK_TYPE, IOErrorCode::DEFAULT_ERROR, "Unknown Chunk Type");
            LOG_ERROR(status);
            return;
        }
    }
    Allocate_more_Invec(raw_data, PNG_swap.datalen_in_buffer);
    swap_copy_to_raw(offset);
    de_comp();
    de_filter();
}
