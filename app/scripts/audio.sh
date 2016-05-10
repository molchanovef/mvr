#!/bin/sh
gst-launch filesrc location=/media/sda1/test.mp3 ! typefind=true ! \
beepdec ! audioconvert  ! 'audio/x-raw-int, channels=2' ! alsasink
