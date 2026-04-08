#ifndef MANAGER_HPP
#define MANAGER_HPP

#include"io.hpp"
#include"png.hpp"


class manager
{
private:
    std::ofstream log_file;
    IO file_io;
    PNG png_parser;
    FFT2D fft_processor;
    RunningStatus status;
    //task
    std::thread disk_thread;
    std::thread parse_thread;
    //monitor
    std::thread monitor_thread;
    std::vector<Vec5> pixelVector;//for fft, need to be updated when new picture is loaded
    bool if_fft_modifying=false;//stop reloading pixelVector when modifying
public:
    void suffocate();
    // wait for parse_thread to finish, aka, task finishedthen then main thread can exit
    void stock_main_thread();
    void monitor();

    manager() : log_file("i_mage.log") {}
    ~manager() {log_file.close();}
    void load_file_rd(const char* fname);
    void close_file(){ file_io.close_file(); }
    void launch_disk_thread();
    void launch_parse_thread();
    void launch_monitor_thread();
    void buffer_write();


    size_t get_width(){ return png_parser.fetch_width(); }
    size_t get_height(){ return png_parser.fetch_height(); }

    void copy_to_png_swap(size_t len);
    void verify_png();
    void Print_png_info();

    size_t get_next_chunk_len();
    void depack_data();
    void de_comp();
    void de_filter();
    void rewrite_png(const char* fname, PNG &png_parser);

    Pixel pixel_visit(size_t x, size_t y);
    //a prepared vector is needed
    void get_pixelVec();
    
    //fft 
    //must be called after get_pixelVec, data saved in complexData, never use this singly!!!
    void fft_set_up(int w, int h, std::vector<Vec5> &input){ fft_processor.set_up(w, h, input); }
    void fft_for_channel(uint8_t channel_index);
    //must be called after fft_for_channel
    void get_fft_at_freq(int u, int v, std::pair<float, float> &fft_freq);
    void edit_at_freq(int index, int u, int v,float real_part, float imag_part);
    PNG create_empty_png(const char* fname, size_t width, size_t height, 
        PNG_BitDepth bit_depth, PNG_ColorType color_type);
    void generate_fft_graph(const char* fname);
    //done on pixelVector, which may be out-defined
    void flag_modifying(const bool flag){ if_fft_modifying=flag; }//stop reloading pixelVector when modifying
    void modify_and_inverse(uint8_t channel_index);
    void generate_from_vec5(const char* fname);
};  





#endif // MANAGER_HPP