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
    RunningStatus status;
    //task
    std::thread disk_thread;
    std::thread parse_thread;
    //monitor
    std::thread monitor_thread;
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
    void rewrite_png(const char* fname);

    Pixel pixel_visit(size_t x, size_t y);
    //a prepared vector is needed
    void get_pixelVec(std::vector<Vec5> &pixelVector);
};  





#endif // MANAGER_HPP