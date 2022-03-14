#include "types.h"
#include "vision.h"
#include "util.h"
#include "parallel.h"
#include "logging.h"
#include <math.h>
#include <opencv2/imgproc.hpp>

VisionCamera::VisionCamera(std::optional<std::string>& filename, int width, int height, int fps):
m_cap(),
m_cam_width(width),
m_cam_height(height),
m_max_fps(fps),
m_filename(filename) {}

VisionCamera::VisionCamera(std::optional<std::string>&& filename, int width, int height, int fps):
m_cap(),
m_cam_width(width),
m_cam_height(height),
m_max_fps(fps),
m_filename(std::move(filename)) {}

VisionCamera::~VisionCamera() {
	stop().ignore();
}

Error VisionCamera::start() {
	if (m_enabled) {
		return Error::invalid_operation("vision camera is already started");
	}

	if (m_filename.has_value()) {
		m_cap.open(*m_filename, cv::CAP_V4L2);
	} else {
		// cv::CAP_V4L2 is needed because by default it might use gstreamer, and because of a bug in opencv, this causes open to fail
		// if this is ever run not on linux, this will likely need to be changed
		m_cap.open(0, cv::CAP_V4L2);
		m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_cam_width);
		m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_cam_height);
		m_cap.set(cv::CAP_PROP_FPS, m_max_fps);
	}

	m_enabled = m_cap.isOpened();
	if (!m_enabled) {
		return Error::resource_unavailable("could not start vision camera");
	} else {
		return Error::ok();
	}
}

Error VisionCamera::stop() {
	if (m_enabled) {
		m_cap.release();
		m_enabled = false;
		return Error::ok();
	} else {
		return Error::invalid_operation("vision camera already stopped");
	}
}

Error VisionCamera::read_to(cv::Mat& mat) {
	if (m_enabled) {
		if (m_cap.read(mat)) {
			return Error::ok();
		} else {
			return Error::resource_unavailable("could not read next fram from camera");
		}
	} else {
		return Error::invalid_operation("can not read from vision camera if it is stopped");
	}
}


bool target_type_contains(TargetType input_type, TargetType contains_type) {
	return (input_type & contains_type) == contains_type;
}

std::optional<std::string_view> target_type_to_string(TargetType type) {
	switch (type) {
		case TargetType::BlueBall:
			return "blue_ball";
		case TargetType::RedBall:
			return "red_ball";
		case TargetType::None:
			return "none";
		default:
			return {};
	}
}


TargetSearchData::TargetSearchData(TargetType target_type, std::string&& in_name, std::string&& template_name, cv::Scalar thresh_min, cv::Scalar thresh_max, double min_score, ScoreWeights weights):
target_type(target_type),
name(std::move(in_name)),
template_name(std::move(template_name)),
thresh_min(thresh_min),
thresh_max(thresh_max),
min_score(min_score),
weights(weights) {
	threshold_name = name + " Threshold";
	morphology_name = name + " Morphology";
}

bool TargetSearchData::is(TargetType type) const {
	return target_type_contains(type, target_type);
}


IntermediateTarget::IntermediateTarget(std::vector<cv::Point>&& contour):
bounding_box(cv::boundingRect(contour)),
contour(std::move(contour)) {}


Vision::Vision(int threads, bool display):
m_threads(threads),
m_display(display) {}

Vision::~Vision() {}

void Vision::set_threads(int threads) {
	m_threads = threads;
}

Error Vision::process_templates(const std::string& template_directory) {
	for (auto& target_data : m_target_data) {
		auto template_file = std::string(template_directory) + "/" + target_data.template_name;
		auto img_template = cv::imread(template_file, -1);
		if (img_template.empty()) {
			return Error::resource_unavailable("could not open template file: " + template_file);
		}

		// TODO: find a way to configure what type of colorspace image is input
		cv::cvtColor(img_template, img_template, cv::COLOR_RGB2HSV);
		cv::inRange(img_template, target_data.thresh_min, target_data.thresh_max, img_template);
		// TODO: pass kernel into morphologyEx instead of plain cv::Mat()
		cv::morphologyEx(img_template, img_template, cv::MORPH_OPEN, cv::Mat());

		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(img_template, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

		if (contours.size() == 0) {
			lg::critical("could not find any contours in the template image");
		}

		show_wait(target_data.template_name, img_template);

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

		auto bounding_box = cv::boundingRect(target_data.template_contour);
		target_data.template_contour.swap(contours[index]);
		target_data.template_area_frac = max_area / bounding_box.area();
		target_data.template_aspect_ratio = (double) bounding_box.width / (double) bounding_box.height;
	}

	return Error::ok();
}

std::vector<Target> Vision::process(cv::Mat img, TargetType type) const {
	std::vector<Target> out;

	for (const auto& target_data : m_target_data) {
		if (!target_data.is(type)) {
		printf("hi2\n");
			continue;
		}

		cv::Size size(img.cols, img.rows);

		show("Input", img);

		// TODO: find a way to configure what type of colorspace image is input
		cv::Mat img_hsv(size, img.type());
		time("HSV conversion", [&] () {
			task(img, img_hsv, [] (cv::Mat in, cv::Mat out) {
				cv::cvtColor(in, out, cv::COLOR_BGR2HSV, 8);
			});
		});

		cv::Mat img_thresh(size, CV_8U);
		time("Threshold", [&] () {
			task(img_hsv, img_thresh, [&] (cv::Mat in, cv::Mat out) {
				cv::inRange(in, target_data.thresh_min, target_data.thresh_max, out);
			});
		});
		show(target_data.threshold_name, img_thresh);

		cv::Mat img_morph(size, CV_8U);
		// TODO: pass kernel into morphologyEx instead of plain cv::Mat()
		time("Morphology", [&] () {
			cv::morphologyEx(img_thresh, img_morph, cv::MORPH_OPEN, cv::Mat());
		});
		show(target_data.morphology_name, img_morph);

		// TODO: reserve eneough space in vector to prevent reallocations
		std::vector<std::vector<cv::Point>> contours;
		time("Contours", [&] () {
			cv::findContours(img_morph, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
		});

		// TODO: maybe thow out targets that score so low on a certain test
		std::vector<IntermediateTarget> targets;
		// move contours into target vector
		for (auto& contour : contours) {
			targets.push_back(IntermediateTarget(std::move(contour)));
		}

		time("Contour Matching", [&] () {
			for (auto& target : targets) {
				double score = cv::matchShapes(target_data.template_contour, target.contour, cv::CONTOURS_MATCH_I3, 0.0);
				target.score += score * target_data.weights.contour_match;
			}
		});

		time("Area Fraction", [&] () {
			for (auto& target : targets) {
				// compute fraction of bounding rectangle taken up by the actual contour
				double area_frac = cv::contourArea(target.contour) / target.bounding_box.area();
				// TODO: add a per target way to configure k
				double score = similarity(area_frac, target_data.template_area_frac, 70.0);
				target.score += score * target_data.weights.area_frac;
			}
		});

		time("Bounding Box Aspect Ratio", [&] {
			for (auto& target : targets) {
				// TODO figure out how to linearlize this
				double aspect_ratio = (double) target.bounding_box.width / (double) target.bounding_box.height;
			}
		});

		char text[32];
		int font_face = cv::FONT_HERSHEY_SIMPLEX;
		double font_scale = 0.5;
		cv::Point text_point(40, 40);

		/*if (best_match == INFINITY) {
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
		}*/

		for (auto& target : targets) {
			auto rect = target.bounding_box;
			/*Target target;
			target.distance = 11386.95362494479 * (1.0 / rect.width);
			auto xpos = rect.x + rect.width / 2;
			target.angle = atan((xpos - 320) / 530.47) * (180.0 / M_PI) + 16;*/
	
			if (m_display) {
				auto img_show = img.clone();
	
				// FIXME: find a better way to do this
				cv::drawContours(img_show, std::vector<std::vector<cv::Point>>(1, target.contour), 0, cv::Scalar(0, 0, 255));
				cv::rectangle(img_show, rect, cv::Scalar(0, 255, 0));
	
				snprintf(text, 32, "match: %6.2f", target.score);
				cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
				text_point.y += 15;
				/*snprintf(text, 32, "distance: %6.2f", target.distance);
				cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));
				text_point.y += 15;
				snprintf(text, 32, "angle: %6.2f", target.angle);
				cv::putText(img_show, text, text_point, font_face, font_scale, cv::Scalar(0, 0, 255));*/
	
				cv::imshow("Contours", img_show);
			}
		}
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

double Vision::similarity(double a, double b, double k) {
	// currently this is just a function I came up with by myself, I am not sure how good it is
	double squared_diff = pow((a - b), 2);

	return 1 / (1 + exp(k * squared_diff));
}
