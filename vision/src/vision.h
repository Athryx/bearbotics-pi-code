#pragma once

#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <vector>
#include <functional>
#include "error.h"
#include "types.h"

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

// bitflags for type of target
enum class TargetType: u64 {
	None = 0x0,
	RedBall = 0x1,
	BlueBall = 0x2,
	All = 0x3,
};

// operations for the TargetType bitflags
inline TargetType operator|(TargetType lhs, TargetType rhs) {
	using T = std::underlying_type_t <TargetType>;
	return static_cast<TargetType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline TargetType& operator|=(TargetType& lhs, TargetType rhs) {
	lhs = lhs | rhs;
	return lhs;
}

inline TargetType operator&(TargetType lhs, TargetType rhs) {
	using T = std::underlying_type_t <TargetType>;
	return static_cast<TargetType>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

inline TargetType& operator&=(TargetType& lhs, TargetType rhs) {
	lhs = lhs & rhs;
	return lhs;
}

// returns true if the left hand side contains all the bits from the right hand side
bool target_type_contains(TargetType input_type, TargetType contains_type);

// returns a string view if the type is only one type, if it is mixed returns none
std::optional<std::string_view> target_type_to_string(TargetType type);

// represents a detected target
struct Target {
	TargetType type;
	double distance;
	double angle;
	double score;
};

struct IntermediateTarget {
	IntermediateTarget(std::vector<cv::Point>&& contour);

	cv::Rect bounding_box;
	std::vector<cv::Point> contour;
	double score { 0.0 };
};

// weights for the scores of different operations to determine how closely the imsage matches the template
struct ScoreWeights {
	double contour_match;
	double area_frac;
};

// data about a desired type of target used to help recognise it
class TargetSearchData {
	public:
		TargetSearchData(TargetType target_type, std::string&& template_name, cv::Scalar thresh_min, cv::Scalar thresh_max, double min_score, ScoreWeights weights);

		// returns true if this is the passed in target type
		bool is(TargetType type) const;

		const std::string& get_name() const;

		// type of target that this is
		TargetType target_type;

		// name of the template file
		// will try and open the template file in the passed in template directory
		std::string template_name;

		// minimum and maximum hsv values for threshholding this object
		// from https://docs.opencv.org/3.4/de/d25/imgproc_color_conversions.html
		// NOTE: if input image is 8 bit rgb, the min and max values for h, s, and v area as follows:
		// H: 180
		// S: 255
		// V: 255
		// This is different from the normal H: 360, S: 100, V: 100 readings you will normally see,
		// so be sure to scale these values to be in the correct range
		cv::Scalar thresh_min;
		cv::Scalar thresh_max;

		// minumum score a controut must have to be considered a valid target
		double min_score;

		ScoreWeights weights;

		// template contour to try and recognise
		std::vector<cv::Point> template_contour {};
		double template_area_frac { 0.0 };
		double template_aspect_ratio { 0.0 };
	
	private:
		std::string m_name {};
};

// TODO: use correct display names for when display flag is true
class Vision {
	public:
		Vision(int threads, bool display);
		~Vision();

		void set_threads(int threads);

		// processess all templates to get required paramaters to look for targets
		// returns error if any of the files cannot be opened
		Error process_templates(const std::string& template_directory);

		// processess the image to find targets
		// pass in targets bitflags to say which targets we can look for
		std::vector<Target> process(cv::Mat img, TargetType targets) const;

	private:
		void show(const std::string& name, cv::Mat& img) const;
		void show_wait(const std::string& name, cv::Mat& img) const;
		void task(cv::Mat in, cv::Mat out, std::function<void(cv::Mat, cv::Mat)> func) const;

		// returns a score from 0 to 1 of how similar 2 numbers are to eachother
		// the k value determines how fast the returned score falls off
		// the higher it is, the faster it falls off
		static double similarity(double a, double b, double k = 1.0);

		// how man threads to use for processing certain operations in parallell
		int m_threads;
		// true to display the frames for debugging
		bool m_display;

		// change this to change which targets we can look for
		// maybe when vision is mature these can be read in from a file
		std::vector<TargetSearchData> m_target_data {
			TargetSearchData(
				TargetType::RedBall,
				"red-ball-template.png",
				// observed min: H: 324.7, S: 47.9, V: 61.3
				// observed max: H: 336.5, S: 69.1, V: 90.7
				//cv::Scalar(162, 122, 156),
				//cv::Scalar(169, 177, 232),
				cv::Scalar(130, 122, 156),
				cv::Scalar(140, 177, 232),
				1.5,
				ScoreWeights {
					.contour_match = 1.0,
					.area_frac = 1.0,
				}
			),
			TargetSearchData(
				TargetType::BlueBall,
				"blue-ball-template.png",
				// observed min: H: 201.9 S: 69.2 V: 64.6
				// observed max: H: 213.3, S: 82.2, V: 96.2
				cv::Scalar(100, 176, 164),
				cv::Scalar(107, 210, 246),
				1.5,
				ScoreWeights {
					.contour_match = 1.0,
					.area_frac = 1.0,
				}
			),
		};
};
