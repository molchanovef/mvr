#!/bin/sh
gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 latency=300 ! gstrtpjitterbuffer ! rtph264depay ! vpudec output-format=1 ! mfw_isink
