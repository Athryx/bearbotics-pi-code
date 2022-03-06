#pragma once

#include <gst/gst.h>
#include <string>
#include "types.h"
#include "error.h"

class RemoteViewing {
	public:
		// TODO: error propagation
		RemoteViewing(const std::string& host, u16 port, int width, int height);
		~RemoteViewing();

		// sets the pipeline to null state on stop, so it does not have the camera open, so the vision code can read from the camera
		Error start();
		Error stop();

		// TODO: move logging from here into main.cpp
		Error update();
	
	private:
		GstElement *m_pipeline;
		GstBus *m_bus;
};
