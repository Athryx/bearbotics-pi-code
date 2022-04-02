#pragma once

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <string>
#include <thread>
#include <optional>
#include "types.h"
#include "error.h"

class RemoteViewing {
	public:
		// TODO: error propagation with constructor
		RemoteViewing(u16 port, const std::string& rtsp_uri, int input_width, int input_height, int processing_width, int processing_height, int fps);
		~RemoteViewing();

		// sets the pipeline to null state on stop, so it does not have the camera open, so the vision code can read from the camera
		Error start();
		Error stop();

		// TODO: move logging from here into main.cpp
		Error update();

	private:
		// thread that runs the g main loop
		std::optional<std::thread> m_loop_runner_thread {};

		GMainLoop *m_loop;
		GstElement *m_pipeline;
		GstBus *m_bus;
		GstRTSPServer *m_server;
};
