#include"manager.hpp"

int main(){
    get_start_time();
    manager png_manager;
    png_manager.load_file_rd("test.png");
    png_manager.launch_monitor_thread();
    png_manager.launch_disk_thread();
    png_manager.launch_parse_thread();
    
    png_manager.stock_main_thread();

    png_manager.Print_png_info();
    size_t x=0, y=0;
    Pixel pixel=png_manager.pixel_visit(x,y);
    std::cout<<"Pixel at ("<<x<<","<<y<<"):"<<std::endl;
    for(uint8_t i=0; i<pixel.get_channel_count(); i++){
        std::cout<<"Channel "<<static_cast<int>(i)<<": "<<pixel.read(i)<<std::endl;
    }   
    png_manager.suffocate(); 
    return 0;
}