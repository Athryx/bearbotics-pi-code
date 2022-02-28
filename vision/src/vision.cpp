#include "types.h"
#include "vision.h"
#include "util.h"
#include "parallel.h"
#include <math.h>

Vision::Vision(cv::Mat template_img, int threads, bool display)
: m_threads(threads)
, m_display(display)
{
	process_template(template_img);
}

Vision::~Vision() {
}

void Vision::set_threads(int threads) {
	m_threads = threads;
}

void Vision::process_template(cv::Mat img) {
	auto img_template = img.clone();
	cv::cvtColor(img_template, img_template, cv::COLOR_BGR2HSV, 8);
	cv::inRange(img_template, m_thresh_min, m_thresh_max, img_template);
	// TODO: pass kernel into morphologyEx instead of plain cv::Mat()
	cv::morphologyEx(img_template, img_template, cv::MORPH_OPEN, cv::Mat());

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(img_template, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	show_wait("Template", img_template);

	// find largest contour
	usize index = 0;
	double max_area = 0;
	for (usize i = 0; i < contours.size(); i ++) {
		double area = cv::contourArea(contours[i]);
		if (area > max_area) {
			max_area = area;
			index = i;
		}
	}

	m_template_contour.swap(contours[index]);
	m_template_area_frac = max_area / cv::boundingRect(m_template_contour).area();
}

std::optional<Target> Vision::process(cv::Mat img) const {
	cv::Size size(img.cols, img.rows);

	show("Input", img);

	// TODO: figure out type of BGR mat
	cv::Mat img_hsv(size, img.type());
	time("HSV conversion", [&] () {
		task(img, img_hsv, [] (cv::Mat in, cv::Mat out) {
			cv::cvtColor(in, out, cv::COLOR_BGR2HSV, 8);
		});
	});

	cv::Mat img_thresh(size, CV_8U);
	time("Threshold", [&] () {
		task(img_hsv, img_thresh, [&] (cv::Mat in, cv::Mat out) {
			cv::inRange(in, m_thresh_min, m_thresh_max, out);
		});
	});
	show("Threshold", img_thresh);

	cv::Mat img_morph(size, CV_8U);
	// TODO: pass kernel into morphologyEx instead of plain cv::Mat()
	time("Morphology", [&] () {
		cv::morphologyEx(img_thresh, img_morph, cv::MORPH_OPEN, cv::Mat());
	});
	show("Morphology", img_morph);

	// TODO: reserve eneough space in vector to prevent reallocations
	std::vector<std::vector<cv::Point>> contours;
	time("Contours", [&] () {
		cv::findContours(img_morph, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
	});

	usize match_index = 0;
	double best_match = INFINITY;
	double best_area = 0.0;

	time("Contour Matching", [&] () {
		for (usize i = 0; i < contours.size(); i ++) {
			auto contour = contours[i];
			double match = cv::matchShapes(m_template_contour, contour, cv::CONTOURS_MATCH_I3, 0.0);

			double area = cv::contourArea(contour);
			double area_frac = area / cv::boundingRect(contour).area();

			if (match < best_match && match < 1.5 && abs(area_frac - m_template_area_frac) / m_template_area_frac < 0.2 && area > best_area) {
				best_match = match;
				best_area = area;
				match_index = i;
			}
		}
	});

	char text[32];
	int font_face = cv::FONT_HERSHEY_SIMPLEX;
	double font_scale = 0.5;
	cv::Point text_point(40, 40);

	if (best_match == INFINITY) {
		if (m_display) {
			auto img_show = img.clone();

			cv::putText(img_show, "match: none", text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
			text_point.y += 15;
			cv::putText(img_show, "distance: unknown", text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
			text_point.y += 15;
			cv::putText(img_show, "angle: unknown", text_point, font_face, font_scale, cv::Scalar(0, 0, 255));

			cv::imshow("Contours", img_show);
		}
		return {};
	}

	auto rect = cv::boundingRect(contours[match_index]);
	Target out;
	out.distance = 11386.95362494479 * (1.0 / rect.width);
	auto xpos = rect.x + rect.width / 2;
	out.angle = atan((xpos - 320) / 530.47) * (180.0 / M_PI) + 16;

	if (m_display) {
		auto img_show = img.clone();

		cv::drawContours(img_show, contours, match_index, cv::Scalar(0, 0, 255));
		cv::rectangle(img_show, rect, cv::Scalar(0, 255, 0));

		snprintf(text, 32, "match: %6.2f", best_match);
		cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
		text_point.y += 15;
		snprintf(text, 32, "distance: %6.2f", out.distance);
		cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
		text_point.y += 15;
		snprintf(text, 32, "angle: %6.2f", out.angle);
		cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));

		cv::imshow("Contours", img_show);
	}

	return out;
}

void Vision::show(const std::string& name, cv::Mat& img) const {
	if (m_display) {
		cv::imshow(name, img);
	}
}

void Vision::show_wait(const std::string& name, cv::Mat& img) const {
	if (m_display) {
		cv::imshow(name, img);
		cv::waitKey();
	}
}

void Vision::task(cv::Mat in, cv::Mat out, std::function<void(cv::Mat, cv::Mat)> func) const {
	if (m_threads > 1) {
		parallel_process(in, out, func, m_threads);
	} else {
		func(in, out);
	}
}
