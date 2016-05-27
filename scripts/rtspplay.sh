#!/bin/sh

gst-launch rtspsrc location=rtsp://192.168.1.2:8554 name=src ! rtph264depay ! vpudec ! mfw_isink src. ! rtpmpadepay ! mpegaudioparse ! beepdec ! alsasink
#mpegaudioparse ! typefind=true ! beepdec ! audioconvert ! alsasink 

