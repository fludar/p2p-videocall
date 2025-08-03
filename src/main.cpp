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

#include <portaudio.h>
#include <opus/opus.h>

#define MAX_BUFFER_SIZE 1000000 
#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 960
#define CHANNELS 1 


std::mutex g_audioMutex;
std::mutex g_frameMutex;
std::deque<cv::Mat> g_frameQueue;
std::atomic<bool> g_bRunning(true);

struct AudioState {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    PaStream *inputStream;
    PaStream *outputStream;
    std::vector<unsigned char> encodedBuffer;
    std::vector<float> audioBuffer;
    int nSock;
    sockaddr_in saTargetAddr;
};

static int AudioInputCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData) {
    AudioState *audio = (AudioState*)userData;
    const float *in = (const float*)inputBuffer;
    
    if(inputBuffer == nullptr) {
        return paContinue;
    }
    
    if (!audio->encoder || !audio->encodedBuffer.data()) {
        std::cerr << "Audio encoder not initialized" << std::endl;
        return paAbort;
    }

    opus_int32 encodedBytes = opus_encode_float(
        audio->encoder,
        in,
        FRAMES_PER_BUFFER,
        audio->encodedBuffer.data(),
        audio->encodedBuffer.size()
    );
    
    if (encodedBytes < 0) {
        std::cerr << "Opus encoding error: " << encodedBytes << std::endl;
        return paContinue;
    }

    if (encodedBytes > 0) {
        try{
            std::vector<uchar> vecPacket;
            vecPacket.reserve(encodedBytes + 2);
            vecPacket.push_back(0xAA); //Audio packet marker
            vecPacket.push_back(0xBB); 
            
            vecPacket.insert(vecPacket.end(), audio->encodedBuffer.begin(), audio->encodedBuffer.begin() + encodedBytes);
            
            

        if(audio->nSock > 0){
            std::cout << "Audio TX: " << encodedBytes << " bytes" << std::endl; // to debug
            sendto(audio->nSock, reinterpret_cast<const char*>(vecPacket.data()), vecPacket.size(), 0,
               reinterpret_cast<const sockaddr*>(&audio->saTargetAddr), sizeof(audio->saTargetAddr));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error sending audio packet: " << e.what() << std::endl;
            return paAbort;
        }
    }
    
    return paContinue;
}

static int AudioOutputCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData) {
    AudioState *audio = (AudioState*)userData;
    float *out = (float*)outputBuffer;

    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (audio->audioBuffer.empty()) {
        memset(out, 0, framesPerBuffer * CHANNELS * sizeof(float));
        return paContinue;
    }

    size_t samplesToCopy = (((framesPerBuffer * 1) < ((unsigned long)audio->audioBuffer.size())) ? (framesPerBuffer * 1) : ((unsigned long)audio->audioBuffer.size()));
    //use vscodes "expands to" feature to see the full expression because std::min doesnt work for me for some reason

    memcpy(out, audio->audioBuffer.data(), samplesToCopy * sizeof(float));

    if (samplesToCopy < framesPerBuffer * CHANNELS) {
        memset(out + samplesToCopy, 0, 
              (framesPerBuffer * CHANNELS - samplesToCopy) * sizeof(float));
    }

    audio->audioBuffer.erase(audio->audioBuffer.begin(), 
                           audio->audioBuffer.begin() + samplesToCopy);
    
    return paContinue;
}

void ReceiveAudio(AudioState* audio){
    std::vector<uchar> vecBuffer(MAX_BUFFER_SIZE);
    sockaddr_in saSenderAddr;
    socklen_t nSenderAddrLen = sizeof(saSenderAddr);

    int audioSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (audioSocket < 0) {
        std::cerr << "Failed to create audio receive socket" << std::endl;
        return;
    }

    // Bind to a different port (e.g., 8081 for audio)
    sockaddr_in audioAddr;
    memset(&audioAddr, 0, sizeof(audioAddr));
    audioAddr.sin_family = AF_INET;
    audioAddr.sin_port = htons(8081);  // Different port than video
    audioAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(audioSocket, reinterpret_cast<sockaddr*>(&audioAddr), sizeof(audioAddr)) < 0) {
        std::cerr << "Failed to bind audio socket" << std::endl;
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms timeout
    setsockopt(audioSocket, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    
    while (g_bRunning) {
        int nReceivedBytes = recvfrom(audioSocket, reinterpret_cast<char*>(vecBuffer.data()), vecBuffer.size(), 0,
                                      reinterpret_cast<sockaddr*>(&saSenderAddr), &nSenderAddrLen);
        if (nReceivedBytes > 2 && vecBuffer[0] == 0xAA && vecBuffer[1] == 0xBB) {
            std::cout << "Audio RX: " << nReceivedBytes << " bytes" << std::endl;
            std::vector<float> decoded(FRAMES_PER_BUFFER * CHANNELS);
            int decodedSamples = opus_decode_float(
                audio->decoder,
                vecBuffer.data() + 2, 
                nReceivedBytes - 2, 
                decoded.data(), 
                FRAMES_PER_BUFFER, 
                0
            );

            if (decodedSamples > 0) {
                std::lock_guard<std::mutex> lock(g_audioMutex);
                for (int i = 0; i < decodedSamples * CHANNELS; i++) {
                    audio->audioBuffer.push_back(decoded[i]);
                }

                const size_t MAX_BUFFER = FRAMES_PER_BUFFER * CHANNELS * 5;
                if (audio->audioBuffer.size() > MAX_BUFFER) {
                    audio->audioBuffer.erase(audio->audioBuffer.begin(), 
                    audio->audioBuffer.begin() + (audio->audioBuffer.size() - MAX_BUFFER));
                }
            }
            std::cout << "Audio Play: " << audio->audioBuffer.size() << " samples" << std::endl;
        }
    }

    #ifdef _WIN32
    closesocket(audioSocket);
    #else
    close(audioSocket);
    #endif
}

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

    AudioState audioState;
    audioState.encodedBuffer.resize(MAX_BUFFER_SIZE);
    audioState.audioBuffer.resize(FRAMES_PER_BUFFER * CHANNELS);

    
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
    
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSendSock);
        #endif
        return -1;
    }
    int opusError;
    audioState.encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &opusError);
    if (opusError != OPUS_OK || !audioState.encoder) {
        std::cerr << "Failed to create Opus encoder: " << opus_strerror(opusError) << std::endl;
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSendSock);
        #endif
        return -1;
    }

    audioState.decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opusError);
    if (opusError != OPUS_OK || !audioState.decoder) {
        std::cerr << "Failed to create Opus decoder: " << opus_strerror(opusError) << std::endl;
        opus_encoder_destroy(audioState.encoder);
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSendSock);
        #endif
        return -1;
    }

    err = Pa_OpenDefaultStream(
    &audioState.inputStream,
    CHANNELS,          
    0,                 
    paFloat32,         
    SAMPLE_RATE,       
    FRAMES_PER_BUFFER, 
    AudioInputCallback,
    &audioState        
    );

    sockaddr_in saTargetAddr;
    memset(&saTargetAddr, 0, sizeof(saTargetAddr));
    saTargetAddr.sin_family = AF_INET;
    saTargetAddr.sin_port = htons(8080);
 
    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio input stream: " << Pa_GetErrorText(err) << std::endl;
        opus_encoder_destroy(audioState.encoder);
        opus_decoder_destroy(audioState.decoder);
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSock);
        #endif
        return -1;
    }

    err = Pa_StartStream(audioState.inputStream);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(audioState.inputStream);
        opus_encoder_destroy(audioState.encoder);
        opus_decoder_destroy(audioState.decoder);
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSock);
        #endif
        return -1;
    }

    

    err = Pa_OpenDefaultStream(
    &audioState.outputStream,
    0,                 
    CHANNELS,          
    paFloat32,         
    SAMPLE_RATE,       
    FRAMES_PER_BUFFER, 
    AudioOutputCallback,
    &audioState
    );  

    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio output stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(audioState.inputStream);
        opus_encoder_destroy(audioState.encoder);
        opus_decoder_destroy(audioState.decoder);
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSock);
        #endif
        return -1;
    }

    err = Pa_StartStream(audioState.outputStream);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio output stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(audioState.inputStream);
        Pa_CloseStream(audioState.outputStream);
        opus_encoder_destroy(audioState.encoder);
        opus_decoder_destroy(audioState.decoder);
        Pa_Terminate();
        #ifdef _WIN32
        closesocket(nSendSock);
        WSACleanup();
        #else
        close(nSock);
        #endif
        return -1;
    }
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

    audioState.saTargetAddr = saTargetAddr;
    audioState.saTargetAddr.sin_addr = saTargetAddr.sin_addr;
    audioState.saTargetAddr.sin_port = htons(8081);  
    audioState.nSock = nSendSock;

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
    std::thread audioReceiverThread(ReceiveAudio, &audioState);
    audioReceiverThread.detach();
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
    if (audioState.inputStream) {
        Pa_StopStream(audioState.inputStream);
        Pa_CloseStream(audioState.inputStream);
    }
    if (audioState.outputStream) {
        Pa_StopStream(audioState.outputStream);
        Pa_CloseStream(audioState.outputStream);
    }
    std::cout << "Application finished." << std::endl;
    return 0;

}