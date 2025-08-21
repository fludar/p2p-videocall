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

## Roadmap / TODO
This project is at **prototype stage**. To make it actually usable, the following need to be implemented:

- [ ] Replace raw UDP transport with **WebRTC** (via [libdatachannel](https://github.com/paullouisageneau/libdatachannel) or Google WebRTC)  
  - Provides encryption (DTLS-SRTP)  
  - NAT traversal (ICE, STUN, TURN)  
  - Reliable transport (retransmissions, congestion control)  
- [ ] Implement a simple **signaling server** (WebSocket/HTTP) to exchange SDP offers/answers and ICE candidates  
- [ ] Switch from JPEG to a **real video codec** (H.264, VP8, or AV1)  
- [ ] Add proper audio support  
- [ ] User interface (start/stop call, peer list)  
- [ ] Cross-platform testing (Windows, Linux, maybe Android)  

## What did I learn until this point?

- Basics of capturing and encoding video frames using OpenCV
- Sending and receiving raw UDP packets
- Simple audio capture and playback with PortAudio and Opus
- Multi-threading and synchronization with mutexes and atomic flags
- Handling low-level networking and socket programming in C++ 
- Error handling and resource management in multimedia applications
