#include "types.h"
#include "vision.h"
#include "util.h"
#include "parallel.h"
#include "logging.h"
#include <cmath>
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
		// if this is ever run not on linux, this will need to be changed
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


TargetSearchData::TargetSearchData(TargetType target_type, std::string&& in_name, cv::Scalar bounding_box_color, std::string&& template_name, PipelineParams params, double min_score, ScoreWeights weights):
target_type(target_type),
name(std::move(in_name)),
bounding_box_color(bounding_box_color),
template_name(std::move(template_name)),
params(params),
min_score(min_score),
weights(weights) {
	hsv_name = name + " HSV Conversion";
	threshold_name = name + " Threshold";
	morphology_name = name + " Morphology";
	contour_name = name + " Contours";
	matching_name = name + " Contour Matching";
}

bool TargetSearchData::is(TargetType type) const {
	return target_type_contains(type, target_type);
}


IntermediateTarget::IntermediateTarget(std::vector<cv::Point>&& contour):
bounding_box(cv::boundingRect(contour)),
contour(std::move(contour)) {}


Vision::Vision(double fov, int threads, bool display):
m_fov(fov),
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
		cv::inRange(img_template, target_data.params.thresh_min, target_data.params.thresh_max, img_template);
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
		target_data.template_aspect_ratio_scaled = log2((double) bounding_box.width / (double) bounding_box.height);
	}

	return Error::ok();
}

std::vector<Target> Vision::process(cv::Mat img, TargetType type) const {
	std::vector<Target> out;

	// image that will be used to show all found targets of all types
	cv::Mat img_show;
	if (m_display) {
		// only copy the data if display flag is set
		img_show = img.clone();
	}

	show("Input", img);

	// values used for distance calulation that only need to be calculated once
	// TODO: don't calculate these every frame
	double img_width = img.size().width;
	double img_height = img.size().height;

	double fovx = m_fov;
	double fovy = fovx * img_width / img_height;

	// slope of field of view line
	double fov_slope = tan(fovy);
	// height of the entire camera frame at a distance of 1m
	double frame_height = 2.0 * fov_slope;

	for (const auto& target_data : m_target_data) {
		if (!target_data.is(type)) {
			continue;
		}

		cv::Size size(img.cols, img.rows);

		// TODO: find a way to configure what type of colorspace image is input
		cv::Mat img_hsv(size, img.type());
		time(target_data.hsv_name.c_str(), [&] () {
			task(img, img_hsv, [] (cv::Mat in, cv::Mat out) {
				cv::cvtColor(in, out, cv::COLOR_BGR2HSV, 8);
			});
		});

		cv::Mat img_thresh(size, CV_8U);
		time(target_data.threshold_name.c_str(), [&] () {
			task(img_hsv, img_thresh, [&] (cv::Mat in, cv::Mat out) {
				cv::inRange(in, target_data.params.thresh_min, target_data.params.thresh_max, out);
			});
		});
		show(target_data.threshold_name, img_thresh);

		cv::Mat img_morph(size, CV_8U);
		// TODO: pass kernel into morphologyEx instead of plain cv::Mat()
		time(target_data.morphology_name.c_str(), [&] () {
			cv::morphologyEx(img_thresh, img_morph, cv::MORPH_OPEN, cv::Mat());
		});
		show(target_data.morphology_name, img_morph);

		// TODO: reserve eneough space in vector to prevent reallocations
		std::vector<std::vector<cv::Point>> contours;
		time(target_data.contour_name.c_str(), [&] () {
			cv::findContours(img_morph, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
		});

		// TODO: maybe thow out targets that score so low on a certain test
		std::vector<IntermediateTarget> targets;
		// move contours into target vector
		for (auto& contour : contours) {
			targets.push_back(IntermediateTarget(std::move(contour)));
		}

		time(target_data.matching_name.c_str(), [&] () {
			for (auto& target : targets) {
				// contour matching with cv::matchShapes
				double match_shapes_score = cv::matchShapes(target_data.template_contour, target.contour, cv::CONTOURS_MATCH_I3, 0.0);
				target.score += match_shapes_score * target_data.weights.contour_match;

				// compute fraction of bounding rectangle taken up by the actual contour
				double area_frac = cv::contourArea(target.contour) / target.bounding_box.area();
				// TODO: add a per target way to configure k
				double frac_score = similarity(area_frac, target_data.template_area_frac, 70.0);
				target.score += frac_score * target_data.weights.area_frac;

				double aspect_ratio = (double) target.bounding_box.width / (double) target.bounding_box.height;
				// make it so that doubling the aspec ratio results in a constant increase in score
				double scaled_ratio = std::log2(aspect_ratio);
				double aspect_score = similarity(scaled_ratio, target_data.template_aspect_ratio_scaled, 100.0);
				target.score += aspect_ratio * target_data.weights.aspect_ratio;
			}
		});

		char text[32];
		int font_face = cv::FONT_HERSHEY_SIMPLEX;
		double font_scale = 0.5;
		cv::Point text_point(40, 40);

		for (const auto& target : targets) {
			// ignore targets that didn't score well enough
			if (target.score < target_data.min_score) {
				continue;
			}

			auto rect = target.bounding_box;
			double xpos = rect.x + rect.width / 2.0;
			// TODO: figure out if top is 0 y or bottom is 0 y
			double ypos = rect.y + rect.height / 2.0;

			// converts xpos and ypos to be in the range -1.0 to 1.0
			double xpos_normalized = (xpos - img_width / 2.0) * (2.0 / img_width);
			double ypos_normalized = (ypos - img_height / 2.0) * (2.0 / img_height);

			// calculate angle of target in degrees
			double xangle = xpos_normalized * fovx;
			double yangle = ypos_normalized * fovy;

			// calculate expected height of target at 1m distance based on target_height and fovy
			// fraction of the frame the target would take up at 1m distance
			double height_fraction_1m = target_data.params.target_height / frame_height;
			// the actual fraction of the frame that the object's height takes up
			double height_fraction = rect.height / frame_height;
			double distance = height_fraction_1m / height_fraction;

			// TODO: account for camera position and angle
			auto out_target = Target {
				.type = target_data.target_type,
				.distance = distance,
				.angle = xangle,
				.score = target.score,
			};

			out.push_back(out_target);

			if (m_display) {
				// TODO: display distance, angle, and score for each target
				cv::drawContours(img_show, std::vector<std::vector<cv::Point>>(1, target.contour), 0, cv::Scalar(0, 0, 255));
				cv::rectangle(img_show, rect, target_data.bounding_box_color);
			}
		}
	}

	show("Targets", img_show);

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
