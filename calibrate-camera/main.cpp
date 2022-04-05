#include "argparse.hpp"
#include <cstdlib>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <stdlib.h>
#include <opencv2/opencv.hpp>

// opencv does not actually have key codes documented anywhere or constants for them, these were found out by testing
enum Key {
	KEY_ENTER = 13,
	KEY_ESC = 27,
};

enum class Mode {
	// used passed in images to calibrate camera
	Calibrate,
	// take pictures of the chessboard
	Capture,
};

argparse::ArgumentParser parse_args(int argc, char **argv) {
	argparse::ArgumentParser program("calibrate-camera", "0.1.0");

	program.add_argument("--mode")
		.help("mode to be in, pass in 'calibrate' to use input images to calibrate camera, or 'capture' to take images of the chessboard")
		.default_value(Mode::Calibrate)
		.default_repr("calibrate")
		.action([] (const std::string& str) {
			if (str == "calibrate") {
				return Mode::Calibrate;
			} else if (str == "capture") {
				return Mode::Capture;
			} else {
				throw std::runtime_error("invalid argument for --mode: must be either 'calibrate' or 'capture'");
			}
		});


	program.add_argument("-w", "--cam-width")
		.help("width of image read in from camera in pixels")
		.default_value(640)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-h", "--cam-height")
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


	program.add_argument("--chessboard-width")
		.help("the number of squares (black and white) along the width of the chessboard image")
		.default_value(7)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("--chessboard-height")
		.help("the number of squares (black and white) along the height of the chessboard image")
		.default_value(10)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("--square-length")
		.help("the length of each chessboard square in centimeters")
		.default_value(2.54)
		.action([] (const std::string& str) {
			return std::atof(str.c_str());
		});


	program.add_argument("-o", "--output")
		.help("output file name of camera calibration constants, can be either json, yaml, or xml depending on file extension")
		.default_value(std::string {"camera_calibration.json"});

	program.add_argument("input")
		.help("input images to calibrate camera with")
		.remaining();


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

	const Mode mode = program.get<Mode>("--mode");

	const int cam_width = program.get<int>("--cam-width");
	const int cam_height = program.get<int>("--cam-height");
	const int fps = program.get<int>("--fps");
	std::chrono::milliseconds frame_interval(1000 / fps);

	// this width and height is the number of points, not the number of squares, so it is 1 less than the input value
	const int chessboard_width = program.get<int>("--chessboard-width") - 1;
	const int chessboard_height = program.get<int>("--chessboard-height") - 1;
	cv::Size chessboard_size(chessboard_width, chessboard_height);

	const double square_length = program.get<double>("--square-length");

	const std::string output_file = program.get("--output");
	std::vector<std::string> input_files;
	if (program.is_used("input")) {
		input_files = program.get<std::vector<std::string>>("input");
	}

	if (mode == Mode::Capture) {
		cv::VideoCapture cap;
	
		// cv::CAP_V4L2 is needed because by default it might use gstreamer, and because of a bug in opencv, this causes open to fail
		// if this is ever run not on linux, this will need to be changed
		cap.open(0, cv::CAP_V4L2);
		cap.set(cv::CAP_PROP_FRAME_WIDTH, cam_width);
		cap.set(cv::CAP_PROP_FRAME_HEIGHT, cam_height);
		cap.set(cv::CAP_PROP_FPS, fps);
	
		if (!cap.isOpened()) {
			std::cout << "error: could not open input file or camera" << std::endl;
			exit(1);
		}
	
		const std::string chessboard_window_name = "Chessboard Corners";
		const std::string snapshot_window_name = "Snapshot Image";
	
		cv::namedWindow(chessboard_window_name, cv::WINDOW_AUTOSIZE | cv::WINDOW_KEEPRATIO | cv::WINDOW_GUI_NORMAL);
	
		for (;;) {
			auto next_frame_time = std::chrono::steady_clock::now() + frame_interval;
	
			cv::Mat img;
			cap >> img;
			if (img.empty()) {
				std::cout << "warning: skipping empty frame" << std::endl;
				continue;
			}
	
			std::vector<cv::Point2f> corners;
	
			bool found = cv::findChessboardCornersSB(img, chessboard_size, corners, cv::CALIB_CB_NORMALIZE_IMAGE);
	
			cv::Mat img_no_corners = img.clone();
			cv::drawChessboardCorners(img, chessboard_size, corners, found);
	
			cv::imshow(chessboard_window_name, img);
	
			switch(cv::pollKey()) {
				case KEY_ENTER:
					// show the current image in a seperate window to decide if user wants to save it
					if (!cv::getWindowProperty(snapshot_window_name, cv::WND_PROP_VISIBLE)) {
						cv::imshow(snapshot_window_name, img_no_corners);
					}
					break;
				case KEY_ESC:
					exit(0);
					break;
				default:
					break;
			}
	
			// exit if main window has been closed
			if (!cv::getWindowProperty(chessboard_window_name, cv::WND_PROP_VISIBLE)) {
				exit(0);
			}
	
			std::this_thread::sleep_until(next_frame_time);
		}
	} else {
		if (input_files.size() == 0) {
			std::cout << "error: no input files provided for camera calibration" << std::endl;
			exit(1);
		}

		// calculate the points of the chessboard in object space
		std::vector<cv::Point3f> chessboard_object_points;
		// go across the rows first, because that is how cv::drawChessboardCorners draws the chessboard corners
		// make the bottom left corner 0,0
		const double total_height = (chessboard_height - 1) * square_length;
		for (int row = 0; row < chessboard_height; row ++) {
			for (int col = 0; col < chessboard_width; col ++) {
				chessboard_object_points.push_back(cv::Vec3f(col * square_length, total_height - (row * square_length), 0.0));
			}
		}

		// find chessboard corners in each image
		std::vector<std::vector<cv::Point2f>> chessboard_corners;
		for (auto& file : input_files) {
			auto image = cv::imread(file);
			std::vector<cv::Point2f> points;

			if (cv::findChessboardCornersSB(image, chessboard_size, points, cv::CALIB_CB_NORMALIZE_IMAGE)) {
				chessboard_corners.push_back(std::move(points));
			} else {
				std::cout << "warning: no chess board points found in image '" << file << "', skipping" << std::endl;
			}
		}

		if (chessboard_corners.size() == 0) {
			std::cout << "error: no input images had any chessboard corners" << std::endl;
			exit(1);
		}

		// cv::calibrateCamera needs a copy of the object points for every input image
		std::vector<std::vector<cv::Point3f>> object_points(chessboard_corners.size(), chessboard_object_points);

		cv::Mat camera_matrix(3, 3, CV_64FC1);
		cv::Mat distortion_matrix(4, 4, CV_64FC1);

		// TODO: figure out if chessboard_size is right here
		cv::calibrateCamera(object_points, chessboard_corners, chessboard_size, camera_matrix, distortion_matrix, cv::noArray(), cv::noArray());

		// save the result to a file
		cv::FileStorage file(output_file, cv::FileStorage::WRITE | cv::FileStorage::FORMAT_AUTO);
		if (!file.isOpened()) {
			std::cout << "error: could not open output file '" << output_file << "'" << std::endl;
			exit(1);
		}

		file.write("camera_matrix", camera_matrix);
		file.write("distortion_matrix", distortion_matrix);

		file.release();
	}
}
