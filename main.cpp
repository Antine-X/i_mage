#include"manager.hpp"

int main(){

    manager png_manager;
    png_manager.load_file_rd("test.png");
    if(png_manager.verify_png()){
        std::cout<<"PNG file is valid."<<std::endl;
    } else {
        std::cout<<"PNG file is invalid."<<std::endl;
    }
    png_manager.close_file();

    return 0;
}