#!/bin/sh

gst-launch-1.0 rtspsrc location=rtsp://localhost:5000/remote-viewing latency=0 ! rtph264depay ! decodebin ! videoconvert ! autovideosink
