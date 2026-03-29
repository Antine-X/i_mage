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
    size_t x=100, y=100;

    Pixel pixel=png_manager.pixel_visit(x,y);
    std::cout<<"Pixel at ("<<x<<","<<y<<"):"<<std::endl;
    for(uint8_t i=0; i<pixel.get_channel_count(); i++){
        std::cout<<"Channel "<<static_cast<int>(i)<<": "<<pixel.read(i)<<std::endl;
    } 
    for(int i=0; i<50;i++){
        for(int j=0; j<50; j++){
            Pixel pixel=png_manager.pixel_visit(i,j);
            pixel.write(0,1);
            pixel.write(1,1);
            pixel.write(2,1);
        }
    }
    png_manager.rewrite_png("new.png");
    png_manager.suffocate(); 
    return 0;
}