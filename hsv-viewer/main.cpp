#include "argparse.hpp"
#include <cstdlib>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <stdio.h>
#include <vector>
#include <algorithm>

std::string window_name("hsv-viewer");
cv::Mat src;

std::vector<cv::Point> points;

int hmin = 180;
int smin = 255;
int vmin = 255;

int hmax = 0;
int smax = 0;
int vmax = 0;

static void onMouse(int e, int x, int y, int f, void *unused) {
	cv::MouseEventTypes event = (cv::MouseEventTypes) e;
	cv::MouseEventFlags flags = (cv::MouseEventFlags) f;

	cv::Mat image = src.clone();

	cv::Mat hsv_img;
	cv::Mat rgb_img = image(cv::Rect(x, y, 1, 1));
	cvtColor(rgb_img, hsv_img, cv::COLOR_RGB2HSV);

	cv::Vec3b rgb = image.at<cv::Vec3b>(y, x);
	int r = rgb.val[0];
	int g = rgb.val[1];
	int b = rgb.val[2];

	cv::Vec3b hsv = hsv_img.at<cv::Vec3b>(0, 0);
	int h = hsv.val[0];
	int s = hsv.val[1];
	int v = hsv.val[2];

	if (event == cv::EVENT_LBUTTONDOWN) {
		points.push_back(cv::Point(x, y));

		hmin = std::min(hmin, h);
		smin = std::min(smin, s);
		vmin = std::min(vmin, v);

		hmax = std::max(hmax, h);
		smax = std::max(smax, s);
		vmax = std::max(vmax, v);

		printf("New set of bounds\n");
		printf("min: H: %d, S: %d, V: %d\n", hmin, smin, vmin);
		printf("max: H: %d, S: %d, V: %d\n", hmax, smax, vmax);
	}

	auto font = cv::FONT_HERSHEY_SIMPLEX;

	for (auto point : points) {
		cv::circle(image, point, 5, cv::Scalar(255, 0, 0), cv::FILLED);
	}

	char name[30];
	sprintf(name, "B=%d", b);
	cv::putText(image, name, cv::Point(150,40), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "G=%d", g);
	cv::putText(image, name, cv::Point(150,80), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "R=%d", r);
	cv::putText(image, name, cv::Point(150,120), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "H=%d", h);
	cv::putText(image, name, cv::Point(25,40), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "S=%d", s);
	cv::putText(image, name, cv::Point(25,80), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "V=%d", v);
	cv::putText(image, name, cv::Point(25,120), font, 0.7, cv::Scalar(0, 255, 0), 2, 8, false);

	sprintf(name, "X=%d", x);
	cv::putText(image, name, cv::Point(25,300), font, 0.7, cv::Scalar(0, 0, 255), 2, 8, false);

	sprintf(name, "Y=%d", y);
	cv::putText(image, name, cv::Point(25,340), font, 0.7, cv::Scalar(0, 0, 255), 2, 8, false);

	imshow(window_name, image);
}

int main(int argc, char **argv) {
	argparse::ArgumentParser program("hsv-viewew", "0.0.1");

	program.add_argument("image")
		.help("8 bit rgb image to view in the hsv viewer");

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		exit(1);
	}

	auto image = program.get("image");

	src = cv::imread(image, 1);
	imshow(window_name, src);
	cv::setMouseCallback(window_name, onMouse, 0);
	cv::waitKey();
}
