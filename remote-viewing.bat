@echo off
REM NOTE: this currently has unix line endings, copy and paste this on a windows machine to get windows line endings
:a
gst-launch-1.0 rtspsrc location=rtsp://10.23.58.12:5800/remote-viewing latency=0 timeout=1000000 tcp-timeout=1500000 ! rtph264depay ! decodebin ! videoconvert ! videoscale ! video/x-raw,width=1280,height=720 ! autovideosink
timeout /T 2 /NOBREAK
goto a
