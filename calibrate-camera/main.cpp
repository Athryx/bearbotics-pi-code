#include "argparse.hpp"
#include <string>
#include <iostream>
#include <stdlib.h>

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

	program.add_argument("-o")
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
}
