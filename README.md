
<h1 align="center">
  Peer2Peer Videocall App
</h1>

<h4 align="center">A basic P2P app that streams video using raw UDP packets</h4>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build-instructions">Build Instructions</a> •
  <a href="#screenshot">Screenshot</a> •
  <a href="#credits">Credits</a> •
  <a href="#what-did-i-learn">What did I learn?</a> •
  <a href="#license">License</a>
</p>



## 🧮 Key Features

- Captures webcam frames and encodes them to **JPEG**
- Transmits video frames using **UDP sockets**
- Decodes incoming frames and displays them in real time
- Peer-to-peer connection without a central server

## 📦 Dependencies

This project uses [vcpkg](https://github.com/microsoft/vcpkg) for dependency management.  

Required libraries:  
- [OpenCV](https://opencv.org/) – webcam capture & video rendering  
- [PortAudio](http://www.portaudio.com/) – audio capture/playback  
- [Opus](https://opus-codec.org/) – audio compression codec  
- [Winsock2 / BSD sockets] – UDP networking  

## 🛠 Build Instructions

1. Install [CMake](https://cmake.org/download/).  
2. Install [vcpkg](https://github.com/microsoft/vcpkg) and integrate it with CMake:  
   ```
   git clone https://github.com/microsoft/vcpkg
   cd vcpkg
   bootstrap-vcpkg.bat   # Windows
   ./bootstrap-vcpkg.sh  # Linux 
   ```
Then install dependencies:
  
    vcpkg install opencv portaudio opus
3. Clone this repo and build:
  ```
  git clone https://github.com/fludar/p2p-video-call
  cd p2p-video-call
  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
  cmake --build build
  ```
  
## 📸 Screenshot

![screenshot](https://raw.githubusercontent.com/fludar/p2p-videocall/main/resources/demo.gif)

Here I used OBS for a virtual webcam since I didn't have a webcam at the momment.

## 🙏 Credits

- [OpenCV](https://opencv.org/) – for video capture, JPEG encoding/decoding, and display  
- [PortAudio](http://www.portaudio.com/) – for cross-platform audio input/output  
- [Opus](https://opus-codec.org/) – for real-time audio compression and decompression  
- [Winsock2](https://learn.microsoft.com/en-us/windows/win32/winsock/) – for UDP networking on Windows  
- BSD sockets – for networking on Linux/macOS  
- [vcpkg](https://github.com/microsoft/vcpkg) – for dependency management and easy CMake integration


## 📖 What did I learn?
- Basics of capturing and encoding video frames using OpenCV
- Sending and receiving raw UDP packets
- Simple audio capture and playback with PortAudio and Opus
- Multi-threading and synchronization with mutexes and atomic flags
- Handling low-level networking and socket programming in C++
- Error handling and resource management in multimedia applications

## 📄 License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

