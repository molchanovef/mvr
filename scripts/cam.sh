#!/bin/bash

stream0="stream0"
stream1="stream1"
stream2="stream2"
stream3="stream3"
if [ ! -f "$stream0" ]
then
mkfifo $stream0
fi
if [ ! -f "$stream1" ]
then
mkfifo $stream1
fi
if [ ! -f "$stream2" ]
then
mkfifo $stream2
fi
if [ ! -f "$stream3" ]
then
mkfifo $stream3
fi

./quad rtsp://admin:9999@192.168.11.94:8555/Stream2 0 &
./quad rtsp://admin:9999@192.168.11.94:8555/Stream2 1 &
./quad rtsp://admin:9999@192.168.11.94:8555/Stream2 2 &
./quad rtsp://admin:9999@192.168.11.94:8555/Stream2 3 &

#./quad rtsp://192.168.1.2:8554/ 1 &
#./quad rtsp://192.168.1.2:8555/ 2 &
#./quad rtsp://192.168.1.2:8556/ 3 &

#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.42:8554/Stream1 ! gstrtpjitterbuffer ! rtph264depay ! vpudec ! vpuenc ! avimux ! filesink location=/mnt/nfs/rec.avi &
#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 ! gstrtpjitterbuffer ! rtph264depay ! vpudec ! vpuenc ! avimux ! filesink location=/run/media/sda1/rec.avi &
#gst-launch rtspsrc location=rtsp://admin:admin@192.168.1.200/0 ! gstrtpjitterbuffer ! rtph264depay ! vpudec ! vpuenc ! avimux ! filesink location=/run/media/sda1/rec.avi

