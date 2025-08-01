#include <iostream>
#include <opencv2/opencv.hpp>

int main()
{
    std::cout << "OpenCV version: " << cv::getVersionString() << std::endl;

    cv::VideoCapture vcCap(0); // Open the default camera

    int64 i64StartTime = cv::getTickCount(); 
    int iFrameCount = 0;
    double dFPS = 0.0;
    
    if (!vcCap.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        return -1;
    }
    
    // Set camera properties
    vcCap.set(cv::CAP_PROP_FRAME_WIDTH, 640); // Set width
    vcCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480); // Set height
    vcCap.set(cv::CAP_PROP_FPS, 30); // Set frames per second
    
    std::cout << "Camera opened successfully." << std::endl;
    
    cv::Mat matFrame;
    bool bRunning = true;
    
    // Create window
    cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
    
    while(true)
    {
        if(bRunning){
            int64 i64CurrentTime = cv::getTickCount();
            // Capture frame-by-frame
            vcCap >> matFrame;

            iFrameCount++;
            
            if(iFrameCount % 30 == 0) {
            double elapsed = (i64CurrentTime - i64StartTime) / cv::getTickFrequency();
            dFPS = iFrameCount / elapsed;
            std::cout << "FPS: " << dFPS << std::endl;
            }
            
            if (matFrame.empty()) {
                std::cerr << "Error: Could not capture frame." << std::endl;
                break;
            }

            // Display the resulting frame
            cv::imshow("Camera Feed", matFrame);

            // Break the loop on 'q' key press
            if (cv::waitKey(1) >= 0) {
                bRunning = false;
            }
        } else {
            std::cout << "Exiting camera feed." << std::endl;
            break;
        }
    }

    
    vcCap.release(); // Release the camera
    cv::destroyAllWindows(); // Close all OpenCV windows
    return 0;
}