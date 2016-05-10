#!/bin/sh

if [[ $# -lt 1 ]]; then
        echo "Usage $0 <file_to_rec>"
        exit 0;
fi
file=$1

gst-launch rtspsrc location=rtsp://admin:admin@192.168.1.200/0 ! gstrtpjitterbuffer ! rtph264depay ! h264parse ! queue ! mux. avimux name=mux ! filesink location=$file -e -v

#! gstrtpjitterbuffer ! rtph264depay \
#! vpudec output-format=1 \
#! tee name=t ! queue ! mfw_isink axis-left=$left axis-top=$top disp-width=$w disp-height=$h \
#t. ! queue \
#! vpuenc codec=0 ! avimux ! filesink location=$file

