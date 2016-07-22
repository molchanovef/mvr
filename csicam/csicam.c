#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <gst/gst.h>
#ifdef BTRLIB
#include <btr.h>
#include <pthread.h>
#else
#include <scd.h>
#endif

/*
	This program captures frames from built-in csi camera through scd driver.
	There are two ways to do it with USE_BTRLIB in Makefile:
	1. Using btr from ipclib
	2. Using bayer2rgb plugin
	Creates appsrc gstreamer pipeline with tee for two tasks:
	1. Show live video with mfw_isink
	2. Encode frames with vpuenc for recording.
*/

#define DW		800
#define DH		480
#define BPP		3
#define FREE(p) {if(p != NULL) free(p);}
#define TAG "CSICAM: "

typedef struct _Csicam
{
	GstElement 	*pipeline;
	GMainLoop *main_loop;  /* GLib's Main Loop */
	GstBus *bus;
	guint bus_watch_id;
	GstElement *appsrc;
	GstElement *filesink;

	guint sourceid;

	GTimer *timer;
#ifdef BTRLIB
	int width;
	int height;
	int fps;
#else
	mt9_setup_t sensor;
	int fd_scd;
	int gain;
	char *sensorBuffer;
#endif
} Csicam;

Csicam *app;

void sig_handler(int signum);

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer ptr)
{
	Csicam *h = (Csicam*)ptr;
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

#ifdef BTRLIB
//static gboolean read_data (Csicam * app)
int appsrc_push(char* argb, int size)
{
    GstFlowReturn ret;
	GstBuffer *buffer;
    gboolean ok = TRUE;

//	g_print("argb %d size %d\n", argb, size);

	if(size == app->width*app->height*BPP && argb != NULL)
	{
		buffer = gst_buffer_new();
		GST_BUFFER_SIZE (buffer) = size;
		GST_BUFFER_MALLOCDATA (buffer) = g_malloc (size);
		GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer);

		memcpy(buffer->data, argb, size);

		GST_DEBUG ("feed buffer");
		g_signal_emit_by_name (app->appsrc, "push-buffer", buffer, &ret);
		gst_buffer_unref (buffer);

		if (ret != GST_FLOW_OK) {
			/* some error, stop sending data */
			GST_DEBUG ("some error");
			ok = FALSE;
		}
		return ok;
	}

    //  g_signal_emit_by_name (app->appsrc, "end-of-stream", &ret);
    return FALSE;
}
#else
static gboolean read_data (Csicam * app)
{
    guint len;
    GstFlowReturn ret;
	GstBuffer *buffer;
	int bytes_read;

	len = app->sensor.w*app->sensor.h;
	bytes_read = pread(app->fd_scd, app->sensorBuffer, len, 0);
	if(bytes_read == len)
	{
		buffer = gst_buffer_new();
		GST_BUFFER_SIZE (buffer) = len;
		GST_BUFFER_MALLOCDATA (buffer) = g_malloc (len);
		GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer);

		memcpy(buffer->data, app->sensorBuffer, len);

//	    g_print ("feed buffer");
	    g_signal_emit_by_name (app->appsrc, "push-buffer", buffer, &ret);
	    gst_buffer_unref (buffer);
	}

    //  g_signal_emit_by_name (app->appsrc, "end-of-stream", &ret);
    return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed (GstElement * pipeline, guint size, Csicam * app)
{
  if (app->sourceid == 0) {
//    g_print ("start feeding");
    app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
  }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed (GstElement * pipeline, Csicam * app)
{
  if (app->sourceid != 0) {
//    g_print ("stop feeding");
    g_source_remove (app->sourceid);
    app->sourceid = 0;
  }
}
#endif

void print_usage(char *name)
{
	printf("Usage %s <width> <height> <fps> <gain> <skip 0 or 1> <bin 0 or 1>\n", name);
	printf("OPTIONS: output window [left] [top] [width] [height]\n");
	exit(1);
}

int main(int argc, char* argv[])
{
	int ret;
	gchar *descr;
	GstCaps *caps;
	GError *error = NULL;
	int sw, sh, fps, gain, left, top, width, height, skip, bin;

	left = 0;
	top = 0;
	width = DW;
	height = DH;
	
	if(argc < 7)
		print_usage(argv[0]);
	else
	{
		sw = atoi(argv[1]);
		sh = atoi(argv[2]);
		fps = atoi(argv[3]);
		gain = atoi(argv[4]);
		skip = atoi(argv[5]);
		bin = atoi(argv[6]);
		if(argc == 11)
		{
			left = atoi(argv[7]);
			top = atoi(argv[8]);
			width = atoi(argv[9]);
			height = atoi(argv[10]);
		}
	}

	if(skip > 1) skip = 1;
	if(skip < 0) skip = 0;
	if(bin > 1) bin = 1;
	if(bin < 0) bin = 0;
	
	app = malloc(sizeof(Csicam));
	if(app == NULL)
	{
		printf("Memory allocation error\n");
		goto finish;
	}

#ifdef BTRLIB
	printf("using btrlib\n");
	app->width = sw;
	app->height = sh;
	app->fps = fps;
	ret = btr_init(sw, sh, fps, gain, B2RGB_SIMPLE, appsrc_push);
	if(ret != 0)
		goto finish;
#else
	printf("using bayer2rgb plugin\n");
    if ((app->fd_scd = open("/dev/scd",O_RDWR)) < 0) {
		printf("Failed to open scd\n");
		goto finish;
    }
    else
    {
		app->sensor.cs[0] = -1;
		app->sensor.rs[0] = -1;
		app->sensor.cs[1] = -1;
		app->sensor.rs[1] = -1;
		app->sensor.fps = fps;
		app->sensor.w = sw;
		app->sensor.h = sh;
		app->sensor.skip = skip;
		app->sensor.bin = bin;
		ret = ioctl(app->fd_scd,IOCTL_SCD_CAMERA_SETUP,&app->sensor);
		if(ret !=0)
		{
			printf("SCD return %d on CAMERA_SETUP\n",ret);
			goto finish;
		}
		app->gain = gain;
		ret = ioctl(app->fd_scd,IOCTL_SCD_CAMERA_GAIN,app->gain);
		if(ret !=0)
		{
			printf("SCD return %d on CAMERA_GAIN\n",ret);
			goto finish;
		}
	}
	app->sensorBuffer = malloc(app->sensor.w*app->sensor.h);
	if(app->sensorBuffer == NULL)
	{
		printf("Memory allocation error\n");
		goto finish;
	}
#endif

	signal(SIGINT, sig_handler);

	/* Initialisation */
	gst_init (&argc, &argv);

	app->main_loop = g_main_loop_new (NULL, FALSE);
	app->timer = g_timer_new();

#ifdef BTRLIB
	descr = g_strdup_printf ("appsrc name=appsrc ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", left, top, width, height);
#else
	#ifdef GRAYMODE
		descr = g_strdup_printf ("appsrc name=appsrc ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", left, top, width, height);
	#else
		descr = g_strdup_printf ("appsrc name=appsrc ! bayer2rgb ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", left, top, width, height);
	#endif
#endif
//	descr = g_strdup_printf ("appsrc name=appsrc ! vpuenc codec=0 ! avimux ! filesink name=filesink");
//	descr = g_strdup_printf ("imxv4l2src ! vpuenc codec=0 ! avimux ! filesink name=filesink");
	app->pipeline = gst_parse_launch (descr, &error);
	if (error == NULL)
	{
//		filesink = gst_bin_get_by_name(GST_BIN(pipeline), "filesink");
		app->appsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "appsrc");
		/* set the caps on the source */
#ifdef BTRLIB
		caps = gst_caps_new_simple (
		"video/x-raw-rgb",
		"framerate", GST_TYPE_FRACTION, fps, 1,
		"bpp",G_TYPE_INT,BPP*8,
		"depth",G_TYPE_INT,BPP*8,
		"width", G_TYPE_INT, sw,
		"height", G_TYPE_INT, sh,
		NULL);
#else
		g_signal_connect (app->appsrc, "need-data", G_CALLBACK (start_feed), app);
		g_signal_connect (app->appsrc, "enough-data", G_CALLBACK (stop_feed), app);
#ifdef GRAYMODE
		caps = gst_caps_new_simple (
		"video/x-raw-gray",
		"framerate", GST_TYPE_FRACTION, fps, 1,
		"bpp",G_TYPE_INT,8,
		"depth",G_TYPE_INT,8,
		"width", G_TYPE_INT, sw,
		"height", G_TYPE_INT, sh,
		NULL);
#else
		/* set the caps on the bayer2rgb */
		caps = gst_caps_new_simple (
		"video/x-raw-bayer",
		"framerate", GST_TYPE_FRACTION, app->sensor.fps, 1,
		"format",G_TYPE_STRING, "grbg",
		"width", G_TYPE_INT, app->sensor.w,
		"height", G_TYPE_INT, app->sensor.h,
		NULL);
#endif

#endif
		g_object_set (G_OBJECT (app->appsrc), "caps", caps, NULL);
		g_object_set (G_OBJECT (app->appsrc),
				"stream-type", 0,
				"format", GST_FORMAT_TIME, NULL);
		/* we add a message handler */
		app->bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
		app->bus_watch_id = gst_bus_add_watch (app->bus, bus_call, (gpointer)app);
		gst_object_unref (app->bus);

		/* Set the pipeline to "playing" state*/
		g_print ("%s Now playing\n", TAG);
		gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

		/* Iterate */
		g_print ("%s Running...\n", TAG);
		g_main_loop_run (app->main_loop);
	}
	else
	{
		g_print ("%s could not construct appsrc pipeline %s\n", TAG, error->message);
		g_error_free (error);
	}

	/* Out of the main loop, clean up nicely */
	g_print ("%s Returned, stopping playback\n", TAG);
	gst_element_set_state (app->pipeline, GST_STATE_NULL);

	g_print ("%s Deleting pipeline\n", TAG);
	gst_object_unref (GST_OBJECT (app->pipeline));
	g_source_remove (app->bus_watch_id);
	g_main_loop_unref (app->main_loop);


finish:
#ifndef BTRLIB
	close(app->fd_scd);
	FREE(app->sensorBuffer);
#endif
	FREE(app);
	return 0;
}

void sig_handler(int signum)
{
//    g_print("%s Received signal %d\n", TAG, signum);
	g_main_loop_quit (app->main_loop);
}

