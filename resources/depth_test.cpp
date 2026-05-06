#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>

int main() {
    // 加载模型
    cv::dnn::Net net = cv::dnn::readNetFromONNX("model-small.onnx");
    if (net.empty()) {
        std::cerr << "模型加载失败" << std::endl;
        return -1;
    }
    
    // 读一张测试图
    cv::Mat img = cv::imread("resources/Halls.png");
    if (img.empty()) {
        std::cerr << "图片加载失败" << std::endl;
        return -1;
    }
    
    // 预处理：resize到256x256，归一化
    cv::Mat blob;
    cv::dnn::blobFromImage(img, blob, 1.0/255.0, 
                           cv::Size(256, 256), 
                           cv::Scalar(0.5, 0.5, 0.5), 
                           true, false);
    
    // 推理
    net.setInput(blob);
    cv::Mat output = net.forward();
    // 归一化到0-255然后保存为图片
    cv::Mat depth_vis;
    cv::normalize(output.reshape(1, 256), depth_vis, 0, 255, cv::NORM_MINMAX, CV_8U);
    cv::imwrite("output/depth_output.png", depth_vis);
    std::cout << "视差图已保存" << std::endl;

    // 输出shape确认
    std::cout << "输出shape: " << output.size << std::endl;
    
    return 0;
}
