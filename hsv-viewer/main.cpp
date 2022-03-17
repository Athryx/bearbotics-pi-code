#include <cstdlib>
#include <iostream>
#include <mutex>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <string.h>
#include <algorithm>

struct HsvVals {
	int hmin { 0 };
	int smin { 0 };
	int vmin { 0 };

	int hmax { 0 };
	int smax { 0 };
	int vmax { 0 };
};

struct HsvPoint {
	cv::Point point;
	int h;
	int s;
	int v;
};

struct WindowData {
	WindowData(cv::Mat&& img):
	img(std::move(img)) {}

	WindowData(WindowData&& data) = default;

	cv::Mat img;
	std::vector<HsvPoint> points {};
};

std::mutex windows_lock;
std::unordered_map<std::string, WindowData> windows;

std::mutex vals_lock;
HsvVals vals = HsvVals {
	.hmin = 256,
	.smin = 256,
	.vmin = 256,
	.hmax = 0,
	.smax = 0,
	.vmax = 0,
};

// TODO: add a way of undoing clicks
static void onMouse(int e, int x, int y, int f, void *in_window_name) {
	std::string window_name((char *) in_window_name);

	cv::MouseEventTypes event = (cv::MouseEventTypes) e;
	cv::MouseEventFlags flags = (cv::MouseEventFlags) f;

	std::scoped_lock<std::mutex> lock(windows_lock);

	auto& window_data = windows.at(window_name);

	cv::Mat image = window_data.img.clone();

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
		auto point = HsvPoint {
			.point = cv::Point(x, y),
			.h = h,
			.s = s,
			.v = v,
		};

		window_data.points.push_back(point);

		{
			std::scoped_lock<std::mutex> lock2(vals_lock);

			vals.hmin = std::min(vals.hmin, h);
			vals.smin = std::min(vals.smin, s);
			vals.vmin = std::min(vals.vmin, v);
	
			vals.hmax = std::max(vals.hmax, h);
			vals.smax = std::max(vals.smax, s);
			vals.vmax = std::max(vals.vmax, v);

			// TODO: only print if min or max actually changes
			printf("New set of bounds\n");
			printf("min: H: %d, S: %d, V: %d\n", vals.hmin, vals.smin, vals.vmin);
			printf("max: H: %d, S: %d, V: %d\n", vals.hmax, vals.smax, vals.vmax);
		}
	}

	auto font = cv::FONT_HERSHEY_SIMPLEX;

	for (auto& point : window_data.points) {
		cv::circle(image, point.point, 5, cv::Scalar(255, 0, 0), cv::FILLED);
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

void usage() {
	printf("Usage: hsv-viewer [options] [images]...\n\n"
		"Positional Arguments:\n"
		"images		list of 8 bit rgb images to to view in the hsv viewer\n\n"
		"Optional Arguments:\n"
		"-h --help	shows help message and exits\n");
}

int main(int argc, char **argv) {
	if (argc <= 1) {
		printf("must provide at least 1 argument\n");
		usage();
		exit(1);
	}

	if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		usage();
		exit(0);
	}


	windows_lock.lock();

	for (int i = 1; i < argc; i ++) {
		std::string arg(argv[i]);

		if (windows.contains(arg)) {
			continue;
		}

		auto img = cv::imread(arg, 1);
		cv::imshow(arg, img);
		windows.insert({arg, WindowData(std::move(img))});
		cv::setMouseCallback(arg, onMouse, argv[i]);
	}

	windows_lock.unlock();

	cv::waitKey();
}
