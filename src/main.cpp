#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <thread>
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

void SendFrame(int nSock, const std::vector<uchar>& vecFrameData, const sockaddr_in& saTargetAddr) {
    if (nSock < 0) return;

    sendto(nSock, reinterpret_cast<const char*>(vecFrameData.data()), vecFrameData.size(), 0,
           reinterpret_cast<const sockaddr*>(&saTargetAddr), sizeof(saTargetAddr));
}

void ReceiveFrame(int nPort) {
    int nSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (nSock < 0) {
        #ifdef _WIN32
        std::cerr << "Error creating receive socket. WSA Error: " << WSAGetLastError() << std::endl;
        #else
        std::cerr << "Error creating receive socket." << std::endl;
        #endif
        return;
    }

    sockaddr_in saLocalAddr;
    memset(&saLocalAddr, 0, sizeof(saLocalAddr));
    saLocalAddr.sin_family = AF_INET;
    saLocalAddr.sin_port = htons(nPort);
    saLocalAddr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(nSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    if (bind(nSock, reinterpret_cast<sockaddr*>(&saLocalAddr), sizeof(saLocalAddr)) < 0) {
        #ifdef _WIN32
        std::cerr << "Error binding socket. WSA Error: " << WSAGetLastError() << std::endl;
        #else
        std::cerr << "Error binding socket." << std::endl;
        #endif
        #ifdef _WIN32
        closesocket(nSock);
        #else
        close(nSock);
        #endif
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms timeout
    setsockopt(nSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));


    const size_t MAX_BUFFER_SIZE = 1000000; // 1MB
    std::vector<uchar> vecBuffer(MAX_BUFFER_SIZE);
    sockaddr_in saSenderAddr;
    socklen_t nSenderAddrLen = sizeof(saSenderAddr);

    std::cout << "Receiver thread started. Listening on port " << nPort << std::endl;

    while (g_bRunning) {
        
        if (vecBuffer.size() != MAX_BUFFER_SIZE) {
            vecBuffer.resize(MAX_BUFFER_SIZE);
        }
        
        int nReceivedBytes = recvfrom(nSock, reinterpret_cast<char*>(vecBuffer.data()), vecBuffer.size(), 0,
                                          reinterpret_cast<sockaddr*>(&saSenderAddr), &nSenderAddrLen);

        if (nReceivedBytes > 0) {
            try {
                if (nReceivedBytes <= 0 || nReceivedBytes > static_cast<int>(MAX_BUFFER_SIZE)) {
                    std::cerr << "Invalid data size received: " << nReceivedBytes << " bytes" << std::endl;
                    continue;
                }
                
                if (nReceivedBytes < 2 || vecBuffer[0] != 0xFF || vecBuffer[1] != 0xD8) {
                    std::cerr << "Invalid data received (" << nReceivedBytes << " bytes)" << std::endl;
                    continue;
                }
                
                vecBuffer.resize(nReceivedBytes);
                cv::Mat receivedFrame = cv::imdecode(vecBuffer, cv::IMREAD_COLOR);

                if (!receivedFrame.empty()) {
                    cv::Mat copiedFrame;
                    receivedFrame.copyTo(copiedFrame);

                    std::lock_guard<std::mutex> lock(g_frameMutex);
                    g_frameQueue.push_back(copiedFrame);

                    if (g_frameQueue.size() > 5) {
                        g_frameQueue.pop_front();
                    }
                } else {
                    std::cerr << "Received JPEG header but couldn't decode image" << std::endl;
                }
            } catch (const cv::Exception& e) {
                std::cerr << "OpenCV exception in receiver: " << e.what() << std::endl;
                vecBuffer.resize(MAX_BUFFER_SIZE);
            } catch (const std::exception& e) {
                std::cerr << "Standard exception in receiver: " << e.what() << std::endl;
                vecBuffer.resize(MAX_BUFFER_SIZE);
            } catch (...) {
                std::cerr << "Error processing received frame data." << std::endl;
                vecBuffer.resize(MAX_BUFFER_SIZE);
            }
        }
    }

    std::cout << "Receiver thread shutting down." << std::endl;
    #ifdef _WIN32
    closesocket(nSock);
    #else
    close(nSock);
    #endif
}


int main()
{
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return -1;
    }
    #endif

    std::string strTargetIP = "";
    std::cout << "Enter target IP address: ";
    std::cin >> strTargetIP;

    if (strTargetIP.empty()) {
        std::cerr << "No IP address entered!" << std::endl;
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }

    int nSendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (nSendSock < 0) {
        std::cerr << "Error creating send socket." << std::endl;
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }

    sockaddr_in saTargetAddr;
    memset(&saTargetAddr, 0, sizeof(saTargetAddr));
    saTargetAddr.sin_family = AF_INET;
    saTargetAddr.sin_port = htons(8080);

    int pton_ret = inet_pton(AF_INET, strTargetIP.c_str(), &saTargetAddr.sin_addr);
    if (pton_ret <= 0) {
        if (pton_ret == 0) {
            std::cerr << "Error: Invalid IP address format provided." << std::endl;
        } else {
            #ifdef _WIN32
            std::cerr << "Error calling inet_pton. WSA Error: " << WSAGetLastError() << std::endl;
            #else
            std::cerr << "Error calling inet_pton." << std::endl;
            #endif
        }

        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSendSock);
        #endif
        return -1;
    }

    cv::VideoCapture vcCap(0); 
    if (!vcCap.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #endif
        return -1;
    }
    vcCap.set(cv::CAP_PROP_FRAME_WIDTH, 640); 
    vcCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480); 
    vcCap.set(cv::CAP_PROP_FPS, 30); 
    std::cout << "Camera opened successfully." << std::endl;

    cv::namedWindow("P2P Video Call - Received", cv::WINDOW_AUTOSIZE);
    cv::Mat matFrame, matReceivedFrame;
    cv::Mat matBlackFrame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::imshow("P2P Video Call - Received", matBlackFrame);
    cv::waitKey(1); 
    
    std::thread receiverThread(ReceiveFrame, 8080);
    receiverThread.detach();
    
    std::vector<uchar> vecCompressedFrame;

    try {

        while (g_bRunning) {
            vcCap >> matFrame;
            if (matFrame.empty()) {
                std::cerr << "Error: Could not capture frame." << std::endl;
                break;
            }

            cv::imencode(".jpg", matFrame, vecCompressedFrame, {cv::IMWRITE_JPEG_QUALITY, 80});
            SendFrame(nSendSock, vecCompressedFrame, saTargetAddr);
            {
                std::lock_guard<std::mutex> lock(g_frameMutex);
                if (!g_frameQueue.empty()) {
                    matReceivedFrame = g_frameQueue.front();
                    g_frameQueue.pop_front();
                }
            }

            if (!matReceivedFrame.empty() && matReceivedFrame.data != NULL) {
                cv::imshow("P2P Video Call - Received", matReceivedFrame);
            } else {
                cv::imshow("P2P Video Call - Received", matBlackFrame);
            }
            if ((cv::waitKey(30) & 0xFF) == 'q') {
                g_bRunning = false;
            }
        }
    } catch (const cv::Exception& e) {
        std::cerr << "FATAL: OpenCV exception caught: " << e.what() << std::endl;
        g_bRunning = false;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: Standard exception caught: " << e.what() << std::endl;
        g_bRunning = false;
    } catch (...) {
        std::cerr << "FATAL: Unknown exception caught." << std::endl;
        g_bRunning = false;
    }

    std::cout << "Main loop shutting down." << std::endl;
    g_bRunning = false;
    
    
    #ifdef _WIN32
    closesocket(nSendSock); 
    #else
    close(nSendSock);
    #endif

    vcCap.release();
    cv::destroyAllWindows();
    
    #ifdef _WIN32
    WSACleanup();
    #endif

    std::cout << "Application finished." << std::endl;
    return 0;

}