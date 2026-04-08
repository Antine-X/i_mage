#include"manager.hpp"

int main(){
    get_start_time();
    manager png_manager;
    png_manager.load_file_rd("cloud.png");
    png_manager.launch_monitor_thread();
    png_manager.launch_disk_thread();
    png_manager.launch_parse_thread();
    
    png_manager.stock_main_thread();

    png_manager.Print_png_info();

    // Pixel pixel=png_manager.pixel_visit(x,y);
    // std::cout<<"Pixel at ("<<x<<","<<y<<"):"<<std::endl;
    // for(uint8_t i=0; i<pixel.get_channel_count(); i++){
    //     std::cout<<"Channel "<<static_cast<int>(i)<<": "<<pixel.read(i)<<std::endl;
    // } 
    // for(int i=0; i<png_manager.get_width();i++){
    //     for(int j=0; j<png_manager.get_height(); j++){
    //         Pixel pixel=png_manager.pixel_visit(i,j);
    //         //uint8_t val =(pixel.read(0)+pixel.read(1)+pixel.read(2))/3;
    //         uint8_t b= pixel.read(2);
    //         pixel.write(0,b);
    //         pixel.write(1,b);
    //         pixel.write(2,b);
    //     }
    // }
    // png_manager.rewrite_png("new.png");
    
    png_manager.flag_modifying(true);
    png_manager.modify_and_inverse(0);
    png_manager.generate_from_vec5("modified.png_b.png");
    png_manager.modify_and_inverse(1);
    png_manager.generate_from_vec5("modified.png_g.png");
    png_manager.modify_and_inverse(2);
    png_manager.generate_from_vec5("modified.png_r.png");
    png_manager.flag_modifying(false);
    png_manager.suffocate();
    return 0;
}


