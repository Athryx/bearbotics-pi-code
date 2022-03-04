cmake_minimum_required(VERSION 2.8)
project(Vision)
set(CMAKE_CXX_STANDARD 17)

set(OpenCV_DIR /usr/share/OpenCV)
# the find gstreamer cmake script is in the project directory
set(glib_DIR /usr/share/glib-2.0)
set(gstreamer_DIR ${CMAKE_CURRENT_LIST_DIR})

find_package(PkgConfig REQUIRED)

find_package(OpenCV REQUIRED)
find_package(gstreamer REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(
	${OpenCV_INCLUDE_DIRS}
	${gstreamer_INCLUDE_DIRS}
	${GLIB_INCLUDE_DIRS}
)

add_executable(Vision main.cpp util.cpp vision.cpp parallel.cpp remote_viewing.cpp)
target_link_libraries(Vision mosquitto ${OpenCV_LIBS} ${gstreamer_LIBS} ${glib_LIBS})
