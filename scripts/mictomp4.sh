#!/bin/bash

gst-launch -e alsasrc ! audio/x-raw-int, rate=32000, channel=2 ! mfw_mp3encoder ! queue ! mp4mux ! filesink location=test.mp4

