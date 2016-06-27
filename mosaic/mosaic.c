#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h> //fork
#include <gst/gst.h>
#include <glib.h>
#include "avi.h"

#define DW		800
#define DH		480
#define TAG		"MOSAIC:"

typedef struct _Mosaic
{
	GstElement 	*pipeline;
	GMainLoop *main_loop;  /* GLib's Main Loop */
	GstBus *bus;
	guint bus_watch_id;
} Mosaic;

Mosaic *mosaic;

void sig_handler(int signum);

void print_help(char *argv)
{
	printf("Usage %s <rtsp url> <name> <decoder_type> <type(1,2,3) single, 2x2 or 3x3> <position 1-4 or 1-9> <latency ms>\n",argv);
	exit(1);
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer ptr)
{
	Mosaic *h = (Mosaic*)ptr;
	switch (GST_MESSAGE_TYPE (msg))
	{
		case GST_MESSAGE_ERROR:
		{
			gchar  *debug;
			GError *error;
			gst_message_parse_error (msg, &error, &debug);
			g_printerr ("%s Error: %s\n", TAG, error->message);
			g_error_free (error);
			if (debug)
			{
				g_printerr ("%s [Debug details: %s]\n", TAG, debug);
				g_free (debug);
			}
			g_main_loop_quit (h->main_loop);
			break;
		}
		default:
		break;
	}
	return TRUE;
}

int main(int argc, char* argv[])
{
	int pos, type, latency;
	char name[1024] = {0};
	char url[1024] = {0};
	char decoder[16] = {0};
	int left, top, width, height;
/*	const gchar *nano_str;
	guint major, minor, micro, nano;*/
	gchar *descr;
	GError *error = NULL;
	Mosaic *h;
	
	h = malloc(sizeof(Mosaic));
	if(h == NULL)
	{
		g_print("%s memory allocation error\n", TAG);
		exit(1);
	}
	mosaic = h;
	
	if(argc < 7)
		print_help(argv[0]);
	else
	{
		strcpy(url,argv[1]);
		strcpy(name, argv[2]);
		strcpy(decoder, argv[3]);
		type = atoi(argv[4]);
		pos = atoi(argv[5]);
		latency = atoi(argv[6]);
	}
		
//	printf("%s name %s url %s decoder %s type %d position %d latency %d\n", TAG, name, url, decoder, type, pos, latency);
	width = DW/type; height = DH/type;
	if( type == 1 )
	{
		left = 0; top = 0;
	}
	else if( type == 2 )
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

	signal(SIGINT, sig_handler);

	/* Initialisation */
	gst_init (&argc, &argv);
/*	gst_version (&major, &minor, &micro, &nano);

	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";

	printf ("%s This program is linked against GStreamer %d.%d.%d %s\n", TAG, major, minor, micro, nano_str);
*/
	h->main_loop = g_main_loop_new (NULL, FALSE);

#ifdef CROSS
	if(strcmp(decoder, "h264") == 0)
	 	descr = g_strdup_printf ("rtspsrc location=%s latency=%d ! gstrtpjitterbuffer ! rtph264depay ! vpudec output-format=1 frame-plus=10 low-latency=true ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", url, latency, left, top, width, height);
	else if(strcmp(decoder, "mpeg4") == 0)
	 	descr = g_strdup_printf ("rtspsrc location=%s latency=%d ! gstrtpjitterbuffer ! rtpmp4vdepay ! vpudec output-format=1 frame-plus=10 low-latency=true ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", url, latency, left, top, width, height);
#else
 	descr = g_strdup_printf ("rtspsrc location=%s latency=%d ! rtph264depay ! h264parse ! ffdec_h264 ! ffmpegcolorspace ! autovideosink", url, latency);
#endif

	h->pipeline = gst_parse_launch (descr, &error);
	if (error == NULL)
	{
		/* we add a message handler */
		h->bus = gst_pipeline_get_bus (GST_PIPELINE (h->pipeline));
		h->bus_watch_id = gst_bus_add_watch (h->bus, bus_call, (gpointer)h);
		gst_object_unref (h->bus);

		/* Set the pipeline to "playing" state*/
//		g_print ("%s Now playing: %s (name %s)\n", TAG, url, name);
		gst_element_set_state (h->pipeline, GST_STATE_PLAYING);

		/* Iterate */
//		g_print ("%s Running...\n", TAG);
		g_main_loop_run (h->main_loop);
	}
	else
	{
		g_print ("%s could not construct rtsp pipeline for %s: %s\n", TAG, url, error->message);
		g_error_free (error);
	}

	/* Out of the main loop, clean up nicely */
//	g_print ("%s Returned, stopping playback for %s (name %s)\n", TAG, url, name);
	gst_element_set_state (h->pipeline, GST_STATE_NULL);

//	g_print ("%s Deleting pipeline for %s (name %s)\n", TAG, url, name);
	gst_object_unref (GST_OBJECT (h->pipeline));
	g_source_remove (h->bus_watch_id);
	g_main_loop_unref (h->main_loop);
	free(h);
	return 0;
}

/*
	Stop pipeline then received SIGINT
*/
void sig_handler(int signum)
{
//    g_print("%s Received signal %d\n", TAG, signum);
	g_main_loop_quit (mosaic->main_loop);
}

#if 0
	int ret;
	char gst[1024] = {0};
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
#endif

