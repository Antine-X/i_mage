#include"manager.hpp"

int main(){

    manager png_manager;
    png_manager.load_file_rd("test.png");
    png_manager.launch_monitor_thread();
    png_manager.launch_disk_thread();
    png_manager.launch_parse_thread();

    png_manager.suffocate(); // 执行你写的那个“一键清场”函数
    png_manager.monitor_exit();
    std::cout << "Press Enter to suffocate and exit..." << std::endl;
    std::cin.get();
    return 0;
}