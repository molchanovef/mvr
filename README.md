This project a set of simple programs, designed with gstreamer-0.10.36 and Freescale plugins for iMX6
for implement functionality MVR (Mobile Video Recorder ).
MVR designed for record video from IP (and embedded) cameras, store it in separated files and upload video files with Wi-Fi or 3G
to FTP server.

mvr: parse xml file with all board settings: rtsp url of IP cameras, files duration, directory to save, position in mosaic and
run for each camera record and mosaic (if needed) application.

record: get from mvr url, duration, directory and save video from given camera to given directory with given duration.

mosaic: get from mvr url, mosaic type (3x3 or 2x2), position in mosaic and show video on LCD and (or) HDMI display.

upload: monitoring Wi-Fi or 3G network for upload stored videos to FTP server on-the-go.

gst: early version of mosaic.

doc: MVR.txt - Technical Specification.

scripts: set of useful sh scripts for i.MX6 board.
