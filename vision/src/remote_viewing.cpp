#include "remote_viewing.h"
#include <gst/gst.h>
#include "util.h"
#include <assert.h>
#include <string>
#include <iostream>
#include "logging.h"

// XXX: this will fail and exit the program if something goes wrong
RemoteViewing::RemoteViewing(const std::string& host, u16 port, int input_width, int input_height, int processing_width, int processing_height) {
	// TODO: set these properties without using parse_luanch
	// Read the video in at 1920x1080 and then scale it, becuase rading it in at a lower resolution
	// will reduce the field of view on the raspberry pi camera that we have
	// TODO: make the resolution we read in from the camera configurable
	auto pipeline_description = "v4l2src device=\"/dev/video0\""
		" ! video/x-raw,width=" + std::to_string(input_width) + ",height=" + std::to_string(input_height) +
		" ! videoscale ! video/x-raw,width=" + std::to_string(processing_width) + ",height=" + std::to_string(processing_height) +
		" ! videoscale ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast"
		" ! rtph264pay config-interval=10 pt=96 ! udpsink host=" + host + " port=" + std::to_string(port);

	m_pipeline = gst_parse_launch(pipeline_description.c_str(), nullptr);
	assert(m_pipeline != nullptr);

	m_bus = gst_element_get_bus(m_pipeline);

	// NOTE: this might be the starting state, so this might be unnesessary
	auto state = gst_element_set_state(m_pipeline, GST_STATE_NULL);
	assert(state != GST_STATE_CHANGE_FAILURE);
}

Error RemoteViewing::start() {
	if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		return Error::library("could not start remote viewing");
	} else {
		return Error::ok();
	}
}

Error RemoteViewing::stop() {
	if (gst_element_set_state(m_pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
		return Error::library("could not stop remote viewing");
	} else {
		return Error::ok();
	}
}

Error RemoteViewing::update() {
	GstMessage *msg = gst_bus_pop_filtered(m_bus, (GstMessageType) (GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

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
	}

	return Error::ok();
}

RemoteViewing::~RemoteViewing() {
	gst_object_unref(m_bus);
	stop().ignore();
	gst_object_unref(m_pipeline);
}
