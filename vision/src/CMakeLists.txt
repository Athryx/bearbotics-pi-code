cmake_minimum_required(VERSION 2.8)
project(Vision)
set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

find_package(OpenCV REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)

include_directories(
	${OpenCV_INCLUDE_DIRS}
	${GLIB_INCLUDE_DIRS}
	${GSTREAMER_INCLUDE_DIRS}
	${GSTREAMER_RTSP_SERVER_INCLUDE_DIRS}
)

link_directories(
	${GLIB_LIBRARY_DIRS}
	${GSTREAMER_LIBRARY_DIRS}
	${GSTREAMER_RTSP_SERVER_LIBRARY_DIRS}
)

add_executable(Vision
	main.cpp
	util.cpp
	vision.cpp
	parallel.cpp
	remote_viewing.cpp
	mqtt.cpp
	logging.cpp
	error.cpp
)

target_link_libraries(Vision
	mosquitto
	${OpenCV_LIBS}
	${GSTREAMER_LIBRARIES}
	${GLIB_LIBRARIES}
	${GSTREAMER_RTSP_SERVER_LIBRARIES}
)
