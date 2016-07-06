This project a set of simple programs, designed with gstreamer-0.10.36 and Freescale plugins for iMX6 for implement functionality MVR (Mobile Video Recorder ).
MVR designed for record video from IP (and embedded) cameras, store it in separated files and upload video files with Wi-Fi or 3G to FTP server.

csicam: Conflicting with gstreamer because both use IPU.
captures frames from built-in csi camera through scd driver
Creates appsrc gstreamer pipeline with tee for two tasks:
1. Showing live video with mfw_isink
2. Encode frames with vpuenc for recording.

lib:
avi - module for manage IPCAM frames in RAM.
btr - bayer to rgb module for mt9p031 sensor.
my_log - logging module.

rtsp: uses lib/ipc/avi module for put frames from IP Cameras into RAM. Not implemented yet.

record: gets from mvr url, duration, directory and save video from given camera to given directory with given duration.

mosaic: gets from mvr url, mosaic type (3x3 or 2x2), position in mosaic and shows video on LCD and (or) HDMI display. After implementing lib/ipc/avi should receive frames from RAM for fast switch layouts.

gst: early version of mosaic.

upload: monitoring Wi-Fi or 3G network for upload stored video files to FTP server on-the-go.

mvr: parse xml file with all settings: name, rtsp url of IP cameras for rtsp, record and mosaic, decoder type for mosaic, files duration, directory to save for record.
Record uses stream from IPCAM with high resolution and quality.
Mosaic uses low quality stream for show video on the screen.
After implement rtsp, mosaic will receive frames from RAM stored by rtsp.
Also start upload application.
Monitoring all applications and restarting them if fails.

doc: MVR.txt - Technical Specification.

scripts: set of useful sh scripts for i.MX6 board.
