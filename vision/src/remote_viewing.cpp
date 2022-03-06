#include "remote_viewing.h"
#include <gst/gst.h>
#include "util.h"
#include <assert.h>
#include <string>
#include <iostream>
#include "logging.h"

// XXX: this will fail and exit the program if something goes wrong
RemoteViewing::RemoteViewing(const std::string& host, u16 port, int width, int height) {
	// TODO: set these properties without using parse_luanch
	// Read the video in at 1920x1080 and then scale it, becuase rading it in at a lower resolution
	// will reduce the field of view on the raspberry pi camera that we have
	// TODO: make the resolution we read in configurable
	auto pipeline_description = std::string("v4l2src device=\"/dev/video0\" ! video/x-raw,width=1920,height=1080"
		" ! videoscale ! video/x-raw,width=" + std::to_string(width) + ",height=" + std::to_string(height) +
		" ! videoscale ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast"
		" ! rtph264pay config-interval=10 pt=96 ! udpsink host=") + host + std::string(" port=") + std::to_string(port);

	m_pipeline = gst_parse_launch(pipeline_description.c_str(), nullptr);
	assert(m_pipeline != nullptr);

	m_bus = gst_element_get_bus(m_pipeline);

	// NOTE: this might be the starting state, so this might be unnesessary
	auto state = gst_element_set_state(m_pipeline, GST_STATE_NULL);
	assert(state != GST_STATE_CHANGE_FAILURE);
}

Error RemoteViewing::start() {
	if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		return Error(ErrorType::RemoteViewingStateChangeError, "could not start remote viewing");
	} else {
		return Error::ok();
	}
}

Error RemoteViewing::stop() {
	if (gst_element_set_state(m_pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
		return Error(ErrorType::RemoteViewingStateChangeError, "could not stop remote viewing");
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
		gst_message_unref(msg);

		switch (msg_type) {
			case GST_MESSAGE_EOS:
				lg::warn("end of stream reached");
				return Error(ErrorType::RemoteViewingEosError, "unexpected eos message recieved from remote viewing pipeline");
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

				return Error(ErrorType::RemoteViewingPipelineError, std::move(error_string));
			}
			default:
				lg::warn("unexpected message type recieved");
				return Error::internal("unexpected message type recieved from remote viewing gstreamer pipeline");
		}
	}

	return Error::ok();
}

RemoteViewing::~RemoteViewing() {
	gst_object_unref(m_bus);
	stop();
	gst_object_unref(m_pipeline);
}
