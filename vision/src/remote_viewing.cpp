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

bool RemoteViewing::start() {
	return gst_element_set_state(m_pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
}

bool RemoteViewing::stop() {
	return gst_element_set_state(m_pipeline, GST_STATE_NULL) != GST_STATE_CHANGE_FAILURE;
}

void RemoteViewing::update() {
	GstMessage *msg = gst_bus_pop_filtered(m_bus, (GstMessageType) (GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

	if (msg != nullptr) {
		GError *err;
		char *debug_message;

		switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_EOS:
				warn("end of stream reached");
				break;
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_message);
				error("gstreamer pipeline element %s: %s\ndebugging info: %s",
					GST_OBJECT_NAME(msg->src),
					err->message,
					debug_message != nullptr ? debug_message : ""
				);
				g_clear_error(&err);
				g_free(debug_message);
				break;
			default:
				warn("unexpected message type recieved");
				break;
		}
		gst_message_unref(msg);
	}
}

RemoteViewing::~RemoteViewing() {
	gst_object_unref(m_bus);
	stop();
	gst_object_unref(m_pipeline);
}
