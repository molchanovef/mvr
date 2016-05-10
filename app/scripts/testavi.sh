gst-launch videotestsrc num-buffers=250 \
! 'video/x-raw,format=(string)I420,width=320,height=240,framerate=(fraction)25/1' \
! queue ! mux. \
audiotestsrc num-buffers=440 ! audioconvert \
! 'audio/x-raw,rate=44100,channels=2' ! queue ! mux. \
avimux name=mux ! filesink location=test.avi
