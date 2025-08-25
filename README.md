# P2P Video Call (Experimental)

This is an experimental peer-to-peer video calling application written in C++.  
Currently it sends **JPEG frames over raw UDP sockets**, which makes it more of a proof-of-concept than a usable app.

⚠️ **Warning**: This project is not secure or production-ready. It does not include NAT traversal, encryption, or reliability mechanisms.  

## Features (current)
- Captures frames from the local camera
- Encodes them as JPEG
- Sends frames over UDP sockets to a peer
- Very basic rendering of incoming frames



## Build Instructions

### Prerequisites
- **CMake** (≥3.15 recommended)
- **vcpkg** for dependency management
- A C++17 (or newer) compiler  
- Windows (tested) or Linux

### Build
```bash
git clone https://github.com/fludar/p2p-videocall.git
cd p2p-videocall
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## Running
Start the program on two machines (or two terminals), making sure they can reach each other’s IP/port.  
Currently you need to manually input the peer IP.

```bash
./p2p-videocall
```

## Limitations / Next Steps

This project is currently a **prototype**. If developed further, it would require:

- Replacing raw UDP with a proper transport (e.g. WebRTC for encryption, NAT traversal, and reliability)  
- A signaling server for peer discovery and connection setup  
- Real video codec support (H.264, VP8, AV1) instead of JPEG frames  
- Full audio support with proper encoding/decoding  
- A usable user interface (start/stop call, peer list)  
- Cross-platform support (Windows, Linux, Android)

## What did I learn until this point?

- Basics of capturing and encoding video frames using OpenCV
- Sending and receiving raw UDP packets
- Simple audio capture and playback with PortAudio and Opus
- Multi-threading and synchronization with mutexes and atomic flags
- Handling low-level networking and socket programming in C++ 
- Error handling and resource management in multimedia applications
