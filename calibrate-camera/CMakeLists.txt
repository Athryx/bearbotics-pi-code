cmake_minimum_required(VERSION 2.8)
project(calibrate-camera)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(calibrate-camera main.cpp)
target_link_libraries(calibrate-camera ${OpenCV_LIBS})
