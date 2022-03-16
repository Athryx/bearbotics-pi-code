#!/bin/sh

cd $(dirname $0)

# For now, only run remote viewing without vision or mqtt
exec ./src/build/Vision --log-level 1 -r --rtp-host 127.0.0.1 templates/
