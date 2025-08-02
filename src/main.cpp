#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

std::mutex g_frameMutex;
std::deque<cv::Mat> g_frameQueue;
std::atomic<bool> g_bRunning(true);

void fnSendFrame(int nSock, const std::vector<uchar>& vecFrameData, const sockaddr_in& saTargetAddr) {
    if (nSock < 0) return;

    sendto(nSock, reinterpret_cast<const char*>(vecFrameData.data()), vecFrameData.size(), 0,
           reinterpret_cast<const sockaddr*>(&saTargetAddr), sizeof(saTargetAddr));
}

int main()
{
    std::cout << "OpenCV version: " << cv::getVersionString() << std::endl;

    cv::VideoCapture vcCap(0); 

    int64 i64StartTime = cv::getTickCount(); 
    int iFrameCount = 0;
    double dFPS = 0.0;
    
    std::vector<uchar> vecCompressedFrame;
    std::deque<std::vector<uchar>> stkFrameBuffer;
    int nStackSize = 10;

    if (!vcCap.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        return -1;
    }

    
    vcCap.set(cv::CAP_PROP_FRAME_WIDTH, 640); 
    vcCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480); 
    vcCap.set(cv::CAP_PROP_FPS, 30); 
    
    std::cout << "Camera opened successfully." << std::endl;

    cv::Mat matFrame;
    bool bRunning = true;
    
    cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
    
    while(true)
    {
        if(bRunning){
            int64 i64CurrentTime = cv::getTickCount();

            vcCap >> matFrame;

            size_t uOriginalSize = matFrame.total() * matFrame.elemSize();

            int64 i64CompressionStartTime = cv::getTickCount();
            cv::imencode(".jpg", matFrame, vecCompressedFrame, std::vector<int>{cv::IMWRITE_JPEG_QUALITY, 90});
            stkFrameBuffer.push_back(vecCompressedFrame);
            if (stkFrameBuffer.size() > nStackSize) {
                stkFrameBuffer.pop_front();
            }
            int64 i64CompressionEndTime = cv::getTickCount();

            size_t uCompressedSize = vecCompressedFrame.size();
            
            double dCompressionTime = (i64CompressionEndTime - i64CompressionStartTime) / cv::getTickFrequency();
           
            iFrameCount++;

            if(iFrameCount % 30 == 0) {
            
            double dElapsed = (i64CurrentTime - i64StartTime) / cv::getTickFrequency();
            dFPS = iFrameCount / dElapsed;
            
            size_t uTotalBufferSize = 0;
            for (const auto& frame : stkFrameBuffer) {
                uTotalBufferSize += frame.size();
            }

            std::cout << "FPS: " << dFPS << std::endl;
            std::cout << "Original size: " << uOriginalSize / 1024 << " KB" << std::endl;
            std::cout << "Compressed size: " << uCompressedSize / 1024 << " KB" << std::endl;
            std::cout << "Compression time: " << dCompressionTime << " seconds" << std::endl;
            std::cout << "Total buffer size: " << uTotalBufferSize / 1024 << " KB" << std::endl;
            }
            
            if (matFrame.empty()) {
                std::cerr << "Error: Could not capture frame." << std::endl;
                break;
            }

            cv::imshow("Camera Feed", matFrame);

            char key = cv::waitKey(1) & 0xFF;
            if (key == 'q') {
                bRunning = false;
            }
        } else {
            std::cout << "Exiting camera feed." << std::endl;
            break;
        }
    }

    
    vcCap.release();
    cv::destroyAllWindows(); 
    return 0;
}