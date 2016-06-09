#!/bin/sh
if [[ $# -lt 0 ]]; then
        echo "Usage $0 [left] [top] [width] [height]"
        exit 0;
fi

left=0
top=0
w=800
h=480

if [[ $# -eq 4 ]]; then
left=$1
top=$2
w=$3
h=$4
fi

#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.253:554/mpeg4 \
#latency=500 ! gstrtpjitterbuffer ! rtpjpegdepay ! vpudec output-format=1 ! mfw_isink \
#axis-left=$left axis-top=$top disp-width=$w disp-height=$h

gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.253:554/mpeg4 \
latency=500 ! gstrtpjitterbuffer ! rtpmp4vdepay ! vpudec output-format=1 ! mfw_isink \
axis-left=$left axis-top=$top disp-width=$w disp-height=$h
