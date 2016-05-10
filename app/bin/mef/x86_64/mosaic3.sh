#!/bin/bash

./mosaic rtsp://admin:admin@192.168.1.200/0 3 1 &
./mosaic rtsp://admin:admin@192.168.1.200/0 3 3 &
./mosaic rtsp://admin:admin@192.168.1.200/0 3 5 &
./mosaic rtsp://admin:admin@192.168.1.200/0 3 7 &
./mosaic rtsp://admin:admin@192.168.1.200/0 3 9 &
#./mosaic rtsp://192.168.1.10/user=admin_password=tlJwpbo6_channel=1_stream=0.sdp?real_stream 1 &
#./mosaic rtsp://admin:9999@192.168.11.94:8554/Stream1 3 5 &
#./mosaic rtsp://admin:9999@192.168.11.94:8555/Stream2 3 9 &

#./quad rtsp://192.168.1.2:8554/ 1 &
#./quad rtsp://192.168.1.2:8555/ 2 &
#./quad rtsp://192.168.1.2:8556/ 3 &

#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.42:8554/Stream1 ! gstrtpjitterbuffer ! rtph264depay ! vpudec ! vpuenc ! avimux ! filesink location=/mnt/nfs/rec.avi &
#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 ! gstrtpjitterbuffer ! rtph264depay ! vpudec ! vpuenc ! avimux ! filesink location=/run/media/sda1/rec.avi &

