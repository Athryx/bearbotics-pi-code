#!/bin/sh

#ffplay -fflags nobuffer -flags low_delay -framedrop -strict experimental -rtsp_transport udp rtsp://localhost:5000/remote-viewing
ffplay -fflags nobuffer -flags low_delay -framedrop -strict experimental rtmp://localhost:1935/test