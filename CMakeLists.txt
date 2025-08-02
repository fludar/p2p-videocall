cmake_minimum_required(VERSION 3.15)
project(P2PVideoCall)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV package
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(p2p-video-call
src/main.cpp
)

# Link OpenCV libraries
if(WIN32)
    target_link_libraries(p2p-video-call ${OpenCV_LIBRARIES} ws2_32)
else()
    target_link_libraries(p2p-video-call ${OpenCV_LIBRARIES})
endif()
target_include_directories(p2p-video-call PRIVATE ${OpenCV_INCLUDE_DIRS})
