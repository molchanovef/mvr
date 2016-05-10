#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h> //fork
#include <sys/stat.h>//mkdir
#include <signal.h>
#include "local.h"

//#define URL "rtsp://admin:9999@192.168.11.94:8555/Stream2"
#define URL				"rtsp://admin:admin@192.168.1.200/0"
#ifdef CROSS
#define BASE_DIR 		"/media/sda1/MVR"
//#define BASE_DIR 		"/media/Videos/MVR"
#else
#define BASE_DIR 		"/home/mef/Videos/MVR"
#endif
#define DURATION	10 //seconds

typedef struct _Rec Rec;

struct _Rec
{
	GMainLoop 	*main_loop;  /* GLib's Main Loop */
	GstElement 	*pipeline;
	GstElement	*appsink;
	GstElement	*pipeline_out;
	GstElement	*appsrc;
	GstElement	*mp4mux;
	GstElement	*filesink;
	GstElement	*rtspsrc;
	guint 		sourceid;
	GTimer		*timer;
};

Rec s_rec;
gboolean firstTime = TRUE;
gboolean outStopped = FALSE;
guint bus_watch_id;

struct timespec tstart, tcurr;
char url[1024];
char baseDir[64];
int duration;
char camFolder[256];
char workFolder[256];

int run_pipeline_out(Rec *rec);
void* control_func (void *arg);

void print_help(char *argv)
{
	printf("Usage %s <duration> <base dir> <rtsp url>\n",argv);
	printf("h - Help\n");
	printf("x - eXit\n");
}

/*
Get SIGINT for correct stop all pipelines
Send EOS to appsrc pipeline
Stop main loop
*/
void sig_handler(int signum)
{
	Rec * rec = &s_rec;
	GstEvent* event;
    printf("Received signal %d\n", signum);
	//send eos to appsrc of out pipeline
	event = gst_event_new_eos();
	gst_element_send_event (rec->appsrc, event);
	//Stop main loop
	g_main_loop_quit (rec->main_loop);
}

int createWorkFolder(char * workFolder)
{
	int status;
	char tmp[256];
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	if(strlen(camFolder) == 0) return -1;
	sprintf(tmp, "%s/%04d_%02d_%02d", baseDir, 1900+t->tm_year, t->tm_mon+1, t->tm_mday);
	status = mkdir((const char *)tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	sprintf(workFolder, "%s/%s", tmp, camFolder);
	status = mkdir((const char *)workFolder, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	if(status != 0)
	{
//		g_print("Error create %s directory: %s (%d)\n", workFolder, strerror(errno), errno);
		if(errno == EEXIST)
			status = 0;
	}
	return status;
}

/*
Search for SPS in stream for correct split stream on files
*/
gboolean isSPSpacket(guint8 * paket)
{
	int RTPHeaderBytes = 3;
//	int fragment_type = paket[RTPHeaderBytes + 0] & 0x1F;
	int nal_type = paket[RTPHeaderBytes + 1] & 0x1F;
//	int start_bit = paket[RTPHeaderBytes + 1] & 0x80;
//	g_print("%s fragment %x nal_type %x start_bit %x\n", __func__, fragment_type, nal_type, start_bit);
	if (nal_type == 7)
	{
		return TRUE;
	}
	return FALSE;
}

gboolean isTimeout()
{
	int ret;
	int elapsedTime;

	ret = clock_gettime(CLOCK_REALTIME, &tcurr);
	if(ret == 0)
	{
		// compute and print the elapsed time in millisec
		elapsedTime = (tcurr.tv_sec - tstart.tv_sec) * 1000.0;      // sec to ms
		elapsedTime += (tcurr.tv_nsec - tstart.tv_nsec) / 1000000.0;   // ns to ms
	}  

	if (elapsedTime >= duration*1000)
		return TRUE;
	return FALSE;
}

int createNewRecordFile(char * fn)
{
	int ret;
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	do
	{
		ret = createWorkFolder(workFolder);
		if( ret != 0 )
			break;
		sprintf(fn,"%s/%02d_%02d_%02d.mp4", workFolder, t->tm_hour, t->tm_min, t->tm_sec);
	}while(0);
	return ret;
}

static gboolean read_data (Rec * rec)
{
	GstFlowReturn ret;
	GstBuffer *buffer;
	
	buffer = gst_buffer_new();

	if(pop(buffer) == 0 && buffer->size > 0)
	{
		/* The only thing we do in this example is print a * to indicate a received buffer */
//		g_print ("*");
		if(isSPSpacket(buffer->data))
		{
//Workaround for tvhelp IP cameras
//		  SPS	
//0 0 0 1 67 64 0 29 ad 84 5 45 62 b8 ac 54 74 20 2a 2b 15 c5 62 a3 a1 1 51 58 ae 2b 15 1d 8 a 8a c5 71 58 a8 e8 40 54 56 2b 8a c5 47 42 2 a2 b1 5c 56 2a 3a 10 24 85 21 39 3c 9f 27 e4 fe 4f c9 f2 79 b9 b3 4d
//																																		error here	    PPS
// 8 12 42 90 9c 9e 4f 93 f2 7f 27 e4 f9 3c dc d9 a6 b4 2 80 2d d2 a4 0 0 3 1 e0 0 0 70 81 81 0 1 f4 0 0 3 2 32 81 bd ef 85 e1 10 8d 40 0 0 0 1 0 0 0 1 68 ee 3c b0 0 0 0 1 61		
int i;
/*g_print("before\n");
for(i = 0; i < buffer->size; i++)
g_print("%x ",buffer->data[i]);
g_print("\n");*/
//Remove 0 0 0 1
for(i = 0; i < buffer->size-7; i++)
{
	if(buffer->data[i] == 0 && buffer->data[i+1] == 0 && buffer->data[i+2] == 0 && buffer->data[i+3] == 1 &&
	   buffer->data[i+4] == 0 && buffer->data[i+5] == 0 && buffer->data[i+6] == 0 && buffer->data[i+7] == 1)
	{
		g_print("!!! %d\n",i);
		memcpy(&buffer->data[i],&buffer->data[i+4],buffer->size - i - 4);
	}
}
/*g_print("after\n");
for(i = 0; i < buffer->size; i++)
g_print("%x ",buffer->data[i]);
g_print("\n");*/

			if(isTimeout())
			{
					//send eos to appsrc of out pipeline
					GstEvent* event = gst_event_new_eos();
					gst_element_send_event (rec->appsrc, event);
//					gst_element_set_state (rec->pipeline_out, GST_STATE_NULL);
					gst_element_set_state (rec->appsrc, GST_STATE_PAUSED);
//					sleep(1);//TODO wait for change state of out pipeline
			}
		}
		
		/* Push the buffer into the appsrc */
		if(rec->pipeline_out)
		{
			g_signal_emit_by_name (rec->appsrc, "push-buffer", buffer, &ret);
			if(ret != GST_FLOW_OK)
				g_print("ERROR!!! Can't push buffer to appsrc (%d)\n",ret);
		}
	gst_buffer_unref (buffer);
	return TRUE;
	}

//	g_signal_emit_by_name (app->appsrc, "end-of-stream", &ret);
	return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed (GstElement * pipeline, guint size, Rec * rec)
{
  if (rec->sourceid == 0) {
    GST_DEBUG ("start feeding");
    rec->sourceid = g_idle_add ((GSourceFunc) read_data, rec);
  }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed (GstElement * pipeline, Rec * rec)
{
  if (rec->sourceid != 0) {
    GST_DEBUG ("stop feeding");
    g_source_remove (rec->sourceid);
    rec->sourceid = 0;
  }
}


static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	Rec *rec = (Rec*)data;
	switch (GST_MESSAGE_TYPE (msg))
	{
		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state;
			gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
			if(strcmp(GST_OBJECT_NAME (msg->src), "appsrc") == 0)
			{
					g_print("appsrc changed from %s %s\n",
							gst_element_state_get_name (old_state),
							gst_element_state_get_name (new_state));
				if(old_state == GST_STATE_PLAYING && new_state == GST_STATE_PAUSED)
				{
					g_print("!!!!!!!\n");
#if 1
						gst_element_set_state (rec->pipeline_out, GST_STATE_NULL);
						gst_object_unref (GST_OBJECT (rec->appsrc));
						gst_object_unref (GST_OBJECT (rec->filesink));
						gst_object_unref (GST_OBJECT (rec->pipeline_out));
						rec->pipeline_out = NULL;
					if(0 != run_pipeline_out(rec))
					{
						g_print("could not construct out pipeline\n");
						//Stop main loop
						g_main_loop_quit (rec->main_loop);
					}
#endif
				}
			}
/*			g_print ("Element %s changed state from %s to %s.\n",
				GST_OBJECT_NAME (msg->src),
				gst_element_state_get_name (old_state),
				gst_element_state_get_name (new_state));
*/			break;
		}
		case GST_MESSAGE_EOS:
		{
			g_print ("End of stream pipeline %d\n",(int)data);
//			if((int)data == 1)//source pipeline
//				g_main_loop_quit (rec->main_loop);
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
			g_main_loop_quit (rec->main_loop);
			break;
		}
		default:
		break;
	}
	return TRUE;
}

int run_pipeline_out(Rec *rec)
{
	gchar *descr;
	GError *error = NULL;
	gint ret;
	GstBus *bus;
	char filename[256];
	
	g_print("%s\n",__func__);
	if (0 != createNewRecordFile(filename))
	{
		g_print("Error create new record file\n");
		return -1;
	}

	descr = g_strdup_printf ("appsrc name=appsrc ! h264parse ! mp4mux ! filesink name=filesink");
	rec->pipeline_out = gst_parse_launch (descr, &error);
	if (error != NULL) {
		g_print ("could not construct out pipeline: %s\n", error->message);
		g_error_free (error);
		return -2;
	}
	rec->appsrc = gst_bin_get_by_name(GST_BIN(rec->pipeline_out), "appsrc");
//	g_assert(rec->appsrc);
//    g_assert(GST_IS_APP_SRC(rec->appsrc));
    g_signal_connect (rec->appsrc, "need-data", G_CALLBACK (start_feed), rec);
    g_signal_connect (rec->appsrc, "enough-data", G_CALLBACK (stop_feed), rec);

	rec->filesink = gst_bin_get_by_name(GST_BIN(rec->pipeline_out), "filesink");
	g_object_set (G_OBJECT (rec->filesink), "location", filename, NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (rec->pipeline_out));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, rec);
	gst_object_unref (bus);

	ret = gst_element_set_state (rec->pipeline_out, GST_STATE_PLAYING);
	g_print("Out pipeline %s %d\n", filename,ret);
	clock_gettime(CLOCK_REALTIME, &tstart);
	return 0;
}

/* The appsink has received a buffer */
static void new_buffer (GstElement *sink, Rec *rec) {
	GstBuffer *buffer;

	/* Retrieve the buffer */
	g_signal_emit_by_name (rec->appsink, "pull-buffer", &buffer, NULL);
	if (buffer)
	{
		if(firstTime)
		{
			if(0 != run_pipeline_out(rec))
			{
				g_print("could not construct out pipeline\n");
				gst_buffer_unref (buffer);
				//Stop main loop
				g_main_loop_quit (rec->main_loop);
			}
			firstTime = FALSE;
		}
		push(buffer);
		gst_buffer_unref (buffer);
	}
}

void searcIPinURL(char *url, char *camFolder)
{
	int i;
	char *p = url;
	char *o = camFolder;
	int index = 0;

	for(i = 0; i < strlen(url); i++)
	{
		if(*p++ == '@')
		{
			index = ++i;
			break;
		}
	}
	p = url;
//	printf("index %d\n",index);
	if(index == 0)//search //
	{
		printf("url without autorization\n");
		if(strncmp(url,"rtsp://",6) == 0)
		{
//			printf("rtsp found\n");
			index = 7;
		}
	}
	p = &url[index];
	while((*p == '.') || (*p <= '9' && *p >= '0') || (*p == ':'))
	{
		*o++ = *p++;
	}
	printf("camFolder %s\n",camFolder);
}

int main(int argc, char* argv[])
{
	const gchar *nano_str;
	guint major, minor, micro, nano;
	gchar *descr;
	GError *error = NULL;
	GstBus *bus;

	Rec *rec = &s_rec;
	memset(rec, 0, sizeof(Rec));
	
	print_help(argv[0]);
	if(argc < 2)
		duration = DURATION;
	else
		duration = atoi(argv[1]);
	if(argc < 3)
		strcpy(baseDir, BASE_DIR);
	else
		strcpy(baseDir,argv[2]);
	if(argc < 4)
		strcpy(url, URL);
	else
		strcpy(url, argv[3]);
		
	printf("duration %d\n",duration);
	printf("baseDir %s\n",baseDir);
	printf("url %s\n",url);

	searcIPinURL(url, camFolder);
	if(0 != createWorkFolder(workFolder))
		return -1;
		
	signal(SIGINT, sig_handler);

	/* Initialisation */
	gst_init (&argc, &argv);
	gst_version (&major, &minor, &micro, &nano);
	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";
	printf ("This program is linked against GStreamer %d.%d.%d %s\n", major, minor, micro, nano_str);

	rec->main_loop = g_main_loop_new (NULL, FALSE);

	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s ! gstrtpjitterbuffer ! rtph264depay ! appsink name=appsink", url);
	rec->pipeline = gst_parse_launch (descr, &error);
	if (error != NULL) {
		g_print ("could not construct rtsp pipeline for %s: %s\n",url, error->message);
		g_error_free (error);
		goto finish;
	}
	rec->rtspsrc = gst_bin_get_by_name(GST_BIN(rec->pipeline), "rtspsrc");
	rec->appsink = gst_bin_get_by_name(GST_BIN(rec->pipeline), "appsink");
	/* Configure appsink */
	g_object_set (rec->appsink, "sync", FALSE, "max-buffers", "200", "drop", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect(rec->appsink, "new-buffer", G_CALLBACK(new_buffer), rec);

	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (rec->pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, rec);
	gst_object_unref (bus);
	/* Set the pipeline to "playing" state*/
//	g_print ("Now playing: %s\n", argv[3]);
	gst_element_set_state (rec->pipeline, GST_STATE_PLAYING);
	/* Iterate */
	g_print ("Running...\n");
	g_main_loop_run (rec->main_loop);
	
finish:
	/* Out of the main loop, clean up nicely */
	g_print ("Returned, stopping playback\n");
	gst_element_set_state (rec->pipeline, GST_STATE_NULL);
	gst_element_set_state (rec->pipeline_out, GST_STATE_NULL);
	gst_element_set_state (rec->appsink, GST_STATE_NULL);

	g_print ("Deleting pipeline\n");
	clear();
	gst_object_unref (GST_OBJECT (rec->pipeline));
	gst_object_unref (GST_OBJECT (rec->rtspsrc));
	gst_object_unref (GST_OBJECT (rec->appsink));
	gst_object_unref (GST_OBJECT (rec->pipeline_out));
	gst_object_unref (GST_OBJECT (rec->appsrc));
	gst_object_unref (GST_OBJECT (rec->filesink));
	g_source_remove (bus_watch_id);
	g_main_loop_unref (rec->main_loop);
	return 0;
}

