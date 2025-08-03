cmake_minimum_required(VERSION 3.15)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(P2PVideoCall)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PortAudio REQUIRED)
find_package(Opus REQUIRED)

add_executable(p2p-video-call
src/main.cpp
)
target_include_directories(p2p-video-call PRIVATE ${PORTAUDIO_INCLUDE_DIRS} ${OPUS_INCLUDE_DIRS})
target_link_libraries(p2p-video-call
    portaudio
    ${OPUS_LIBRARIES}
)

if(WIN32)
    target_link_libraries(p2p-video-call ${OpenCV_LIBRARIES} ws2_32)
else()
    target_link_libraries(p2p-video-call ${OpenCV_LIBRARIES})
endif()
target_include_directories(p2p-video-call PRIVATE ${OpenCV_INCLUDE_DIRS})
