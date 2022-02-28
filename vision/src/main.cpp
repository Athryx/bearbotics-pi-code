#include "types.h"
#include "argparse.hpp"
#include "vision.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <mosquitto.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>

int main(int argc, char **argv) {
	argparse::ArgumentParser program("vision", "0.1.0");
	
	program.add_argument("-d", "--display")
		.help("display processing frames")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("-m")
		.help("publish distance and angle to mqtt broker")
		.default_value(std::string {"localhost"});

	program.add_argument("-p", "--port")
		.help("use specified port to send mqtt data")
		.default_value(1883)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-t", "--topic")
		.help("mqtt topic to publish data to")
		.default_value(std::string {"PI/CV/SHOOT/DATA"});

	program.add_argument("-f", "--fps")
		.help("maximum frames per second")
		.default_value(120)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-w", "--width")
		.help("camera pixel width")
		.default_value(320)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-h", "--height")
		.help("camera pixel height")
		.default_value(240)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-t", "--threads")
		.help("amount of threads to use for parallel processing")
		.default_value(4)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-c", "--camera")
		.help("camera device file name to process, if no file name is given, use camera 0")
		.default_value(std::optional<std::string> {})
		.action([] (const std::string& str) -> std::optional<std::string> {
			return str;
		});

	program.add_argument("template")
		.help("template image file to process");

	try {
		program.parse_args (argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		exit(1);
	}

	const bool display_flag = program.get<bool>("-d");
	const long max_fps = program.get<int>("-f");
	const int cam_width = program.get<int>("-w");
	const int cam_height = program.get<int>("-h");
	const int threads = program.get<int>("-t");

	if (threads < 1) {
		printf("error: can't use less than 1 thread");
		exit(1);
	}
	cv::setNumThreads(threads);

	// TODO: maybe it is ugly to have a boolean and mqtt_client, maybe use an optional?
	const bool mqtt_flag = program.is_used("-m");
	const auto mqtt_topic = program.get("-t");
	// XXX: if mqtt_flag is set, this is guaranteed to be a valid pointer
	struct mosquitto *mqtt_client = nullptr;
	if (mqtt_flag) {
		auto host_name = program.get("-m");
		const int mqtt_port = program.get<int>("-p");
		auto client_name = std::string {"vision_"} + std::to_string(getpid());

		mosquitto_lib_init();
		mqtt_client = mosquitto_new(client_name.c_str(), true, nullptr);
		if (mqtt_client == nullptr) {
			printf("couldn't create MQTT client\n");
			exit(1);
		}

		// mosquitto_connect returns a non zero value on failure
		if (mosquitto_connect(mqtt_client, host_name.c_str(), mqtt_port, 60)) {
			printf("warning: could not connect to mqtt_host %s\n", host_name.c_str());
		}
	}

	cv::VideoCapture cap;
	auto file_name = program.get<std::optional<std::string>>("-c");
	if (file_name.has_value()) {
		cap.open(*file_name, cv::CAP_V4L2);
	} else {
		// cv::CAP_V4L2 is needed because by default it might use gstreamer, and because of a bug in opencv, this causes open to fail
		// if this is ever run not on linux, this will likely need to be changed
		cap.open(0, cv::CAP_V4L2);
		cap.set(cv::CAP_PROP_FRAME_WIDTH, cam_width);
		cap.set(cv::CAP_PROP_FRAME_HEIGHT, cam_height);
		cap.set(cv::CAP_PROP_FPS, max_fps);
	}

	if (!cap.isOpened()) {
		printf("error: could not open camera\n");
		exit(1);
	}


	auto template_file = program.get("template");
	auto template_img = cv::imread(template_file, -1);
	if (template_img.empty()) {
		printf("template file '%s' empty or missing\n", template_file.c_str());
		exit(1);
	}
	Vision vis(template_img, threads, display_flag);

	const usize msg_len = 32;
	char msg[msg_len];
	memset(msg, 0, msg_len);

	long total_time = 0;
	long frames = 0;

	for (;;) {
		cv::Mat frame;
		cap >> frame;
		if (frame.empty()) break;

		long elapsed_time;
		auto target = time<std::optional<Target>>("frame", [&] () {
			return vis.process(frame);
		}, &elapsed_time);

		total_time += elapsed_time;
		frames ++;

		printf("instantaneous fps: %ld\n", std::min(1000000 / elapsed_time, max_fps));
		printf("average fps: %ld\n", std::min(1000000 * frames / total_time, max_fps));

		printf("\n");

		if (mqtt_flag) {
			if (target.has_value()) {
				snprintf(msg, msg_len, "1 %6.2f %6.2f", target->distance, target->angle);
			}
			else {
				snprintf(msg, msg_len, "0 %6.2f %6.2f", 0.0f, 0.0f);
			}

			mosquitto_publish(mqtt_client, 0, mqtt_topic.c_str(), strlen(msg), msg, 0, false);
			int ret = mosquitto_loop(mqtt_client, 0, 1);
			printf("message sent: %s\n", msg);
			if (ret) {
				printf("connection lost, reconnecting...\n");
				mosquitto_reconnect(mqtt_client);
			}
		}

		// this is necessary to poll events for opencv highgui
		if (display_flag) cv::pollKey();
	}

	if (mqtt_flag) {
		mosquitto_destroy(mqtt_client);
		mosquitto_lib_cleanup();
	}
}
