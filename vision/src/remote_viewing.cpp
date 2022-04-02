#include "remote_viewing.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
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
	// We read the video in at a higher resolution (configurable from command line options) and then scale it, becuase rading it in at a lower resolution
	// will reduce the field of view on the raspberry pi camera that we have
	auto pipeline_description = "( v4l2src device=\"/dev/video0\""
		" ! video/x-raw,width=" + std::to_string(input_width) + ",height=" + std::to_string(input_height) + ",framerate=" + std::to_string(fps) + "/1"
		" ! videoscale ! video/x-raw,width=" + std::to_string(processing_width) + ",height=" + std::to_string(processing_height) + ",framerate=" + std::to_string(fps) + "/1"
		" ! videoconvert ! video/x-raw,format=Y42B"
		" ! x264enc quantizer=25 tune=zerolatency speed-preset=superfast intra-refresh=true ref=1 sliced-threads=true"
		" ! rtph264pay aggregate-mode=zero-latency name=pay0 pt=96 )";


	m_server = gst_rtsp_server_new();
	// set port of server
	gst_rtsp_server_set_service(m_server, std::to_string(port).c_str());

	GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

	GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
	// pipeline should return a GstBin, which is what the parenthasees for the pipeline description are
	gst_rtsp_media_factory_set_launch(factory, pipeline_description.c_str());
	// stream can be shared across network
	gst_rtsp_media_factory_set_shared(factory, true);

	gst_rtsp_mount_points_add_factory(mounts, rtsp_uri.c_str(), factory);
	g_object_unref(mounts);

	gst_rtsp_server_attach(m_server, nullptr);
}

RemoteViewing::~RemoteViewing() {
	if (m_loop_runner_thread.has_value()) {
		stop().ignore();
	}

	g_object_unref(m_loop);
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
		// wait for thread to stop, at this point no new clients can connect
		m_loop_runner_thread->join();
		m_loop_runner_thread = {};

		// remove all currently connected clients, this will cause camera to be available for vision
		gst_rtsp_server_client_filter(m_server, remove_clients, nullptr);

		return Error::ok();
	} else {
		return Error::invalid_operation("could not stop remote viewing, it is not running");
	}
}

Error RemoteViewing::update() {
	return Error::ok();
}

GstRTSPFilterResult RemoteViewing::remove_clients(GstRTSPServer *server, GstRTSPClient *client, gpointer data) {
	return GST_RTSP_FILTER_REMOVE;
}
