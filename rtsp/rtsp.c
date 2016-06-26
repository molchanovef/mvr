#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <gst/gst.h>
#include <glib.h>

#define TAG			"RTSP:"
#define DECODER		"h264"

GMainLoop *main_loop;  /* GLib's Main Loop */
GstElement 	*pipeline, *rtspsrc, *vsink;
#ifdef AUDIO
GstElement  *asrc, *asink, *alsasrc, *mp3enc;
#endif
GstBus *bus;
guint bus_watch_id;

char url[1024];
char decoder[16];

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);

void print_usage(char *app)
{
	printf("%s application connects to IPCAM using given url\n", app);
	printf("for get and save frames in tmpfs directory.\n");
	printf("Another programs (mosaic, record) reads frames\n");
	printf("from tmpfs for them own purposes.\n");
	printf("Usage %s <rtsp url> <name>\n", app);
}

/* The vsink has received a buffer */
static void new_video_buffer (GstElement *sink) {
	GstBuffer *buffer;

	/* Retrieve the buffer */
	g_signal_emit_by_name (vsink, "pull-buffer", &buffer, NULL);
	g_print ("*");
	/* Push the buffer into /tvh/av */
	//TODO
	gst_buffer_unref (buffer);
}
#ifdef AUDIO
/* The asink has received a buffer */
static void new_audio_buffer (GstElement *sink) {
	GstBuffer *buffer;
	/* Retrieve the buffer */
	g_signal_emit_by_name (asink, "pull-buffer", &buffer, NULL);
	g_print ("^");

	/* Push the buffer into /tvh/av */
	//TODO
	gst_buffer_unref (outbuf);
}
#endif

int main(int argc, char* argv[])
{
	gchar *descr;
	GError *error = NULL;
	int avidx, latency;
	
	if(argc < 4)
		print_usage(argv[0]);
	else
	{
		strcpy(url, argv[1]);
		strcpy(decoder, argv[2]);
		avidx = atoi(argv[3]);
		latency = atoi(argv[4]);
	}

	/* Initialisation */
	gst_init (&argc, &argv);

	main_loop = g_main_loop_new (NULL, FALSE);
//	Video
#ifdef CROSS
	if(strcmp(decoder, "h264") == 0)
	 	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s latency=%d ! gstrtpjitterbuffer ! rtph264depay ! appsink name=vsink", url, latency);
	else if(strcmp(decoder, "mpeg4") == 0)
	 	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s latency=%d ! gstrtpjitterbuffer ! rtpmp4vdepay ! appsink name=vsink", url, latency);
#else
 	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s latency=%d ! rtph264depay ! appsink name=vsink", url, latency);
#endif
//	with mp3 audio
//	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s name=src ! gstrtpjitterbuffer ! rtph264depay ! appsink name=vsink src. ! rtpmpadepay ! appsink name=asink", url);
//	with aac audio
//	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s ! gstrtpjitterbuffer ! rtph264depay ! appsink name=vsink rtspsrc. ! rtppcmudepay ! appsink name=asink", url);
//
	pipeline = gst_parse_launch (descr, &error);
	if (error != NULL) {
		g_print ("%s could not construct rtsp pipeline for %s: %s\n", TAG, url, error->message);
		g_error_free (error);
		goto finish;
	}
	rtspsrc = gst_bin_get_by_name(GST_BIN(pipeline), "rtspsrc");
	vsink = gst_bin_get_by_name(GST_BIN(pipeline), "vsink");
	/* Configure vsink */
	g_object_set (vsink, "sync", FALSE, "max-buffers", "1000", "drop", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect(vsink, "new-buffer", G_CALLBACK(new_video_buffer), NULL);
#ifdef AUDIO
	asink = gst_bin_get_by_name(GST_BIN(pipeline), "asink");
	/* Configure asink */
	g_object_set (asink, "sync", FALSE, "max-buffers", "1000", "drop", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect(asink, "new-buffer", G_CALLBACK(new_audio_buffer), NULL);
#endif

	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, "appsink");
	gst_object_unref (bus);
	/* Set the pipeline to "playing" state*/
//	g_print ("%s Now playing: %s duration %d baseDir %s\n", TAG, url, duration, baseDir);
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	/* Iterate */
//	g_print ("%s Running...\n", TAG);
	g_main_loop_run (main_loop);

finish:
	/* Out of the main loop, clean up nicely */
//	g_print ("%s Returned, stopping playback\n", TAG);
	gst_element_set_state (pipeline, GST_STATE_NULL);

//	g_print ("%s Deleting pipeline\n", TAG);
	gst_object_unref (GST_OBJECT (pipeline));
	gst_object_unref (GST_OBJECT (rtspsrc));
	gst_object_unref (GST_OBJECT (vsink));
#ifdef AUDIO
	gst_object_unref (GST_OBJECT (asink));
#endif
	g_source_remove (bus_watch_id);
	g_main_loop_unref (main_loop);
	return 0;
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	char *str = (char*)data;

	switch (GST_MESSAGE_TYPE (msg))
	{
		case GST_MESSAGE_EOS:
		{
			g_print ("%s End of stream pipeline %s\n", TAG, str);
			if(strcmp(str, "appsink") == 0)
					g_main_loop_quit (main_loop);
			break;
		}
		case GST_MESSAGE_ERROR:
		{
			gchar  *debug;
			GError *error;
			gst_message_parse_error (msg, &error, &debug);
			g_printerr ("Error: %s\n", error->message);
			g_error_free (error);
			if (debug)
			{
				g_printerr ("[Debug details: %s]\n", debug);
				g_free (debug);
			}
			g_main_loop_quit (main_loop);
			break;
		}
		default:
		break;
	}
	return TRUE;
}

