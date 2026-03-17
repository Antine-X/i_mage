#include"manager.hpp"

int main(){
    get_start_time();
    manager png_manager;
    png_manager.load_file_rd("test.png");
    png_manager.launch_monitor_thread();
    png_manager.launch_disk_thread();
    png_manager.launch_parse_thread();

    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();
    png_manager.suffocate(); 
    return 0;
}