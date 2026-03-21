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

void calc_crc32_step(const uint8_t* data, size_t length, uint32_t &crc){
    if(!is_table_init) {
        crc32_make_list(list);
        is_table_init=true;
    }
    for(size_t i=0; i<length; i++){
        crc=(crc>>8)^list[((crc^data[i])&0xFF)];
    }
}

//for IHDR only
uint32_t calc_crc32(const uint8_t* data, size_t length){
    uint32_t crc=0xFFFFFFFF;
    calc_crc32_step(const_cast<uint8_t*>(data), length, crc);
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
void PNG::verify_png(RunningStatus &status)
{   
    size_t offset=0;
    if(PNG_swap.datalen_in_buffer<PNG_SIGNATURE_LENGTH+CHUNK_COMPULSORY_LENGTH+IHDR_DATA_LENGTH){
        SET_ERROR(status, PNGErrorCode::INSUFF_SWAP, IOErrorCode::DEFAULT_ERROR, "Not enough data for PNG signature and IHDR chunk, get: "+std::to_string(PNG_swap.datalen_in_buffer));
        return;
    }
    //check signature
    if(memcmp(PNG_swap.swap_buffer+offset, PNG_SIGNATURE, PNG_SIGNATURE_LENGTH)!=0) {
        char temp_signature[PNG_SIGNATURE_LENGTH];
        memcpy(temp_signature, PNG_swap.swap_buffer+offset, PNG_SIGNATURE_LENGTH);
        SET_ERROR(status, PNGErrorCode::INVALID_SIGNATURE, IOErrorCode::DEFAULT_ERROR, "Invalid PNG signature");
        return ;
    }
    offset+=PNG_SIGNATURE_LENGTH;

    //check chunk lenth
    uint32_t len;
    memcpy(&len, PNG_swap.swap_buffer+offset, sizeof(uint32_t));
    offset+=sizeof(uint32_t);
    uint32_t IHDR_len=net_to_host(len);
    if(IHDR_len!=IHDR_DATA_LENGTH) {
        SET_ERROR(status, PNGErrorCode::INVALID_IHDR_LENGTH, IOErrorCode::DEFAULT_ERROR, "Invalid IHDR chunk length");
        return;
    }

    //check IHDR-type
    if(memcmp(PNG_swap.swap_buffer+offset, "IHDR", 4)!=0) {std::cout<<"[ERROR] not valid IHDR(type)"<<std::endl; return; }
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
        return ;
    }
    //get IHDR-compression method
    PNGCompressionMethod comp_m;
    comp_m=static_cast<PNGCompressionMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(static_cast<uint8_t>(comp_m)==0) comp_method=comp_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_COMPRESSION_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid compression method"); return ;}
    
    //get IHDR-filter method
    PNGFilterMethod filter_m;
    filter_m=static_cast<PNGFilterMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(static_cast<uint8_t>(filter_m)==0) filter_method=filter_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_FILTER_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid filter method"); return ;}
    
    //get IHDR-interlace method
    PNGInterlaceMethod interl_m;
    interl_m=static_cast<PNGInterlaceMethod>(*(PNG_swap.swap_buffer+offset));
    offset+=sizeof(uint8_t);
    if(interl_m==PNGInterlaceMethod::NONE || interl_m==PNGInterlaceMethod::ADAM7) interlace_method=interl_m;
    else{ SET_ERROR(status, PNGErrorCode::INVALID_FILTER_METHOD, IOErrorCode::DEFAULT_ERROR, "Invalid interlace method"); return ;}
    
    //check CRC
    uint32_t be_crc;
    memcpy(&be_crc, PNG_swap.swap_buffer + offset, sizeof(be_crc));
    uint32_t file_crc = net_to_host(be_crc);
    //begin to calculate crc from chunk type, chunk data, but not include length and crc field itself(ignore signature-4 bytes and length field-4 bytes)
    if (file_crc != calc_crc32(PNG_swap.swap_buffer+PNG_SIGNATURE_LENGTH+4, CHUNK_COMPULSORY_LENGTH-8+IHDR_DATA_LENGTH)) {
        SET_ERROR(status, PNGErrorCode::INCORR_CRC, IOErrorCode::DEFAULT_ERROR, "CRC check failed");
        return ;
    }
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "PNG file verified successfully");
    return;
}

void PNG::Print_png_info()
{
    std::cout<<"PNG Image Information:"<<std::endl;
    std::cout<<"Width: "<<width<<std::endl;
    std::cout<<"Height: "<<height<<std::endl;
    std::cout<<"Bit Depth: "<<static_cast<int>(bit_depth)<<std::endl;
    switch(color_type){
        case PNG_ColorType::GRAYSCALE: std::cout<<"Color Type: Grayscale"<<std::endl; break;
        case PNG_ColorType::TRUE_COLOR: std::cout<<"Color Type: True Color"<<std::endl; break;
        case PNG_ColorType::INDEXED_COLOR: std::cout<<"Color Type: Indexed Color"<<std::endl; break;
        case PNG_ColorType::GRAYSCALE_WITH_ALPHA: std::cout<<"Color Type: Grayscale with Alpha"<<std::endl; break;
        case PNG_ColorType::TRUE_COLOR_WITH_ALPHA: std::cout<<"Color Type: True Color with Alpha"<<std::endl; break;
        default: std::cout<<"Color Type: Unknown"<<std::endl; break;
    }
    std::cout<<"Compression Method: "<<(comp_method==PNGCompressionMethod::DEFLATE ? "Deflate" : "Unknown")<<std::endl;
    std::cout<<"Filter Method: "<<(filter_method==PNGFilterMethod::ADAPTIVE ? "Adaptive" : "Unknown")<<std::endl;
    std::cout<<"Interlace Method: "<<(interlace_method==PNGInterlaceMethod::NONE ? "None" : (interlace_method==PNGInterlaceMethod::ADAM7 ? "Adam7" : "Unknown"))<<std::endl;
}

// need next 4 bytes in swap
void PNG::get_next_chunk_length(RunningStatus &status)
{   
    if(PNG_swap.datalen_in_buffer< sizeof(uint32_t)){
        SET_ERROR(status, PNGErrorCode::INSUFF_SWAP, IOErrorCode::DEFAULT_ERROR, "Insufficient length data in swap!");
        return;
    }
    uint32_t net_chunk_length;
    memcpy(&net_chunk_length, PNG_swap.swap_buffer, sizeof(uint32_t));
    uint32_t host_chunk_length= net_to_host(net_chunk_length);
    SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Next chunk is "+std::to_string(host_chunk_length)+"-byte-long");
    ChunkStatus.chunk_length=host_chunk_length;
    return;
}

//notice: crc is calculated on chunk type and chunk data
void PNG::get_next_chunk_type(RunningStatus &status)
{
    if(PNG_swap.datalen_in_buffer<sizeof(uint32_t)){
        SET_ERROR(status, PNGErrorCode::INSUFF_SWAP, IOErrorCode::DEFAULT_ERROR, "Insufficient type data in swap!");
        return;
    }
    uint32_t net_chunk_type;
    memcpy(&net_chunk_type, PNG_swap.swap_buffer, sizeof(uint32_t));
    calc_crc32_step(PNG_swap.swap_buffer, sizeof(uint32_t), ChunkStatus.crc);
    uint32_t host_chunk_type= net_to_host(net_chunk_type);
    ChunkStatus.type=static_cast<PNGChunkType>(host_chunk_type);
    return;
}

void PNG::update_chunk_crc_with_swap()
{
    calc_crc32_step(PNG_swap.swap_buffer, PNG_swap.datalen_in_buffer, ChunkStatus.crc);
}       



//swap to raw and calc crc
void PNG::swap_copy_to_raw()
{   
    raw_data.insert(raw_data.end(), PNG_swap.swap_buffer, PNG_swap.swap_buffer+PNG_swap.datalen_in_buffer);
}




 void PNG::de_comp(RunningStatus &status)
 {   
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = raw_data.size();
    strm.next_in = raw_data.data();
    if (inflateInit(&strm) != Z_OK) {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, "Failed to initialize zlib for decompression");
        return;
    }
    constexpr size_t CHUNK_SIZE = 1024;
    std::vector<uint8_t> out_buffer(CHUNK_SIZE);
    int ret;
    //chunk_size is an arbitrary step length, and different from Chunk size in PNG
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = out_buffer.data();
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, "Decompression error, code: " + std::to_string(ret));
            return;
        }
        size_t have = CHUNK_SIZE - strm.avail_out;
        filtered_data.insert(filtered_data.end(), out_buffer.data(), out_buffer.data() + have);
    } while (ret != Z_STREAM_END||status.stop_flag);
    inflateEnd(&strm);
    if (ret == Z_STREAM_END) {
        SET_ERROR(status, PNGErrorCode::SUCCESS, IOErrorCode::SUCCESS, "Decompression completed successfully, now " + std::to_string(filtered_data.size()) + " bytes in filtered_data");
    } else {
        SET_ERROR(status, PNGErrorCode::DEFAULT_ERROR, IOErrorCode::DEFAULT_ERROR, "Decompression ended with unexpected status, code: " + std::to_string(ret));
    }
 }

// void PNG::de_filter()
// {
//     return;
// }

