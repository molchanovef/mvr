#!/bin/sh
#CAPS=caps="application/x-rtp, media=(string)audio, clock-rate=(int)8000, encoding-name=(string)MP4A-LATM, cpresent=(string)0, config=(string)40002810, payload=(int)96, ssrc=(uint)723033857, clock-base=(uint)3702678, seqnum-base=(uint)15429"
CAPS=caps="application/x-rtp, media=(string)audio, payload=(int)96, clock-rate=(int)8000, encoding-name=(string)MPEG4-GENERIC, streamtype=(string)5, profile-level-id=(string)1, mode=(string)AAC-hbr, sizelength=(string)13, indexlength=(string)3, indexdeltalength=(string)3, config=(string)1588, a-tool=(string)\"LIVE555\\ Streaming\\ Media\\ v2011.05.25\", a-type=(string)broadcast, x-qt-text-nam=(string)\"RTSP/RTP\\ stream\\ from\\ IPNC\", x-qt-text-inf=(string)Stream1, clock-base=(uint)2612926470, seqnum-base=(uint)48553, npt-start=(guint64)0, play-speed=(double)1, play-scale=(double)1"

#gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 name=src ${CAPS} ! rtpmp4adepay ! aacparse ! beepdec ! alsasink src. ! rtph264depay ! vpudec ! mfw_isink -v
#gst-launch rtspsrc location=rtsp://192.168.1.2:8554 name=src ! rtph264depay ! vpudec ! mfw_isink src. ! rtpmpadepay ! mpegaudioparse ! beepdec ! alsasink
gst-launch rtspsrc location=rtsp://admin:9999@192.168.11.94:8554/Stream1 name=src src. ! decodebin ! mfw_isink src. ! decodebin ! audioconvert ! alsasink

