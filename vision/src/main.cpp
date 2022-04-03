#include "mqtt.h"
#include "types.h"
#include "argparse.hpp"
#include "vision.h"
#include "remote_viewing.h"
#include "util.h"
#include "logging.h"
#include "error.h"
#include <cstdlib>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>

enum class Mode {
	Vision,
	RemoteViewing,
	None,
};

const char* mode_to_string(Mode mode) {
	switch(mode) {
		case Mode::Vision:
			return "vision";
		case Mode::RemoteViewing:
			return "remote_viewing";
		case Mode::None:
			return "none";
	}
	// stop compiler warning
	return "";
}

// TODO: modify argparse to allow overiding default represented value string
argparse::ArgumentParser parse_args(int argc, char **argv) {
	argparse::ArgumentParser program("vision", "0.1.0");

	// TODO: do this to parse numbers for all other options
	program.add_argument("-l", "--log-level")
		.help("how much to log: 0: nothing, 1: ciritical, 2: errors, 3: warnings, 4: info")
		.default_value(4)
		.action([] (const std::string& str) {
			int level;
			std::istringstream iss(str);

			if ((iss >> level).fail() || !(iss >> std::ws).eof()) {
				throw std::runtime_error("must pass number to --log-level");
			}

			if (level < 0 || level > 4) {
				throw std::runtime_error("invalid logging level");
			}
			return level;
		});

	program.add_argument("-m", "--mqtt")
		.help("publish distance and angle to mqtt broker")
		.default_value(std::string {"localhost"});

	program.add_argument("-p", "--port")
		.help("use specified port to send mqtt data")
		.default_value(1883)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});


	// for some reason localhost doesn't work here
	program.add_argument("--rtsp-uri")
		.help("rtsp uri to make remote viewing camera available on")
		.default_value(std::string {"/remote-viewing"});

	program.add_argument("--rtsp-port")
		.help("port for rtsp server to send data on")
		.default_value(5000)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});


	program.add_argument("-t", "--topic")
		.help("mqtt topic to publish data to")
		.default_value(std::string {"pi/cv/data"});

	program.add_argument("-c", "--control-topic")
		.help("mqtt topic to recieve commands from to switch modes between remote viewing and vision, or to switch teams")
		.default_value(std::string {"pi/cv/control"});

	program.add_argument("-e", "--error-topic")
		.help("mqtt topic to send error information on")
		.default_value(std::string {"pi/cv/error"});


	program.add_argument("-r", "--remote-viewing")
		.help("mode to start in, by default it is vision, unless this flag is specified, than it starts in remote viewing")
		.default_value(Mode::Vision)
		.implicit_value(Mode::RemoteViewing);

	program.add_argument("--target-type")
		.help("what type of target to look for, default is all")
		.default_value(TargetType::All)
		.action([] (const std::string& str) {
			if (str == "red-ball") {
				return TargetType::RedBall;
			} else if (str == "blue-ball") {
				return TargetType::BlueBall;
			} else if (str == "all") {
				return TargetType::All;
			} else {
				throw std::runtime_error("invalid argument for --team: must be either 'red-ball', 'blue-ball', or 'all'");
			}
		});


	program.add_argument("-f", "--fps")
		.help("maximum frames per second")
		.default_value(120)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-w", "--image-width")
		.help("pixel width of image put through computer vision and remote viewing")
		.default_value(320)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("-h", "--image-height")
		.help("pixel height of image put through computer vision and remote viewing")
		.default_value(240)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	// TODO: make this apply to computer vision
	program.add_argument("--cam-width")
		.help("pixel width of image read in from camera, it is downscaled or upscaled to match the --image-width argument, by default it is the same as --image-height")
		.default_value(320)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("--cam-height")
		.help("pixel height of image read on from camera, it is downscaled or upscaled to match the --image-height argument, by default it is the same as --image-height")
		.default_value(240)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});

	program.add_argument("--fov")
		.help("horizantal field of view of the camera. This camera is not set to the passed in fov, but the passed in fov is used to calculate distance and angle")
		// I think this is the default value for the pi v2 camera
		.default_value(47.0)
		.action([] (const std::string& str) {
			return std::atof(str.c_str());
		});


	program.add_argument("-d", "--display")
		.help("display processing frames")
		.default_value(false)
		.implicit_value(true);


	program.add_argument("-t", "--threads")
		.help("amount of threads to use for parallel processing")
		.default_value(4)
		.action([] (const std::string& str) {
			return std::atoi(str.c_str());
		});


	program.add_argument("-a", "--camera")
		.help("camera device file name to process, if no file name is given, use camera 0")
		.default_value(std::optional<std::string> {})
		.action([] (const std::string& str) -> std::optional<std::string> {
			return str;
		});

	program.add_argument("template-dir")
		.help("template directory containing all template files, which must be 8 bits per channel rgb images");


	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		exit(1);
	}

	return program;
}

class AppState {
	public:
		// sets old mode to none to force mode init to be initially run
		AppState(Mode mode, TargetType targets):
		m_mode(mode),
		m_targets(targets),
		m_old_mode(Mode::None) {}

		Mode mode() const { return m_mode; }
		TargetType targets() const { return m_targets; }

		void set_mode(Mode new_mode) {
			if (new_mode != m_mode) {
				m_old_mode = m_mode;
				m_mode = new_mode;
			}
		}

		std::optional<Mode> old_mode() const {
			return m_old_mode;
		}

		bool has_mode_changed() const {
			return m_old_mode.has_value();
		}

		void mod_change_complete() {
			m_old_mode = {};
		}

		void set_targets(TargetType targets) {
			m_targets = targets;
		}

	private:
		Mode m_mode;
		TargetType m_targets;
		// this will be none under normal circumstances, and set to Some(old mode) after mode change
		std::optional<Mode> m_old_mode;
};

void mqtt_control_callback(std::string_view msg, AppState *data) {
	// this is kind of jank command processor, needs to be improved for more complex commands if they are ever added
	if (msg == "mode vision") {
		data->set_mode(Mode::Vision);
	} else if (msg == "mode remote_viewing") {
		data->set_mode(Mode::RemoteViewing);
	} else if (msg == "mode none") {
		data->set_mode(Mode::None);
	} else if (msg == "targets red_balls") {
		data->set_targets(TargetType::RedBall);
	} else if (msg == "targets blue_balls") {
		data->set_targets(TargetType::BlueBall);
	} else if (msg == "targets all") {
		data->set_targets(TargetType::All);
	} else {
		lg::warn("recieved invalid control message");
	}
}

int main(int argc, char **argv) {
	auto program = parse_args(argc, argv);

	lg::init(program.get<int>("--log-level"));
	gst_init(&argc, &argv);

	const bool display_flag = program.get<bool>("--display");
	const long max_fps = program.get<int>("--fps");
	// the amount of time that will pass between each fram with the given framerate
	std::chrono::milliseconds frame_interval(1000 / max_fps);
	const int image_width = program.get<int>("--image-width");
	const int image_height = program.get<int>("--image-height");
	const int cam_width = program.is_used("--cam-width") ? program.get<int>("--cam-width") : image_width;
	const int cam_height = program.is_used("--cam-height") ? program.get<int>("--cam-height") : image_height;
	const double fov = program.get<double>("--fov");
	const int threads = program.get<int>("--threads");

	if (threads < 1) {
		lg::critical("error: can't use less than 1 thread");
	}
	cv::setNumThreads(threads);


	const bool mqtt_flag = program.is_used("--mqtt");
	const auto mqtt_topic = program.get("--topic");
	const auto mqtt_control_topic = program.get("--control-topic");
	const auto mqtt_error_topic = program.get("--error-topic");

	AppState app_state(program.get<Mode>("--remote-viewing"), program.get<TargetType>("--target-type"));

	std::optional<MqttClient> mqtt_client {};
	if (mqtt_flag) {
		mosquitto_lib_init();

		auto host_name = program.get("--mqtt");
		const int mqtt_port = program.get<int>("--port");

		mqtt_client = MqttClient::create(host_name, mqtt_port);

		if (!mqtt_client.has_value()) {
			lg::critical("could not create MqttClient");
		}

		if (mqtt_client->subscribe(mqtt_control_topic, mqtt_control_callback, &app_state).is_err()) {
			lg::warn("could not subscribe to mqtt control topic %s", mqtt_control_topic.c_str());
		}
	}

	// helper closure to report errors
	auto report_error = [&](const Error& error) {
		lg::error("%s", error.to_string().c_str());
		if (mqtt_flag) {
			std::string msg = ";" + error.serialize();
			auto result = mqtt_client->publish(mqtt_error_topic, msg);
			if (result.is_err()) {
				lg::error("error sending error message over mqtt: %s", msg.c_str());
			}
		}
	};

	// reports a mode change and an error
	auto report_mode_change = [&](Mode mode, const Error& reason) {
		auto mode_str = mode_to_string(mode);
		lg::error("changing to mode %s because %s", mode_str, reason.to_string().c_str());
		if (mqtt_flag) {
			std::string msg = std::string(mode_str) + ";" + reason.serialize();
			auto result = mqtt_client->publish(mqtt_error_topic, msg);
			if (result.is_err()) {
				lg::error("error sending error message over mqtt: %s", msg.c_str());
			}
		}
	};


	const auto rtsp_uri = program.get("--rtsp-uri");
	const auto rtsp_port = program.get<int>("--rtsp-port");
	RemoteViewing remote_viewing(rtsp_port, rtsp_uri, cam_width, cam_height, image_width, image_height, max_fps);


	auto file_name = program.get<std::optional<std::string>>("--camera");
	VisionCamera camera(std::move(file_name), image_width, image_height, max_fps);

	Vision vis(fov, threads, display_flag);
	auto template_dir = program.get("template-dir");
	auto template_res = vis.process_templates(template_dir);
	if (template_res.is_err()) {
		lg::critical("%s", template_res.to_string().c_str());
	}


	constexpr usize msg_buf_len = 2048;
	char msg_buf[msg_buf_len];
	memset(msg_buf, 0, msg_buf_len);

	long total_time = 0;
	long frames = 0;

	for(;;) {
		// the time we will need to wake up for next frame
		auto next_frame_time = std::chrono::steady_clock::now() + frame_interval;

		// check if mode has changed
		if (app_state.has_mode_changed()) {

			// if there is an error when stopping cameras, it is not as important, so just emit a warning, don't tell rio or change state
			switch (*app_state.old_mode()) {
				case Mode::Vision: {
					auto result = camera.stop();
					if (result.is_err()) {
						lg::warn("%s", result.to_string().c_str());
					}
					break;
				}
				case Mode::RemoteViewing: {
					auto result = remote_viewing.stop();
					if (result.is_err()) {
						lg::warn("%s", result.to_string().c_str());
					}
					break;
				}
				case Mode::None:
					break;
			}

			switch (app_state.mode()) {
				case Mode::Vision: {
					auto result = camera.start();
					if (result.is_err()) {
						app_state.set_mode(Mode::None);
						report_mode_change(Mode::None, result);
					}
					break;
				}
				case Mode::RemoteViewing: {
					auto result = remote_viewing.start();
					if (result.is_err()) {
						app_state.set_mode(Mode::None);
						report_mode_change(Mode::None, result);
					}
					break;
				}
				case Mode::None:
					break;
			}

			// this will clear the changing mode state that may occur when switching mode to none if the mode init fails
			// this will stop the app from trying to close the cameras
			app_state.mod_change_complete();
		}

		switch (app_state.mode()) {
			case Mode::Vision: {
				cv::Mat frame;
				auto result = camera.read_to(frame);
				if (result.is_err()) {
					if (result.is(ErrorType::ResourceUnavailable)) {
						lg::warn("could not read frame from camera, skipping vision processing");
						continue;
					} else {
						// some other error has occured, don't do vision anymore
						app_state.set_mode(Mode::None);
						report_mode_change(Mode::None, result);
					}
				}

				long elapsed_time;
				auto targets = time<std::vector<Target>>("frame", [&] () {
					return vis.process(frame, app_state.targets());
				}, &elapsed_time);

				total_time += elapsed_time;
				frames ++;

				lg::info("instantaneous fps: %ld", std::min(1000000 / elapsed_time, max_fps));
				lg::info("average fps: %ld\n", std::min(1000000 * frames / total_time, max_fps));

				if (mqtt_flag) {
					// true if serialization succeeded
					bool serialize_good = true;

					usize i = 0;
					for (auto& target : targets) {
						char *ptr = msg_buf + i;
						usize n = msg_buf_len - i;
						int result = snprintf(ptr, n, "%d %f %f %f", (int) target.type, target.distance, target.angle, target.score);
						if (result < 0 || result >= n) {
							serialize_good = false;
							lg::error("targets could not fit in mqtt send buffer, skipping sending data");
							break;
						}
					}

					auto result = mqtt_client->publish(mqtt_topic, std::string(msg_buf));
					if (result.is_err()) {
						lg::error("could not publish vision data to mqtt: %s", result.to_string().c_str());
					}
				}
				break;
			}
			case Mode::RemoteViewing: {
				auto result = remote_viewing.update();
				if (result.is_err()) {
					app_state.set_mode(Mode::None);
					report_mode_change(Mode::None, result);
				}
				break;
			}
			case Mode::None:
				break;
		}

		if (mqtt_flag) {
			auto result = mqtt_client->update();
			if (result.is_err()) {
				lg::error("error updating mqtt client: %s", result.to_string().c_str());
			}
		}

		// this is necessary to poll events for opencv highgui
		if (display_flag) cv::pollKey();

		// sleep until next frame occurs, or just continue looping if it is already ready
		std::this_thread::sleep_until(next_frame_time);
	}

	if (mqtt_flag) {
		// FIXME: MqttClient is dropped after this is called which might cuase problems
		mosquitto_lib_cleanup();
	}
}
