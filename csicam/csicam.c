#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <scd.h>
#include <pthread.h>
#include <gst/gst.h>
#include <scd.h>
#include <btr.h>

/*
	This program captures frames from built-in csi camera through scd driver.
	Creates appsrc gstreamer pipeline with tee for two tasks:
	1. Show live video with mfw_isink
	2. Encode frames with vpuenc for recording.
*/

#define DW		800
#define DH		480
#define WIDTH	800
#define HEIGHT	480
#define FPS		30
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

	int fd_scd;
	int width;
	int height;
	int fps;
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

//static gboolean read_data (Csicam * app)
int appsrc_push(char* argb, int size)
{
    GstFlowReturn ret;
	GstBuffer *buffer;
    gboolean ok = TRUE;

//	g_print("argb %d size %d\n", argb, size);

	if(size == WIDTH*HEIGHT*BPP && argb != NULL)
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

int main(int argc, char* argv[])
{
	int ret;
	gchar *descr;
	GstCaps *caps;
	GError *error = NULL;
	int left, top, width, height;
	
	if(argc == 5)
	{
		left = atoi(argv[1]);
		top = atoi(argv[2]);
		width = atoi(argv[3]);
		height = atoi(argv[4]);
	}
	else
	{
		left = 0;
		top = 0;
		width = DW;
		height = DH;
	}
	
	app = malloc(sizeof(Csicam));
	if(app == NULL)
	{
		printf("Memory allocation error\n");
		goto finish;
	}

	ret = btr_init(WIDTH, HEIGHT, FPS, B2RGB_SIMPLE, appsrc_push);
	if(ret != 0)
		goto finish;

	signal(SIGINT, sig_handler);

	/* Initialisation */
	gst_init (&argc, &argv);

	app->main_loop = g_main_loop_new (NULL, FALSE);
	app->timer = g_timer_new();
	g_timer_start(app->timer);

	descr = g_strdup_printf ("appsrc name=appsrc ! mfw_isink axis-left=%d axis-top=%d disp-width=%d disp-height=%d", left, top, width, height);
//	descr = g_strdup_printf ("appsrc name=appsrc ! vpuenc codec=0 ! avimux ! filesink name=filesink");
//	descr = g_strdup_printf ("imxv4l2src ! vpuenc codec=0 ! avimux ! filesink name=filesink");
	app->pipeline = gst_parse_launch (descr, &error);
	if (error == NULL)
	{
		app->appsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "appsrc");
//		filesink = gst_bin_get_by_name(GST_BIN(pipeline), "filesink");

		/* set the caps on the source */
		caps = gst_caps_new_simple (
		"video/x-raw-rgb",
		"framerate", GST_TYPE_FRACTION, FPS, 1,
		"bpp",G_TYPE_INT,BPP*8,
		"depth",G_TYPE_INT,BPP*8,
		"width", G_TYPE_INT, WIDTH,
		"height", G_TYPE_INT, HEIGHT,
		NULL);
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
	close(app->fd_scd);
	FREE(app);
	return 0;
}

void sig_handler(int signum)
{
//    g_print("%s Received signal %d\n", TAG, signum);
	g_main_loop_quit (app->main_loop);
}

