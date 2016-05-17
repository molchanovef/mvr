#!/bin/sh
if [[ $# -lt 1 ]]; then
        echo "Usage $0 <file_to_rec>"
        exit 0;
fi
file=$1

gst-launch rtspsrc location=rtsp://admin:admin@192.168.1.200/0 ! rtph264depay ! h264parse ! matroskamux ! filesink location=$file -e -v

