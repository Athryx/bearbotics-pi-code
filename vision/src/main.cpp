#include "mqtt.h"
#include "types.h"
#include "argparse.hpp"
#include "vision.h"
#include "remote_viewing.h"
#include "util.h"
#include "logging.h"
#include "error.h"
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <algorithm>
#include <iostream>

enum class Mode {
	Vision,
	RemoteViewing,
	None,
};

enum class Team {
	Red,
	Blue,
};

// TODO: modify argparse to allow overiding default represented value string
argparse::ArgumentParser parse_args(int argc, char **argv) {
	argparse::ArgumentParser program("vision", "0.1.0");

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
	program.add_argument("--rtp-host")
		.help("host to send the remote viewing data to")
		.default_value(std::string {"127.0.0.1"});

	program.add_argument("--rtp-port")
		.help("port to send the remote viewing data to")
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

	program.add_argument("--team")
		.help("what team to recognise balls for, default is red")
		.default_value(Team::Red)
		.action([] (const std::string& str) {
			if (str == "red") {
				return Team::Red;
			} else if (str == "blue") {
				return Team::Blue;
			} else {
				throw std::runtime_error("invalid argument for --team: must be either 'red' or 'blue'");
			}
		});


	// TODO: make these settings apply to remote viewing or add seperate settings fro remote viewing
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

	program.add_argument("template")
		.help("template image file to process");


	try {
		program.parse_args (argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		exit(1);
	}

	return program;
}

// passed to mqtt message callback
// FIXME: improve this, it is currently used wrong
class AppState {
	public:
		// sets old mode to none to force mode init to be initially run
		AppState(Mode mode, Team team):
		m_mode(mode),
		m_team(team),
		m_old_mode(Mode::None) {}

		Mode mode() const { return m_mode; }
		Team team() const { return m_team; }

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

		void set_team(Team team) {
			m_team = team;
		}

	private:
		Mode m_mode;
		Team m_team;
		// this will be none under normal circumstances, and set to Some(old mode) after mode change
		std::optional<Mode> m_old_mode;
};

// TODO: be able to set team
void mqtt_control_callback(std::string_view msg, AppState *data) {
	if (msg == "vision") {
		data->set_mode(Mode::Vision);
	} else if (msg == "remote-viewing") {
		data->set_mode(Mode::RemoteViewing);
	}
}

int main(int argc, char **argv) {
	auto program = parse_args(argc, argv);

	gst_init(&argc, &argv);

	const bool display_flag = program.get<bool>("--display");
	const long max_fps = program.get<int>("--fps");
	const int cam_width = program.get<int>("--width");
	const int cam_height = program.get<int>("--height");
	const int threads = program.get<int>("--threads");

	if (threads < 1) {
		lg::critical("error: can't use less than 1 thread");
	}
	cv::setNumThreads(threads);


	// TODO: maybe it is ugly to have a boolean and mqtt_client, maybe use an optional?
	const bool mqtt_flag = program.is_used("--mqtt");
	const auto mqtt_topic = program.get("--topic");
	const auto mqtt_control_topic = program.get("--control-topic");
	const auto mqtt_error_topic = program.get("--error-topic");

	AppState app_state(program.get<Mode>("--remote-viewing"), program.get<Team>("--team"));

	std::optional<MqttClient> mqtt_client {};
	if (mqtt_flag) {
		mosquitto_lib_init();

		auto host_name = program.get("--mqtt");
		const int mqtt_port = program.get<int>("--port");

		mqtt_client = MqttClient::create(host_name, mqtt_port);

		if (!mqtt_client.has_value()) {
			lg::critical("could not create MqttClient");
		}

		if (!mqtt_client->subscribe(mqtt_control_topic, mqtt_control_callback, &app_state)) {
			lg::warn("could not subscribe to mqtt control topic %s", mqtt_control_topic.c_str());
		}
	}

	// helper closure to report errors
	auto report_error = [&](const Error& error) {
		lg::error("%s", error.to_string().c_str());
		if (mqtt_flag) {
			auto result = mqtt_client->publish(mqtt_error_topic, error.serialize());
			if (result.is_err()) {
				lg::error("error sending error over mqtt: %s", result.to_string().c_str());
			}
		}
	};


	const auto rtp_host = program.get("--rtp-host");
	const auto rtp_port = program.get<int>("--rtp-port");
	RemoteViewing remote_viewing(rtp_host, rtp_port, cam_width, cam_height);


	auto file_name = program.get<std::optional<std::string>>("--camera");
	VisionCamera camera(std::move(file_name), cam_width, cam_height, max_fps);

	auto template_file = program.get("template");
	auto template_img = cv::imread(template_file, -1);
	if (template_img.empty()) {
		lg::critical("template file '%s' empty or missing", template_file.c_str());
	}
	Vision vis(template_img, threads, display_flag);


	const usize msg_len = 32;
	char msg[msg_len];
	memset(msg, 0, msg_len);

	long total_time = 0;
	long frames = 0;

	// TODO: don't constantly loop
	for(;;) {
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
						report_error(result);
					}
					break;
				}
				case Mode::RemoteViewing: {
					auto result = remote_viewing.start();
					if (result.is_err()) {
						app_state.set_mode(Mode::None);
						report_error(result);
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
						report_error(result);
					}
				}

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
					} else {
						snprintf(msg, msg_len, "0 %6.2f %6.2f", 0.0f, 0.0f);
					}

					// TODO: reduce amount of allocations for string
					auto result = mqtt_client->publish(mqtt_topic, std::string(msg));
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
					report_error(result);
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
	}

	if (mqtt_flag) {
		// FIXME: MqttClient is dropped after this is called
		mosquitto_lib_cleanup();
	}
}
