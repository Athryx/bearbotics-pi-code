#!/bin/sh

#ffmpeg -re -f v4l2 -s 640x480 -i /dev/video0 -c:v libx264 -profile:v baseline -trellis 0 -subq 1 -level 32 -preset superfast -intra-refresh 1 -tune zerolatency -crf 30 -threads 0 -bufsize 1 -refs 4 -coder 0 -b_strategy 0 -bf 0 -sc_threshold 0 -x264-params vbv-maxrate=2000:slice-max-size=1500:keyint=30:min-keyint=10: -pix_fmt yuv420p -an -f flv rtmp://localhost:1935/test

#ffmpeg -format bgra -framerate 60 -f kmsgrab -i - -c:v libx264 -profile:v baseline -trellis 0 -subq 1 -level 32 -preset superfast -intra-refresh 1 -tune zerolatency -crf 30 -threads 0 -bufsize 1 -refs 4 -coder 0 -b_strategy 0 -bf 0 -sc_threshold 0 -x264-params vbv-maxrate=2000:slice-max-size=1500:keyint=30:min-keyint=10: -pix_fmt yuv420p -an -f flv rtmp://localhost:1935/test

#ffmpeg -format bgra -framerate 60 -f x11grab -s 1920x1200 -i :0.0 -c:v libx264 -profile:v baseline -trellis 0 -subq 1 -level 32 -preset superfast -intra-refresh 1 -tune zerolatency -crf 30 -threads 0 -bufsize 1 -refs 4 -coder 0 -b_strategy 0 -bf 0 -sc_threshold 0 -x264-params vbv-maxrate=2000:slice-max-size=1500:keyint=30:min-keyint=10: -pix_fmt yuv420p -an -f flv rtmp://localhost:1935/test

ffmpeg \
	-f v4l2 -s 640x480 -framerate 30 -i /dev/video0 \
	-c:v libx264 -profile:v baseline -trellis 0 -subq 1 -level 32 -preset superfast -tune zerolatency -crf 30 -threads 0 -bufsize 1 -refs 1 -coder 0 -b_strategy 0 -bf 0 -sc_threshold 0 -pix_fmt yuv420p -intra-refresh 1 \
	-x264opts vbv-maxrate=2000:vbv-bufsize=67:slice-max-size=1500:keyint=30:min-keyint=10 \
	-f mpegts - | nc -l -p 5000
	#-an -f flv rtmp://localhost:1935/test
