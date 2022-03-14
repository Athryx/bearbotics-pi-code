cmake_minimum_required(VERSION 2.8)
project(hsv-viewer)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(hsv-viewer main.cpp)
target_link_libraries(hsv-viewer mosquitto ${OpenCV_LIBS})
