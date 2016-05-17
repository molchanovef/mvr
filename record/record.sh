#!/bin/bash
nfsDir=/media/Videos/MVR
sataDir=/media/sda1/MVR

./record 60 ${sataDir} rtsp://admin:admin@192.168.1.200/0 &
./record 60  ${sataDir} rtsp://admin:9999@192.168.11.94:8554/Stream1 &
./record 60  ${sataDir} rtsp://admin:9999@192.168.11.94:8555/Stream2 &
./record 60  ${sataDir} rtsp://192.168.1.10/user=admin_password=tlJwpbo6_channel=1_stream=0.sdp?real_stream &
#./record 60  ${sataDir} rtsp://admin:9999@192.168.1.168:8554/Stream1 &

