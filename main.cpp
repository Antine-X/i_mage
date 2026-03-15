#include"manager.hpp"

int main(){

    manager png_manager;
    png_manager.load_file_rd("test.png");
    png_manager.verify_png();

    return 0;
}