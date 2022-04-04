#!/bin/sh

cd $(dirname $0)

# For now, only run remote viewing without vision or mqtt
#exec ./src/build/vision -r --image-width 640 --image-height 480 --fps 30 -m localhost templates/
exec ./src/build/vision -r --image-width 640 --image-height 480 --fps 30 templates/

# remote viewing on pi
#exec ./src/build/vision -r --cam-width 1280 --cam-height 720 --image-width 320 --image-height 240 --fps 30 templates/
