#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h> //fork
#include <gst/gst.h>
#include <glib.h>

#define DW		800
#define DH		480
#define BORDER	5//minimum 2 cause 1 pixel used for black border
//#define URL "rtsp://admin:9999@192.168.11.94:8555/Stream2"
#define URL	"rtsp://admin:admin@192.168.1.200/0"

void print_help(char *argv)
{
	printf("Usage %s <rtsp url> <type(2,3) 2x2 or 3x3> <position 1-4 or 1-9> <latency ms>\n",argv);
}

int main(int argc, char* argv[])
{
	int ret, pos, type, latency;
	char url[1024] = {0};
	char gst[1024] = {0};
	int left, top, width, height;

	print_help(argv[0]);
	if(argc < 2)
		strcpy(url, URL);
	else
		strcpy(url,argv[1]);
	if(argc < 3)
		type = 2;
	else
		type = atoi(argv[2]);
	if(argc < 4)
		pos = 1;
	else
		pos = atoi(argv[3]);
	if(argc < 5)
		latency = 3000;
	else
		latency = atoi(argv[4]);

	printf("url %s type %d position %d latency %d\n", url, type, pos, latency);
#if 0
	const gchar *nano_str;
	guint major, minor, micro, nano;

	gchar *descr;
	GError *error = NULL;
	GMainLoop *main_loop;  /* GLib's Main Loop */

	/* Initialisation */
	gst_init (&argc, &argv);

	gst_version (&major, &minor, &micro, &nano);

	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";

	printf ("This program is linked against GStreamer %d.%d.%d %s\n",
		  major, minor, micro, nano_str);

	main_loop = g_main_loop_new (NULL, FALSE);
#endif
	width = DW/type; height = DH/type;
	if( type == 2 )
	{
		switch(pos)
		{
			case 1: left = 0; top = 0; break;
			case 2: left = width; top = 0; break;
			case 3: left = 0; top = height; break;
			case 4: left = width; top = height; break;
		}
	}
	else if ( type == 3 )
	{
		switch(pos)
		{
			case 1: left = 0; top = 0; break;
			case 2: left = width; top = 0; break;
			case 3: left = width*2; top = 0; break;
			case 4: left = 0; top = height; break;
			case 5: left = width; top = height; break;
			case 6: left = width*2; top = height; break;
			case 7: left = 0; top = height*2; break;
			case 8: left = width; top = height*2; break;
			case 9: left = width*2; top = height*2; break;
		}

	}
	ret = fork();
	if(ret == 0)
	{
#ifdef CROSS
		//																						1: I420
		sprintf(gst,"gst-launch rtspsrc location=%s latency=%d ! gstrtpjitterbuffer ! rtph264depay\
		 ! vpudec output-format=1 ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d &", url, latency, left, top, width, height);
#else
// gst-launch-0.10 rtspsrc location=rtsp://admin:admin@192.168.1.200/0 ! rtph264depay ! h264parse ! ffdec_h264 ! ffmpegcolorspace ! autovideosink
/*		sprintf(gst,"gst-launch-0.10 rtspsrc location=%s ! rtph264depay ! h264parse ! ffdec_h264 !  videoscale ! ffmpegcolorspace ! v4l2sink \
					crop-left=%d crop-top=%d crop-width=%d crop-height=%d&", url, left, top, width, height);
*/		sprintf(gst,"gst-launch-0.10 rtspsrc location=%s latency=%d ! rtph264depay ! h264parse ! ffdec_h264 !  ffmpegcolorspace ! autovideosink &", url, latency);
#endif
		printf("%s\n",gst);
		printf("sys %d\n",system(gst));
		return 0;
	}
	if(ret < 0)
		return -1;
	return 0;
}
