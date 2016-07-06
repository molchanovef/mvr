#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h> //fork
#include <sys/stat.h>//mkdir
#include <signal.h>
#include <gst/gst.h>
#include <glib.h>

#define URL				"rtsp://admin:admin@192.168.1.200/0"
#define DECODER			"h264"
#ifdef CROSS
#define BASE_DIR 		"/media/sda1/MVR"
//#define BASE_DIR 		"/media/Videos/MVR"
#else
#define BASE_DIR 		"/home/mef/Videos/MVR"
#endif
#define DURATION	10 //seconds
#define TAG			"RECORD:"

GstBuffer *SPSPacket;
GMainLoop *main_loop;  /* GLib's Main Loop */
GstElement 	*pipeline, *rtspsrc, *vsink;
GstElement	*pipeline_out, *vsrc, *filesink, *parse, *mux;
#ifdef AUDIO
GstElement  *asrc, *asink, *alsasrc, *mp3enc;
gboolean push_to_asrc = FALSE;
#endif
GstBus *bus;
guint bus_watch_id;
gboolean push_to_vsrc = FALSE;
gboolean signal_received = FALSE;

struct timespec tstart, tcurr;
char url[1024];
char decoder[16];
char baseDir[64];
int duration;
char camFolder[256];
char workFolder[256];
char filename[256];
char recfile[256];

int run_pipeline_out(void);

int cpRecToFile(void);
gboolean isSPSpacket(guint8 * paket);
gboolean isMPEG4iFrame(guint8 * paket);
gboolean isTimeout();
void sig_handler(int signum);
void searchIPinURL(char *url, char *camFolder);
int createWorkFolder(char * workFolder);
int createNewRecordFile(char * fn);
void tvhIPCAMworkaround(GstBuffer *buffer);
void print_buffer(GstBuffer *buf);

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	switch (GST_MESSAGE_TYPE (msg))
	{
		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state;
			gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
			if(strcmp(GST_OBJECT_NAME (msg->src), "vsrc") == 0)
			{
//					g_print("vsrc changed from %s %s\n",
//							gst_element_state_get_name (old_state),
//							gst_element_state_get_name (new_state));
				if(old_state == GST_STATE_PLAYING && new_state == GST_STATE_PAUSED)
				{
					gst_element_set_state (pipeline_out, GST_STATE_NULL);
					gst_object_unref (GST_OBJECT (vsrc));
					gst_object_unref (GST_OBJECT (parse));
					gst_object_unref (GST_OBJECT (mux));
#ifdef AUDIO
					gst_object_unref (GST_OBJECT (asrc));
					gst_object_unref (GST_OBJECT (alsasrc));
					gst_object_unref (GST_OBJECT (mp3enc));
					g_free(asrc);
					g_free(alsasrc);
					g_free(mp3enc);
#endif
					gst_object_unref (GST_OBJECT (filesink));
					gst_object_unref (GST_OBJECT (pipeline_out));
//					g_free(vsrc);
//					g_free(parse);
//					g_free(mux);
//					g_free(filesink);
//					g_free(pipeline_out);
					pipeline_out = NULL;
					
					if(cpRecToFile() != 0)
					{
						g_print("%s rename rec fail. Exit.\n", TAG);
						g_main_loop_quit (main_loop);
					}

					if( (signal_received == TRUE) )
					{
						unlink(recfile);
						g_main_loop_quit (main_loop);
					}
					else//create new recording pipeline
					{
						if(0 != run_pipeline_out())
						{
							g_print("%s could not construct out pipeline\n", TAG);
							//Stop main loop
							g_main_loop_quit (main_loop);
						}
					}
				}

				if(old_state == GST_STATE_PAUSED && new_state == GST_STATE_PLAYING)
				{
					push_to_vsrc = TRUE;
#ifdef AUDIO
					push_to_asrc = TRUE;
#endif
				}
			}
			break;
		}
		case GST_MESSAGE_EOS:
		{
//			g_print ("%s End of stream pipeline %d\n", TAG, (int)data);
			switch((int)data)
			{
				case 1:
					g_main_loop_quit (main_loop);
				break;
				case 2:
					gst_element_set_state (vsrc, GST_STATE_PAUSED);
#ifdef AUDIO
					gst_element_set_state (asrc, GST_STATE_PAUSED);
					gst_element_set_state (alsasrc, GST_STATE_PAUSED);
					gst_element_set_state (mp3enc, GST_STATE_PAUSED);
#endif
				break;				
			}
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

int run_pipeline_out()
{
	gchar *descr;
	GError *error = NULL;
	gint ret;
	gboolean alarm = FALSE;
	char raw[256];
	
	ret = createWorkFolder(workFolder);
	if( ret != 0 ) return ret;

	sprintf(recfile,"%s/rec",workFolder);

	//no one year_month_day.mp4 file created yet in new session
	if(strlen(filename) == 0)
	{		
		//we should check what previous session completed successfully
		if( access( recfile, F_OK ) != -1 )
		{
			// file exists
			g_print("%s !!! rec file exist !!!\n", TAG);
			sprintf(filename,"%s/%s",workFolder,"alarm");
//			alarm = TRUE;
			//save alarm file from previously incorrect session
			ret = rename(recfile, filename);
			if(ret != 0) return ret;
			g_print("%s %s file from previously incorrect session saved\n",TAG, filename);
		}
	}

	ret = createNewRecordFile(filename);
	if (ret != 0)
	{
		g_print("%s Error create new record file\n", TAG);
		return ret;
	}

	sprintf(raw,"%s/%s", workFolder, "raw.h264");
	
	if(alarm)
	{
		descr = g_strdup_printf ("filesrc location=%s ! h264parse ! mp4mux ! filesink name=filesink", recfile);
		pipeline_out = gst_parse_launch (descr, &error);
		if (error != NULL) {
			g_print ("%s could not construct out pipeline: %s\n", TAG, error->message);
			g_error_free (error);
			return -2;
		}
		filesink = gst_bin_get_by_name(GST_BIN(pipeline_out), "filesink");
		g_object_set (G_OBJECT (filesink), "location", filename, NULL);
	}
	else
	{
//		Video to mp4 file
		if(strcmp(decoder, "h264") == 0)
			descr = g_strdup_printf ("appsrc name=vsrc ! h264parse name=parse ! mp4mux name=mux ! filesink name=filesink");
		else if(strcmp(decoder, "mpeg4") == 0)
			descr = g_strdup_printf ("appsrc name=vsrc ! mpeg4videoparse name=parse ! mp4mux name=mux ! filesink name=filesink");
//		video to mp4 file mp3 audio to alsasink
//		descr = g_strdup_printf (" mp4mux name=mux ! filesink name=filesink appsrc name=vsrc ! h264parse ! queue ! mux.video_0 appsrc name=asrc typefind=true ! beepdec ! audioconvert ! alsasink");
//		video with pcm audio to mp4 file
//		descr = g_strdup_printf ("mp4mux name=mux ! filesink name=filesink appsrc name=vsrc ! h264parse ! queue ! mux.video_0 appsrc name=asrc ! audioparse ! caps=\"audio/x-raw-int, rate=(int)32000, channels=(int)1\" mfw_mp3encoder ! queue ! mux.audio_0");
//		video with mic audio to mp4 file
//		descr = g_strdup_printf ("mp4mux name=mux ! filesink name=filesink appsrc name=vsrc ! h264parse ! queue ! mux.video_0 alsasrc name=alsasrc ! caps=\"audio/x-raw-int, rate=(int)32000, channels=(int)1\" mfw_mp3encoder name=mp3enc ! queue ! mux.audio_0");
//		video to mp4 file, audio to alsasink		
//		descr = g_strdup_printf ("mp4mux name=mux ! filesink name=filesink appsrc name=vsrc ! h264parse ! queue ! mux. appsrc name=asrc ! queue ! decodebin ! audioconvert ! alsasink");
//		descr = g_strdup_printf ("appsrc name=vsrc ! tee name=t t. ! queue ! h264parse ! mp4mux ! filesink name=filesink  t. ! queue ! filesink location=%s",raw);
		pipeline_out = gst_parse_launch (descr, &error);
		if (error != NULL) {
			g_print ("%s could not construct out pipeline: %s\n", TAG, error->message);
			g_error_free (error);
			return -2;
		}
		vsrc = gst_bin_get_by_name(GST_BIN(pipeline_out), "vsrc");
		parse = gst_bin_get_by_name(GST_BIN(pipeline_out), "parse");
		mux = gst_bin_get_by_name(GST_BIN(pipeline_out), "mux");
#ifdef AUDIO
		asrc = gst_bin_get_by_name(GST_BIN(pipeline_out), "asrc");
		alsasrc = gst_bin_get_by_name(GST_BIN(pipeline_out), "alsasrc");
		mp3enc = gst_bin_get_by_name(GST_BIN(pipeline_out), "mp3enc");
#endif
		filesink = gst_bin_get_by_name(GST_BIN(pipeline_out), "filesink");
		g_object_set (G_OBJECT (filesink), "location", recfile, NULL);
	}
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline_out));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, (gpointer)2);
	gst_object_unref (bus);

	ret = gst_element_set_state (pipeline_out, GST_STATE_PLAYING);
	g_print("%s %s\n", TAG, filename);

	if(!alarm)
	{
		clock_gettime(CLOCK_REALTIME, &tstart);
		push_to_vsrc = TRUE;
#ifdef AUDIO
		push_to_asrc = TRUE;
#endif
	}
	return 0;
}

/* The vsink has received a buffer */
static void new_video_buffer (GstElement *sink) {
	GstBuffer *buffer;
	GstFlowReturn ret = GST_FLOW_OK;

	if(push_to_vsrc)
	{
		if(SPSPacket)//if SPS PPS packet was saved in previous pipeline
		{
			buffer = gst_buffer_copy(SPSPacket);
			gst_buffer_unref(SPSPacket);
			SPSPacket = NULL;
//			g_print ("|");
		}
		else
		{
			/* Retrieve the buffer */
			g_signal_emit_by_name (vsink, "pull-buffer", &buffer, NULL);
//			g_print ("*");
		}
		if (buffer)
		{
			if( ((strcmp(decoder, "h264") == 0) && isSPSpacket(buffer->data)) || 
				((strcmp(decoder, "mpeg4") == 0) && isMPEG4iFrame(buffer->data)) )
			
			{
				tvhIPCAMworkaround(buffer);
				//Save SPS packet for next pipeline and stop pushing data to current pipeline
				if(isTimeout())
				{
					SPSPacket = gst_buffer_copy(buffer);
					//send eos to vsrc of out pipeline
					GstEvent* event = gst_event_new_eos();
					gst_element_send_event (vsrc, event);
					gst_buffer_unref (buffer);
					push_to_vsrc = FALSE;
					return;
				}
			}
			/* Push the buffer into the vsrc */
			g_signal_emit_by_name (vsrc, "push-buffer", buffer, &ret);
			if(ret != GST_FLOW_OK)
				g_print("%s ERROR!!! Can't push buffer to vsrc (%d)\n", TAG, ret);
			gst_buffer_unref (buffer);
		}
	}
}
#ifdef AUDIO
/* The asink has received a buffer */
static void new_audio_buffer (GstElement *sink) {
	GstBuffer *buffer;
	int outsize, i;
	GstBuffer *outbuf;
	GstFlowReturn ret = GST_FLOW_OK;
	guint8 *p, *pout;
	if(push_to_asrc)
	{
		/* Retrieve the buffer */
		g_signal_emit_by_name (asink, "pull-buffer", &buffer, NULL);
//			g_print ("^");
		
		if (buffer && asrc)
		{
//			if(stereo_buffer->data == NULL)
			{
				outsize = buffer->size * 2;
//				g_print("size %d stereo_size %d\n", buffer->size, outsize);
				outbuf = gst_buffer_new ();
				GST_BUFFER_SIZE (outbuf) = outsize;
				GST_BUFFER_MALLOCDATA (outbuf) = g_malloc (outsize);
				GST_BUFFER_DATA (outbuf) = GST_BUFFER_MALLOCDATA (outbuf);
			}
			p = buffer->data;
			pout= outbuf->data;
			for(i = 0; i < 2; i++)
			{
				memcpy(pout, p, buffer->size);
				pout += buffer->size;
			}
			if(isTimeout())
			{
				//send eos to asrc of out pipeline
				GstEvent* event = gst_event_new_eos();
				gst_element_send_event (asrc, event);
				gst_buffer_unref (outbuf);
				push_to_asrc = FALSE;
				return;
			}

			/* Push the buffer into the asrc */
			g_signal_emit_by_name (asrc, "push-buffer", outbuf, &ret);
			if(ret != GST_FLOW_OK)
				g_print("%s ERROR!!! Can't push buffer to asrc (%d)\n", TAG, ret);
			gst_buffer_unref (outbuf);
//			g_free(outbuf);
		}
	}
}
#endif

void print_help(char *argv)
{
	g_print("Usage %s <rtsp url> <decoder> <base dir> <duration> [name]\n",argv);
	exit(1);
}

int main(int argc, char* argv[])
{
//	const gchar *nano_str;
//	guint major, minor, micro, nano;
	gchar *descr;
	GError *error = NULL;

	if(argc < 5)
		print_help(argv[0]);
	else
	{
		strcpy(url, argv[1]);
		strcpy(decoder, argv[2]);
		strcpy(baseDir,argv[3]);
		duration = atoi(argv[4]);
		if(argc < 6)
			searchIPinURL(url, camFolder);
		else
			strcpy(camFolder, argv[5]);
	}
		
	signal(SIGINT, sig_handler);

	filename[0] = '\0';
	workFolder[0] = '\0';
	
	/* Initialisation */
	gst_init (&argc, &argv);
/*	gst_version (&major, &minor, &micro, &nano);
	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";
	g_print ("This program is linked against GStreamer %d.%d.%d %s\n", major, minor, micro, nano_str);
*/
	if(0 != createWorkFolder(workFolder))
		return -1;

	main_loop = g_main_loop_new (NULL, FALSE);
//	Video	
	if(strcmp(decoder, "h264") == 0)
		descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s ! gstrtpjitterbuffer ! rtph264depay ! appsink name=vsink", url);
	else if(strcmp(decoder, "mpeg4") == 0)
		descr = g_strdup_printf ("rtspsrc name=rtspsrc location=%s ! gstrtpjitterbuffer ! rtpmp4vdepay ! appsink name=vsink", url);
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
	bus_watch_id = gst_bus_add_watch (bus, bus_call, (gpointer)1);
	gst_object_unref (bus);
	if(0 != run_pipeline_out())
	{
		g_print("%s could not construct out pipeline\n", TAG);
		goto finish;
	}
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
	unlink(recfile);
	return 0;
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

gboolean isMPEG4iFrame(guint8 *packet)
{
	if( packet[0]==0x00 && packet[1]==0x00 && packet[2]==0x01 && packet[3]==0xB6 && !(packet[4]&0xC0) )
//	if( packet[0]==0x00 && packet[1]==0x00 && packet[2]==0x01 && (packet[3]==0xB0 || (packet[3]==0xB6 && !(packet[4]&0xC0))) )
		return TRUE;

	return FALSE;
}

gboolean isTimeout()
{
	int ret;
	int elapsedTime;

	ret = clock_gettime(CLOCK_REALTIME, &tcurr);
	if(ret == 0)
	{
		// compute elapsed time in millisec
		elapsedTime = (tcurr.tv_sec - tstart.tv_sec) * 1000.0;      // sec to ms
		elapsedTime += (tcurr.tv_nsec - tstart.tv_nsec) / 1000000.0;   // ns to ms
	}  

	if (elapsedTime >= duration*1000)
		return TRUE;
	return FALSE;
}

/*
Get SIGINT for correct stop all pipelines
Send EOS to vsrc pipeline 
*/
void sig_handler(int signum)
{
	GstEvent* event;
//    g_print("%s Received signal %d\n", TAG, signum);
    //Stop push data to vsrc
	push_to_vsrc = FALSE;
	//Don't create new pipeline
    signal_received = TRUE;
	//send eos to vsrc of out pipeline
	event = gst_event_new_eos();
	gst_element_send_event (vsrc, event);
#ifdef AUDIO
	push_to_asrc = FALSE;
	gst_element_send_event (asrc, event);
	gst_element_send_event (alsasrc, event);
	gst_element_send_event (mp3enc, event);
#endif
}

void searchIPinURL(char *url, char *camFolder)
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
	if(index == 0)//search //
	{
		g_print("%s url %s without autorization\n", TAG, url);
		if(strncmp(url,"rtsp://",6) == 0)
		{
			index = 7;
		}
	}
	p = &url[index];
	while((*p == '.') || (*p <= '9' && *p >= '0') || (*p == ':'))
	{
		*o++ = *p++;
	}
//	g_print("%s record: %s IPCAM folder %s\n", TAG, __func__, camFolder);
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

int createNewRecordFile(char * fn)
{
	int ret = 0;
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	if(strlen(workFolder))
		sprintf(fn,"%s/%02d_%02d_%02d.mp4", workFolder, t->tm_hour, t->tm_min, t->tm_sec);
	else
		ret = -ENOENT;
	return ret;
}

int cpRecToFile(void)
{
	int ret = 0;
	char cmd[1024];
	if(strlen(filename))
	{
//		g_print("cp rec to filename created in previously call of run_pipeline_out\n");
//		g_print("recfile %s filename %s\n", recfile, filename);
		//cp recfile to filename
//		ret = link(recfile, filename);
//		if(ret != 0)
//			printf("link return error %s(%d)\n",strerror(errno),errno);
		sprintf(cmd, "cp %s %s", recfile, filename);
//		printf("%s: %s\n", __func__, cmd);
		ret = system(cmd);
		if(ret != 0)
			printf("%s system return error %s(%d)\n", TAG, strerror(errno), errno);
//		ret = rename(recfile, filename);
	}
	return ret;
}

void tvhIPCAMworkaround(GstBuffer *buffer)
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
			g_print("%s Tvhelp IPCAM PPS double '0 0 0 1' at (%d) position in first SPS packet\n", TAG, i);
			memcpy(&buffer->data[i],&buffer->data[i+4],buffer->size - i - 4);
		}
	}
	/*g_print("after\n");
	for(i = 0; i < buffer->size; i++)
	g_print("%x ",buffer->data[i]);
	g_print("\n");*/
}

void print_buffer(GstBuffer *buf)
{
	int i;
	if(buf)
	{
		g_print("%s Buffer size %d: ", TAG, buf->size);
		for(i = 0; i < buf->size; i++)
			g_print("%x ",buf->data[i]);
		g_print("\n");
	}
	else
		g_print("%s !!! Error !!! Buffer empty\n", TAG);
}

