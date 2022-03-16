#!/bin/sh

# For now, only run remote viewing without vision or mqtt
./src/build/Vision --log-level 1 -r --rtp-host 127.0.0.1 templates/
