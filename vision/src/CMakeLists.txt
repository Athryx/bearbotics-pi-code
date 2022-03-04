cmake_minimum_required(VERSION 2.8)
project(Vision)
set(CMAKE_CXX_STANDARD 17)

set(OpenCV_DIR /usr/share/OpenCV)
# the find gstreamer cmake script is in the project directory
#set(glib_DIR /usr/share/glib-2.0)

find_package(PkgConfig REQUIRED)

find_package(OpenCV REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

message(STATUS ${GSTREAMER_INCLUDE_DIRS})

include_directories(
	${OpenCV_INCLUDE_DIRS}
	${GLIB_INCLUDE_DIRS}
	${GSTREAMER_INCLUDE_DIRS}
)

link_directories(
	${GLIB_LIBRARY_DIRS}
	${GSTREAMER_LIBRARY_DIRS}
)

add_executable(Vision main.cpp util.cpp vision.cpp parallel.cpp remote_viewing.cpp)
target_link_libraries(Vision mosquitto ${OpenCV_LIBS} ${gstreamer_LIBS} ${glib_LIBS})
