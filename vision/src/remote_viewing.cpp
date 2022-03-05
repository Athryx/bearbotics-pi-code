#include "remote_viewing.h"
#include "gst/gstelement.h"
#include "util.h"
#include <assert.h>
#include <string>

#include <iostream>

RemoteViewing::RemoteViewing(const std::string& host, u16 port, int width, int height) {
	// TODO: set these properties without using parse_luanch
	// Read the video in at 1920x1080 and then scale it, becuase rading it in at a lower resolution
	// will reduce the field of view on the raspberry pi camera that we have
	// TODO: make the resolution we read in configurable
	auto pipeline_description = std::string("v4l2src device=\"/dev/video0\""
		" ! videoscale ! video/x-raw,width=" + std::to_string(width) + ",height=" + std::to_string(height) +
		" ! videoscale ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast"
		" ! rtph264pay config-interval=10 pt=96 ! udpsink host=") + host + std::string(" port=") + std::to_string(port);

	m_pipeline = gst_parse_launch(pipeline_description.c_str(), nullptr);
	assert(m_pipeline != nullptr);

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

RemoteViewing::~RemoteViewing() {
	stop();
	gst_object_unref(m_pipeline);
}
