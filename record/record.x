#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h> //fork
#include <sys/stat.h>//mkdir
#include <gst/gst.h>
#include <glib.h>

//#define URL 			"rtsp://admin:9999@192.168.11.94:8555/Stream2"
#define URL				"rtsp://admin:admin@192.168.1.200/0"
//#define MVR_FOLDER 		"/media/sda1/MVR"
#define MVR_FOLDER 		"/media/Videos/MVR"
#define FILE_DURATION	10 //seconds
#define NUM_HEADER_BYTES	44

GMainLoop *main_loop;  /* GLib's Main Loop */
GstElement 	*pipeline, *rtspsrc, *sink, *pipeline_out[2], *appsrc[2], *filesink[2];
GstCaps *caps;
gint width;
gint height;
GstBus *bus;
guint bus_watch_id;
gboolean once_run_out_pipeline = FALSE;
struct timespec tstart, tcurr;
char *header;
int header_bytes_written = 0;
int currid = 0;

//int ping_pong_out_pipelines(void);
//int delete_out_pipeline(int id);
int create_out_pipeline(int id);
int filesink_out_pipeline(int id);
int play_out_pipeline(int id);
int delete_out_pipeline(int id);

void print_help(char *argv)
{
	printf("Usage %s <rtsp url>\n",argv);
}

int createDayFolder(char * df)
{
	int status;
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	sprintf(df, "%s/%04d_%02d_%02d", MVR_FOLDER, 1900+t->tm_year, t->tm_mon+1, t->tm_mday);
	status = mkdir((const char *)df, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	if(status != 0)
	{
//		g_print("Error create %s directory: %s (%d)\n", df, strerror(errno), errno);
		if(errno == EEXIST)
			status = 0;
	}
	return status;
}

int createNewRecordFile(char * fn)
{
	int ret;
	char df[256];
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	do
	{
		ret = createDayFolder(df);
		if( ret != 0 )
			break;
		sprintf(fn,"%s/%02d_%02d_%02d.mp4", df, t->tm_hour, t->tm_min, t->tm_sec);
	}while(0);
	return ret;
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	int id = (int)data;
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_print ("End of stream pipeline %d\n", id);
		if(id < 2)//out-pipeline
		{
			currid=1-currid;
			filesink_out_pipeline(currid);
			play_out_pipeline(currid);
			delete_out_pipeline(1-currid);
		}
		if((int)data == 2)//source pipeline
			g_main_loop_quit (main_loop);
	break;
	case GST_MESSAGE_ERROR: {
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
//		g_main_loop_quit (main_loop);
	break;
	}
	default:
	break;
	}
	return TRUE;
}

gboolean isH264iFrame(guint8 * paket)
{
	int RTPHeaderBytes = 4;
	int fragment_type = paket[RTPHeaderBytes + 0] & 0x1F;
	int nal_type = paket[RTPHeaderBytes + 1] & 0x1F;
	int start_bit = paket[RTPHeaderBytes + 1] & 0x80;
//	g_print("fragment %x nal_type %x start_bit %x\n", fragment_type, nal_type, start_bit);
	if (((fragment_type == 28 || fragment_type == 29) && nal_type == 5 && start_bit == 128) || fragment_type == 5)
	{
/*		g_print("I-frame found!!!\n");
		int i;
		for(i = 0; i < 20; i++)
			g_print("%x ", buffer->data[i]);
		g_print("\n");
*/		return TRUE;
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

	if (elapsedTime >= FILE_DURATION*1000)
		return TRUE;
	return FALSE;
}

int create_out_pipeline(int id)
{
	gchar *descr;
	GError *error = NULL;
	char tmp[16];

	g_print("%s %d\n",__func__, id);

	descr = g_strdup_printf ("appsrc name=appsrc%d ! h264parse ! mp4mux ! filesink name=filesink%d", id, id);
	pipeline_out[id] = gst_parse_launch (descr, &error);
	if (error != NULL) {
		g_print ("could not construct out pipeline%d: %s\n", id, error->message);
		g_error_free (error);
		return -2;
	}
	sprintf(tmp,"appsrc%d",id);
	appsrc[id] = gst_bin_get_by_name(GST_BIN(pipeline_out[id]), tmp);
	sprintf(tmp,"filesink%d",id);
 	filesink[id] = gst_bin_get_by_name(GST_BIN(pipeline_out[id]), tmp);
//		g_print("%s\n", gst_caps_to_string(caps));
	g_object_set (G_OBJECT (appsrc[id]), "caps", caps, NULL);

/*		g_object_set(G_OBJECT(appsrc), "is-live", TRUE, "block", FALSE,"do-timestamp", TRUE, 
			"stream-type", 0,
			 "format",GST_FORMAT_TIME, NULL);
*/
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline_out[id]));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, (gpointer)id);
	gst_object_unref (bus);
	return 0;
}

int filesink_out_pipeline(int id)
{
	char filename[256];
	int ret;
	
	if (0 != createNewRecordFile(filename))
	{
		g_print("Error create new record file\n");
		return -1;
	}
	g_print("%s %d %s\n",__func__, id, filename);
	ret = gst_element_set_state (filesink[id], GST_STATE_NULL);
	g_object_set(filesink[id], "location", filename, NULL);
	ret = gst_element_set_state (filesink[id], GST_STATE_PLAYING);
	
	if(header)
	{
		GstBuffer* newBuffer = gst_buffer_try_new_and_alloc(header_bytes_written);
		memcpy(GST_BUFFER_DATA(newBuffer), header, header_bytes_written);
		g_signal_emit_by_name (appsrc, "push-buffer", newBuffer, &ret);
		gst_buffer_unref(newBuffer);
	}
	return 0;
}

int play_out_pipeline(int id)
{
	int ret;
	//Set pipeline to playing state
	clock_gettime(CLOCK_REALTIME, &tstart);
	ret = gst_element_set_state (pipeline_out[id], GST_STATE_PLAYING);
	g_print("Out pipeline%d PLAYING (%d)\n", id, ret);
	return ret;
}

int pause_out_pipeline(int id)
{
	int ret;
	ret = gst_element_set_state (pipeline_out[id], GST_STATE_PAUSED);
	g_print("gst_element_set_state PAUSED %d\n", ret);
	return ret;
}
/*
int ping_pong_out_pipelines()
{
	gint ret;
	char filename[256];
	if (0 != createNewRecordFile(filename))
	{
		g_print("Error create new record file\n");
		return -1;
	}
	//Paused current out pipeline
	ret = gst_element_set_state (pipeline_out[currid], GST_STATE_NULL);
	g_print("gst_element_set_state NULL %d\n", ret);
	//ping-pong
	currid=1-currid;
	//set filename for next pipeline
	g_object_set (G_OBJECT (filesink[currid]), "location", filename, NULL);

	ret = gst_element_set_state (pipeline_out[currid], GST_STATE_PLAYING);
	g_print("Out pipeline%d PLAYING (%d) file %s\n", currid, ret, filename);
	clock_gettime(CLOCK_REALTIME, &tstart);
	return 0;
}
*/
/* The appsink has received a buffer */
static void new_buffer (GstElement *sink) {
	GstBuffer *buffer;
	GstStructure *structure;
	GstFlowReturn ret = GST_FLOW_OK;
	int i;
	/* Retrieve the buffer */
	g_signal_emit_by_name (sink, "pull-buffer", &buffer, NULL);
	if (buffer) {
		/* The only thing we do in this example is print a * to indicate a received buffer */
//		g_print ("*");
//		get_video_info(buffer);
		if(isH264iFrame(buffer->data) && (once_run_out_pipeline == TRUE))
		{
			if(isTimeout())
			{
				clock_gettime(CLOCK_REALTIME, &tstart);
				g_print("send eos event to appsrc%d\n", currid);
				GstEvent* event = gst_event_new_eos();
				gst_element_send_event (appsrc[currid], event);
			}
		}
		else//collect header bytes for next negotiation process
		{
//			g_print("size %d header_bytes_written %d\n", buffer->size, header_bytes_written);
			if(header_bytes_written + buffer->size <= NUM_HEADER_BYTES)
			{
				memcpy(&header[header_bytes_written], buffer->data, buffer->size);
				header_bytes_written += buffer->size;
				g_print("header_bytes_written %d\n",header_bytes_written);
				for(i = 0; i < buffer->size; i++)
					g_print("%x ",buffer->data[i]);
				g_print("\n");
			}
		}
		caps = gst_caps_copy(buffer->caps);
		structure = gst_caps_get_structure (GST_CAPS (buffer->caps), 0);
		gst_structure_get_int (structure, "width", &width);
		gst_structure_get_int (structure, "height", &height);
//		g_print("buffer size %d\n", buffer->size);
		if(once_run_out_pipeline == FALSE)
		{
			if(0 != create_out_pipeline(0))
				g_print("could not construct out pipeline0\n");
			if(0 != create_out_pipeline(1))
				g_print("could not construct out pipeline1\n");
			filesink_out_pipeline(0);
			play_out_pipeline(0);
			once_run_out_pipeline = TRUE;
		}
		/* Push the buffer into the appsrc */
		g_signal_emit_by_name (appsrc[currid], "push-buffer", buffer, &ret);
		if(ret != GST_FLOW_OK)
			g_print("ERROR!!! Can't push buffer to appsrc%d (%d)\n", currid, ret);
//		gst_buffer_unref (buffer);
	}
}

int main(int argc, char* argv[])
{
	const gchar *nano_str;
	guint major, minor, micro, nano;
	gchar *descr;
	GError *error = NULL;
	char url[1024] = {0};

	if(argc < 2)
	{
		print_help(argv[0]);
		strcpy(url, URL);
		printf("Default url %s\n", url);
	}	
	else
	{
		strcpy(url,argv[1]);
		printf("url %s\n", url);
	}

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
	header = (char*)malloc(NUM_HEADER_BYTES);

	main_loop = g_main_loop_new (NULL, FALSE);

	descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s ! gstrtpjitterbuffer ! rtph264depay ! appsink name=appsink", url);
	pipeline = gst_parse_launch (descr, &error);
	if (error != NULL) {
		g_print ("could not construct rtsp pipeline for %s: %s\n", url, error->message);
		g_error_free (error);
		goto error;
	}
	rtspsrc = gst_bin_get_by_name(GST_BIN(pipeline), "rtspsrc");
	sink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
	/* Configure sink */
	g_object_set (sink, "sync", FALSE, "max-buffers", "10", "drop", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect(sink, "new-buffer", G_CALLBACK(new_buffer), NULL);

	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, (gpointer)2);
	gst_object_unref (bus);
	/* Set the pipeline to "playing" state*/
	//  g_print ("Now playing: %s\n", argv[1]);
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	/* Iterate */
	g_print ("Running...\n");
	g_main_loop_run (main_loop);
	
error:
	/* Out of the main loop, clean up nicely */
	g_print ("Returned, stopping playback\n");

	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_element_set_state (pipeline_out[0], GST_STATE_NULL);
	gst_element_set_state (pipeline_out[1], GST_STATE_NULL);

	g_print ("Deleting pipeline\n");
	gst_object_unref (GST_OBJECT (pipeline));
	gst_object_unref (GST_OBJECT (pipeline_out[0]));
	gst_object_unref (GST_OBJECT (appsrc[0]));
	gst_object_unref (GST_OBJECT (filesink[0]));
	gst_object_unref (GST_OBJECT (pipeline_out[1]));
	gst_object_unref (GST_OBJECT (appsrc[1]));
	gst_object_unref (GST_OBJECT (filesink[1]));
	g_source_remove (bus_watch_id);

	g_main_loop_unref (main_loop);
	return 0;
}

