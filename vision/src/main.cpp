#include "types.h"
#include "argparse.hpp"
#include "vision.h"
#include "remote_viewing.h"
#include "util.h"
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <iostream>

enum class Mode {
	Vision,
	RemoteViewing
};

enum class Team {
	Red,
	Blue
};

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


	program.add_argument("--rtp-host")
		.help("host to send the remote viewing data to")
		.default_value(std::string {"localhost"});

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
		.help("mqtt topic to recieve commands from to switch modes between remote viewing and vision")
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
struct MqttData {
	Mode mode;
	Team team;
	// is none under normal circumstances, sets to Some(old mode) after mode change
	std::optional<Mode> old_mode {};
	const std::string& control_topic;
};

// TODO: be able to set team
void mqtt_message_callback(mosquitto *mosq, void *data_in, const mosquitto_message *msg) {
	MqttData *data = (MqttData *) data_in;

	bool matches = false;
	mosquitto_topic_matches_sub(data->control_topic.c_str(), msg->topic, &matches);

	if(matches) {
		const char *vstr = "vision";
		const char *rstr = "remote-viewing";
		usize vlen = strlen(vstr);
		usize rlen = strlen(rstr);

		Mode new_mode;
		if (vlen == msg->payloadlen && strncmp(vstr, (char *) msg->payload, msg->payloadlen) == 0) {
			new_mode = Mode::Vision;
		} else if (rlen == msg->payloadlen && strncmp(rstr, (char *) msg->payload, msg->payloadlen) == 0) {
			new_mode = Mode::RemoteViewing;
		}

		if (new_mode != data->mode) {
			data->old_mode = data->mode;
			data->mode = new_mode;
		}
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
		printf("error: can't use less than 1 thread");
		exit(1);
	}
	cv::setNumThreads(threads);


	// TODO: maybe it is ugly to have a boolean and mqtt_client, maybe use an optional?
	const bool mqtt_flag = program.is_used("--mqtt");
	const auto mqtt_topic = program.get("--topic");
	const auto mqtt_control_topic = program.get("--control-topic");

	auto mqtt_data = MqttData {
		.mode = program.get<Mode>("--remote-viewing"),
		.team = program.get<Team>("--team"),
		.control_topic = mqtt_control_topic
	};

	// XXX: if mqtt_flag is set, this is guaranteed to be a valid pointer
	mosquitto *mqtt_client = nullptr;
	if (mqtt_flag) {
		auto host_name = program.get("--mqtt");
		const int mqtt_port = program.get<int>("--port");
		auto client_name = std::string {"vision_"} + std::to_string(getpid());

		mosquitto_lib_init();
		mqtt_client = mosquitto_new(client_name.c_str(), true, &mqtt_data);
		if (mqtt_client == nullptr) {
			printf("couldn't create MQTT client\n");
			exit(1);
		}

		mosquitto_message_callback_set(mqtt_client, mqtt_message_callback);

		// mosquitto_connect returns a non zero value on failure
		if (mosquitto_connect(mqtt_client, host_name.c_str(), mqtt_port, 60)) {
			printf("warning: could not connect to mqtt_host %s\n", host_name.c_str());
		}

		if (mosquitto_subscribe(mqtt_client, nullptr, mqtt_control_topic.c_str(), 0)) {
			printf("warning: could not subscribe to mqtt control topic %s\n", mqtt_control_topic.c_str());
		}
	}


	const auto rtp_host = program.get("--rtp-host");
	const auto rtp_port = program.get<int>("--rtp-port");
	RemoteViewing remote_viewing(rtp_host, rtp_port);


	auto file_name = program.get<std::optional<std::string>>("--camera");
	VisionCamera camera(std::move(file_name), cam_width, cam_height, max_fps);

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

	// enable device for initial mode
	switch (mqtt_data.mode) {
		case Mode::Vision:
			// TODO: handle when this fails
			camera.start();
			break;
		case Mode::RemoteViewing:
			// TODO: handle when this fails
			remote_viewing.stop();
			break;
	}

	// TODO: don't constantly loop
	for(;;) {
		// check if mode has changed
		if (mqtt_data.old_mode.has_value()) {
			Mode old_mode = *mqtt_data.old_mode;
			mqtt_data.old_mode = {};

			switch (old_mode) {
				case Mode::Vision:
					camera.stop();
					break;
				case Mode::RemoteViewing:
					// TODO: handle when this fails
					remote_viewing.stop();
					break;
			}

			switch (mqtt_data.mode) {
				case Mode::Vision:
					// TODO: handle when this fails
					camera.start();
					break;
				case Mode::RemoteViewing:
					// TODO: handle when this fails
					remote_viewing.stop();
					break;
			}
		}

		switch (mqtt_data.mode) {
			case Mode::Vision: {
				cv::Mat frame;
				camera.read_to(frame);
				if (frame.empty()) {
					printf("warning: empty frame recieved, skipping vision processing\n");
					continue;
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

					mosquitto_publish(mqtt_client, 0, mqtt_topic.c_str(), strlen(msg), msg, 0, false);
					int ret = mosquitto_loop(mqtt_client, 0, 1);
					printf("message sent: %s\n", msg);
					if (ret) {
						printf("connection lost, reconnecting...\n");
						mosquitto_reconnect(mqtt_client);
					}
				}
				break;
			}
			case Mode::RemoteViewing: {
				// TODO: handle remote viewing errors here
				break;
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
