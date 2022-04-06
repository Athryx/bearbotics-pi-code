#!/bin/sh

while true
do
	# set the timeout to be low so that when remote viewing is disabled (when switching to vision) the gstreamer window quickly closes
	gst-launch-1.0 rtspsrc location=rtsp://$1:5800/remote-viewing latency=0 timeout=1000000 tcp-timeout=1500000 ! rtph264depay ! decodebin ! videoconvert ! videoscale ! video/x-raw,width=1280,height=720 ! autovideosink
	sleep 2
done
