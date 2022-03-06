#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <vector>
#include <functional>
#include "error.h"

// represents a detected target
struct Target {
	double distance;
	double angle;
};

// wrapper around opencv VideoCapture to quickly open and close with the correct arguments
class VisionCamera {
	public:
		VisionCamera(std::optional<std::string>& filename, int width, int height, int fps);
		VisionCamera(std::optional<std::string>&& filename, int width, int height, int fps);
		~VisionCamera();

		Error start();
		Error stop();

		Error read_to(cv::Mat& mat);

	private:
		cv::VideoCapture m_cap;
		std::optional<std::string> m_filename;
		int m_cam_width;
		int m_cam_height;
		int m_max_fps;
		bool m_enabled { false };
};

// TODO: come up with better class name
class Vision {
	public:
		Vision(cv::Mat template_img, int threads, bool display);
		~Vision();

		void set_threads(int threads);

		void process_template(cv::Mat img);
		std::optional<Target> process(cv::Mat img) const;

	private:
		void show(const std::string& name, cv::Mat& img) const;
		void show_wait(const std::string& name, cv::Mat& img) const;
		void task(cv::Mat in, cv::Mat out, std::function<void(cv::Mat, cv::Mat)> func) const;

		int m_threads;
		bool m_display;

		cv::Scalar m_thresh_min { cv::Scalar(10, 70, 70) };
		cv::Scalar m_thresh_max { cv::Scalar(40, 255, 255) };

		std::vector<cv::Point> m_template_contour {};
		double m_template_area_frac { 0.0 };
};
