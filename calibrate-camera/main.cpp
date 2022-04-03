#include "argparse.hpp"
#include <cstdlib>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <opencv2/opencv.hpp>

argparse::ArgumentParser parse_args(int argc, char **argv) {
	argparse::ArgumentParser program("calibrate-camera", "0.1.0");

	program.add_argument("-w", "--width")
		.help("width of image read in from camera in pixels")
		.default_value(640)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-h", "--height")
		.help("height of image read in from camera in pixels")
		.default_value(640)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-f", "--fps")
		.help("frames per second to read in from camera")
		.default_value(30)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-i", "--input")
		.help("image of chessboard, if this argument is used, the image will be used to calibrate the camera");

	program.add_argument("-o", "--output")
		.help("output file name of camera calibration constants")
		.default_value("camera-constants.json");

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		exit(1);
	}

	return program;
}

int main(int argc, char **argv) {
	auto program = parse_args(argc, argv);

	const int width = program.get<int>("--width");
	const int height = program.get<int>("--height");
	const int fps = program.get<int>("--fps");
	const std::string output_file = program.get("--output");

	cv::VideoCapture cap;

	if (program.is_used("--input")) {
		cap.open(program.get("--input"), cv::CAP_V4L2);
	} else {
		// cv::CAP_V4L2 is needed because by default it might use gstreamer, and because of a bug in opencv, this causes open to fail
		// if this is ever run not on linux, this will need to be changed
		cap.open(0, cv::CAP_V4L2);
		cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
		cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
		cap.set(cv::CAP_PROP_FPS, fps);
	}

	if (!cap.isOpened()) {
		std::cout << "error: could not open input file or camera" << std::endl;
		exit(1);
	}
}
