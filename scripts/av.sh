#!/bin/bash

if [[ $# -lt 1 ]]; then
	echo "Usage $0 <path to file>"
	echo "Usage $0 <path to file> [left] [top] [width] [height]"
	echo "Usage $0 <path to file> [type 2 or 3 for mosaic 2x2 or 3x3] [pos 1-4 or 1-9]"
	exit 0;
fi

vfile=$1
type=$2
pos=$3
dw=800
dh=480
left=0
top=0
w=$dw
h=$dh

if [[ $# -eq 5 ]]; then
left=$2
top=$3
w=$4
h=$5
fi

if [[ $# -eq 3 ]]; then
w=`expr $dw / $type`
h=`expr $dh / $type`
w2=`expr $w \* 2`
w3=`expr $w \* 3`
h2=`expr $h \* 2`
h3=`expr $h \* 3`

if [[ type -eq 2 ]]; then
case $pos in
	1) left=0 top=0;;
	2) left=$w top=0;;
	3) left=0 top=$h;;
	4) left=$w top=$h;;
esac
fi
if [[ type -eq 3 ]]; then
case $pos in
	1) left=0;		top=0;;
	2) left=$w; 	top=0;;
	3) left=$w2;	top=0;;
	4) left=0;		top=$h;;
	5) left=$w; 	top=$h;;
	6) left=$w2; 	top=$h;;
	7) left=0; 		top=$h2;;
	8) left=$w; 	top=$h2;;
	9) left=$w3; 	top=$h3;;
esac
fi
fi

gst-launch filesrc location=${vfile} typefind=true \
! aiurdemux name=demux demux. ! \
queue max-size-buffers=0 max-size-time=0 ! vpudec ! mfw_isink  \
axis-left=$left axis-top=$top disp-width=$w disp-height=$h \
demux. ! queue max-size-buffers=0 max-size-time=0 ! beepdec \
! audioconvert ! alsasink

