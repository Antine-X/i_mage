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
    void monitor();

    manager() : log_file("i_mage.log") {}
    ~manager() {log_file.close();}
    void load_file_rd(const char* fname);
    void close_file(){ file_io.close_file(); }

    void launch_disk_thread();
    void launch_parse_thread();
    void launch_monitor_thread();
    void buffer_write();

    void copy_to_png_swap(size_t len);
    void verify_png();
    void Print_png_info();

    size_t get_next_chunk_len();
    void depack_data();
};





#endif // MANAGER_HPP