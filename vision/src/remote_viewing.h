#pragma once

#include <gst/gst.h>
#include <string>
#include "types.h"

class RemoteViewing {
	public:
		RemoteViewing(const std::string& host, u16 port, int width, int height);
		~RemoteViewing();

		// returns true on success
		// sets the pipeline to null state on stop, so it does not have the camera open, so the vision code can read from the camera
		bool start();
		bool stop();

		// TODO: error propagation
		void update();
	
	private:
		GstElement *m_pipeline;
		GstBus *m_bus;
};
