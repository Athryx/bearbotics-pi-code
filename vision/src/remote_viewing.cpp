#include "remote_viewing.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp-server/rtsp-server-object.h>
#include "util.h"
#include <assert.h>
#include <string>
#include <iostream>
#include "logging.h"

// XXX: this will fail and exit the program if something goes wrong
RemoteViewing::RemoteViewing(u16 port, const std::string& rtsp_uri, int input_width, int input_height, int processing_width, int processing_height, int fps) {
	// use default context for main loop
	m_loop = g_main_loop_new(nullptr, false);

	// TODO: set these properties without using parse_luanch
	// Read the video in at 1920x1080 and then scale it, becuase rading it in at a lower resolution
	// will reduce the field of view on the raspberry pi camera that we have
	auto pipeline_description = "( v4l2src device=\"/dev/video0\""
		" ! video/x-raw,width=" + std::to_string(input_width) + ",height=" + std::to_string(input_height) + ",framerate=" + std::to_string(fps) + "/1"
		" ! videoscale ! video/x-raw,width=" + std::to_string(processing_width) + ",height=" + std::to_string(processing_height) + ",framerate=" + std::to_string(fps) + "/1"
		" ! videoconvert ! video/x-raw,format=Y444"
		" ! x264enc quantizer=25 tune=zerolatency speed-preset=superfast intra-refresh=true ref=1 sliced-threads=true"
		" ! rtph264pay aggregate-mode=zero-latency name=pay0 pt=96 )";

	/*GstElement *pipeline = gst_parse_launch(pipeline_description.c_str(), nullptr);
	assert(m_pipeline != nullptr);

	m_bus = gst_element_get_bus(m_pipeline);

	// NOTE: this might be the starting state, so this might be unnesessary
	auto state = gst_element_set_state(m_pipeline, GST_STATE_NULL);
	assert(state != GST_STATE_CHANGE_FAILURE);*/


	m_server = gst_rtsp_server_new();
	// set port of server
	g_object_set(m_server, "service", std::to_string(port).c_str(), nullptr);

	GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

	GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
	// pipeline should return a GstBin, which is what the parenthasees for the pipeline description are
	gst_rtsp_media_factory_set_launch(factory, pipeline_description.c_str());
	// stream can be shared across network
	gst_rtsp_media_factory_set_shared(factory, true);

	gst_rtsp_mount_points_add_factory(mounts, rtsp_uri.c_str(), factory);
	g_object_unref(mounts);

	// attach server to the default maincontext for the event loop
	gst_rtsp_server_attach(m_server, nullptr);
}

RemoteViewing::~RemoteViewing() {
	if (m_loop_runner_thread.has_value()) {
		stop().ignore();
	}

	/*gst_object_unref(m_bus);
	stop().ignore();
	gst_object_unref(m_pipeline);*/
	g_object_unref(m_loop);
	// TODO: figure out if this is the correct unref object
	g_object_unref(m_server);
}

Error RemoteViewing::start() {
	if (m_loop_runner_thread.has_value()) {
		return Error::invalid_operation("could not start remote viewing, it is already running");
	} else {
		m_loop_runner_thread = std::thread(g_main_loop_run, m_loop);
		return Error::ok();
	}
}

Error RemoteViewing::stop() {
	if (m_loop_runner_thread.has_value()) {
		g_main_loop_quit(m_loop);
		// wait for thread to stop, at this point camera is available for vision
		m_loop_runner_thread->join();
		m_loop_runner_thread = {};
		return Error::ok();
	} else {
		return Error::invalid_operation("could not stop remote viewing, it is not running");
	}
}

Error RemoteViewing::update() {
	/*GstMessage *msg = gst_bus_pop_filtered(m_bus, (GstMessageType) (GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

	if (msg != nullptr) {
		GError *err;
		char *debug_message;

		auto msg_type = GST_MESSAGE_TYPE(msg);

		switch (msg_type) {
			case GST_MESSAGE_EOS:
				lg::warn("end of stream reached");
				gst_message_unref(msg);
				return Error::resource_unavailable("unexpected end of stream message recieved from remote viewing pipeline");
			case GST_MESSAGE_ERROR: {
				gst_message_parse_error(msg, &err, &debug_message);

				std::string error_string = "gstreamer pipeline element " + std::string(GST_OBJECT_NAME(msg->src))
					+ ": " + std::string(err->message);

				lg::error("%s\ndebugging info: %s",
					error_string.c_str(),
					debug_message != nullptr ? debug_message : ""
				);
				g_clear_error(&err);
				g_free(debug_message);

				gst_message_unref(msg);
				return Error::library(std::move(error_string));
			}
			default:
				lg::warn("unexpected message type recieved");
				gst_message_unref(msg);
				return Error::unknown("unexpected message type recieved from remote viewing gstreamer pipeline");
		}
	}*/

	return Error::ok();
}
