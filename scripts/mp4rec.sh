#!/bin/sh

#gst-launch rtspsrc location=rtsp://admin:admin@192.168.1.200/0 ! gstrtpjitterbuffer ! rtph264depay ! h264parse ! mp4mux ! filesink location=/media/Videos/mvr.mp4 -e -v
gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 ! gstrtpjitterbuffer ! rtph264depay ! h264parse ! mp4mux ! filesink location=/media/Videos/mvr.mp4 -e -v

#  ! queue max-size-buffers=10 ! vpuenc codec=0 ! qtmux ! filesink location=$file
#! vpudec output-format=1 ! tee name=t ! queue ! mfw_isink \
#axis-left=$left axis-top=$top disp-width=$w disp-height=$h \
#t. ! queue 

