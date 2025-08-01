#include <iostream>
#include <opencv2/opencv.hpp>

int main()
{
    std::cout << "OpenCV version: " << cv::getVersionString() << std::endl;
    return 0;
}