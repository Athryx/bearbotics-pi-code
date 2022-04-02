#!/bin/sh

while true
do
	gst-launch-1.0 rtspsrc location=rtsp://$1:5000/remote-viewing latency=0 ! rtph264depay ! decodebin ! videoconvert ! autovideosink
	sleep 3
done
