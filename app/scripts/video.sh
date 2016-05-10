#!/bin/bash

if [[ $# -lt 1 ]]; then
        echo "Usage $0 <path to file> [left] [top] [width] [height]"
        exit 0;
fi

vfile=$1
left=0
top=0
w=800
h=480

if [[ $# -eq 5 ]]; then
left=$2
top=$3
w=$4
h=$5
fi

gst-launch filesrc location=${vfile} typefind=true \
! aiurdemux name=demux demux. ! \
queue max-size-buffers=0 max-size-time=0 ! vpudec ! mfw_isink  \
axis-left=$left axis-top=$top disp-width=$w disp-height=$h \
#demux. ! queue max-size-buffers=0 max-size-time=0 ! beepdec \
#! audioconvert ! alsasink

