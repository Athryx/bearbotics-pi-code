#!/bin/sh

cd $(dirname $0)

# For now, only run remote viewing without vision or mqtt
exec ./src/build/Vision -r --image-width 640 --image-height 480 --fps 30 -m localhost templates/
#exec ./src/build/Vision -r --image-width 640 --image-height 480 --fps 30 templates/
