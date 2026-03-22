#ifndef CONFIG_HPP
#define CONFIG_HPP

#include<iostream>
#include<cstring>
#include<fstream>
#include<vector>
#include <atomic>
#include <mutex>
#include<thread>
#include <chrono>


inline auto& get_start_time() {
    static std::chrono::high_resolution_clock::time_point start = 
        std::chrono::high_resolution_clock::now();
    return start;
}

inline double get_elapsed_ms() {
    auto now = std::chrono::high_resolution_clock::now();
    auto dur = now - get_start_time();
    return std::chrono::duration<double, std::milli>(dur).count();
}

//include 1,2,4 bytes shift for big-endian to little-endian
#define net_to_host(x) (sizeof((x)) == 4 ? \
    ((x & 0xFF000000) >> 24) | \
    ((x & 0x00FF0000) >> 8)  | \
    ((x & 0x0000FF00) << 8)  | \
    ((x & 0x000000FF) << 24) : (sizeof((x)) == 2 ? \
        ((x & 0xFF00) >> 8) | \
        ((x & 0x00FF) << 8) : (x)))

enum class PNG_BitDepth : uint8_t{
    BIT_DEPTH_1=1,
    BIT_DEPTH_2=2,
    BIT_DEPTH_4=4,
    BIT_DEPTH_8=8,
    BIT_DEPTH_16=16,
    BIT_DEFAULT=255,
};

static PNG_BitDepth get_bit_depth(uint8_t value){
    switch(value){
        case 1: return PNG_BitDepth::BIT_DEPTH_1;
        case 2: return PNG_BitDepth::BIT_DEPTH_2;
        case 4: return PNG_BitDepth::BIT_DEPTH_4;
        case 8: return PNG_BitDepth::BIT_DEPTH_8;
        case 16: return PNG_BitDepth::BIT_DEPTH_16;
        default: return PNG_BitDepth::BIT_DEFAULT;
    }
}

enum class PNG_ColorType : uint8_t{
    GRAYSCALE=0,
    TRUE_COLOR=2,
    INDEXED_COLOR=3,
    GRAYSCALE_WITH_ALPHA=4,
    TRUE_COLOR_WITH_ALPHA=6,
    COLOR_DEFAULT=255,
};

static PNG_ColorType get_color_type(uint8_t value){
    switch(value){
        case 0: return PNG_ColorType::GRAYSCALE;
        case 2: return PNG_ColorType::TRUE_COLOR;
        case 3: return PNG_ColorType::INDEXED_COLOR;
        case 4: return PNG_ColorType::GRAYSCALE_WITH_ALPHA;
        case 6: return PNG_ColorType::TRUE_COLOR_WITH_ALPHA;
        default: return PNG_ColorType::COLOR_DEFAULT;
    }
}

enum class PNGCompressionMethod : uint8_t{
    DEFLATE=0
};

enum class PNGFilterMethod : uint8_t{
    ADAPTIVE=0
};

enum class PNGInterlaceMethod : uint8_t{
    NONE=0,
    ADAM7=1
};

enum class PNGChunkType : uint32_t{
    IHDR=0x49484452, // "IHDR"
    PLTE=0x504C5445, // "PLTE"
    IDAT=0x49444154, // "IDAT"
    IEND=0x49454E44,  // "IEND"
    DEFAULT=0x00000000
};


constexpr uint8_t PNG_SIGNATURE[8]={137, 80, 78, 71, 13, 10, 26, 10};
constexpr size_t PNG_SIGNATURE_LENGTH=8;
constexpr size_t CHUNK_COMPULSORY_LENGTH=12; // length(4) + type(4) + CRC(4)
constexpr size_t IHDR_DATA_LENGTH=13; // 4+4+1+1+1+1+1

enum class PNGErrorCode: uint8_t{
    SUCCESS=0,
    INVALID_SIGNATURE= 1,
    INVALID_IHDR_LENGTH= 2,
    INVALID_IHDR_TYPE= 3,
    INVALID_BIT_DEPTH= 4,
    INVALID_COLOR_TYPE= 5,
    INVALID_COMPRESSION_METHOD= 6,
    INVALID_FILTER_METHOD= 7,
    INSUFF_SWAP= 8,
    INVALID_CHUNK_TYPE= 9,
    INCORR_CRC=10,
    DEFAULT_ERROR=255,
};


enum class IOErrorCode: uint8_t{
    SUCCESS=0,
    FILE_NOT_FOUND=1,
    ERROR_PTR_NULL=2,
    ERROR_OVERFLOW=3,
    ERROR_INSUFFICIENT_DATA=4,
    DEFAULT_ERROR=255,
};


struct ErrorInfo{
    uint32_t error_line;
    const char* error_file;
    std::string error_message;
};

struct RunningStatus{
    bool has_new_update{false};
    ErrorInfo error_info;
    std:: mutex status_mutex;
    std::atomic<PNGErrorCode> png_error_code{PNGErrorCode::SUCCESS};
    std::atomic<IOErrorCode> io_error_code{IOErrorCode::SUCCESS};
    std::condition_variable cv_wake_monitor;
    std::atomic<bool> stop_flag{false};
    bool is_processed = true;
};


#define LOG_ERROR(status) do{\
    log_file <<"[" << std::fixed << std::setprecision(3) << get_elapsed_ms() << "ms] "\
    <<"[ERROR] "<<status.error_info.error_message<<" at "\
    <<status.error_info.error_file<<":"<<status.error_info.error_line<<std::endl\
    <<std::flush;\
    status.is_processed= true;\
} while(0)

#define LOG_INFO(status) do{\
    log_file <<"[" << std::fixed << std::setprecision(3) << get_elapsed_ms() << "ms] "\
    <<"[INFO] "<<status.error_info.error_message<<std::endl\
    <<std::flush;\
    status.is_processed= true;\
} while(0)

#define SET_ERROR(status, png_err, io_err, msg) do { \
    {\
    std::lock_guard<std::mutex> _lock(status.status_mutex); \
    status.png_error_code = png_err; \
    status.io_error_code = io_err; \
    status.error_info.error_line = __LINE__; \
    status.error_info.error_file = __FILE__; \
    status.error_info.error_message = msg; \
    status.has_new_update = true;  \
    status.is_processed= false; \
    status.cv_wake_monitor.notify_one();\
    }\
} while(0)

constexpr size_t SWAP_SIZE=512;
struct Swap{
    uint8_t swap_buffer[SWAP_SIZE];
    size_t datalen_in_buffer;
};

constexpr size_t BUFFER_SIZE=1024*24; // 24KB

#define NEXT_N(offset, n) ((offset+n)%BUFFER_SIZE) 

#endif // CONFIG_HPP