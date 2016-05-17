#!/bin/sh

#if [[ $# -lt 1 ]]; then
#        echo "Usage $0 <path to file>"
#        exit 0;
#fi

#file=$1

#gst-launch videotestsrc ! vpuenc ! rtph264pay config-interval=10 pt=96 ! udpsink host=192.168.1.2 port=5000

## IMX2PC: Case where iMX does the streaming
IP=192.168.1.7 # IP address of the playback machine
VIDEO_SRC="videotestsrc"
VIDEO_ENC="vpuenc codec=h263 ! rtph263ppay "
AUDIO_ENC="audiotestsrc ! mfw_mp3encoder ! rtpmpapay "
## END IMX2PC settings

STREAM_AUDIO="$AUDIO_ENC ! rtpbin.send_rtp_sink_1 \
rtpbin.send_rtp_src_1 ! udpsink host=$IP port=5002 \
rtpbin.send_rtcp_src_1 ! udpsink host=$IP port=5003 sync=false async=false \
udpsrc port=5007 ! rtpbin.recv_rtcp_sink_1"

STREAM_VIDEO="$VIDEO_SRC ! $VIDEO_ENC ! rtpbin.send_rtp_sink_0 \
rtpbin.send_rtp_src_0 ! queue ! udpsink host=$IP port=5000 \
rtpbin.send_rtcp_src_0 ! udpsink host=$IP port=5001 sync=false async=false \
udpsrc port=5005 ! rtpbin.recv_rtcp_sink_0"

STREAM_AV="$STREAM_VIDEO $STREAM_AUDIO"

# Stream pipeline

gst-launch -v gstrtpbin name=rtpbin $STREAM_AV
