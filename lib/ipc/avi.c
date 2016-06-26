#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "avi.h"

#define MOD "AVI"
#include "mylog.h"
#define DBG	LOGT

//#define PROFILING

#define PREFIX	"/tvh/av/"
#define NUM_STREAMS	32
#define MAX_FRAMES	8
#define MEM_LIMIT	(2*1024*1024)

typedef struct frame_s
{
	struct timeval timestamp;
	int size;
	char name[32];
} frame_t;
typedef struct stream_s
{
	frame_t frame[MAX_FRAMES];
	int put;
	int get;
	int format;
	int width;
	int height;
	pthread_mutex_t lock;
} stream_t;

static pthread_mutex_t lock;
static stream_t stream[NUM_STREAMS];
static int inited = 0;

static void remove_frame(int s, int f);
static int remove_oldest_frame();
static int get_free_mem();

int avi_init()
{
	int ret = 0;
	if (inited) return 0;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	if (0 != pthread_mutex_init(&lock, &attr))
	{
		pthread_mutexattr_destroy(&attr);
		ret = -ENOMEM;
	}
	if (ret == 0)
	{
		pthread_mutex_lock(&lock);
		int i;
		for (i = 0; i < NUM_STREAMS; i++)
		{
			memset(stream[i].frame, 0, sizeof(frame_t)*MAX_FRAMES);
			stream[i].get = stream[i].put = 0;
			if (0 != pthread_mutex_init(&stream[i].lock, &attr))
			{
				ret = -ENOMEM;
				break;
			}
		}
		pthread_mutex_unlock(&lock);
	}
	pthread_mutexattr_destroy(&attr);
	system("rm -r "PREFIX);
	if (ret == 0)
	{
		mkdir(PREFIX, 0666);
		int i;
		for (i = 0; i < NUM_STREAMS; i++)
		{
			char dn[256];
			sprintf(dn, PREFIX"%d", i);
			mkdir(dn, 0666);
		}
		inited = 1;
	}
	return ret;
}

int avi_exit()
{
	if (!inited) return 0;
	pthread_mutex_lock(&lock);
	int i;
	for (i = 0; i < NUM_STREAMS; i++)
	{
		pthread_mutex_lock(&stream[i].lock);
		int j; for (j = stream[i].get; j < stream[i].put; j++)
		{
			unlink(stream[i].frame[j&(MAX_FRAMES-1)].name);
		}
		stream[i].get = stream[i].put = 0;
		pthread_mutex_unlock(&stream[i].lock);
		pthread_mutex_destroy(&stream[i].lock);
	}
	pthread_mutex_unlock(&lock);
	pthread_mutex_destroy(&lock);
	system("rm -r "PREFIX);
	inited = 0;
	return 0;
}

int avi_set_stream(int idx, int format, int width, int height)
{
	if (!inited) return -ENOENT;
	if (idx < 0 || idx >= NUM_STREAMS) return -EINVAL;
	pthread_mutex_lock(&lock);
	stream[idx].format = format;
	stream[idx].width = width;
	stream[idx].height = height;
	pthread_mutex_unlock(&lock);
	return 0;
}

int avi_get_stream(int idx, avi_stream_t *stream)
{
	int ret = 0;
	if (idx < 0 || idx >= NUM_STREAMS) return -EINVAL;
	if (!stream) return -EINVAL;
	
	char fn[32]; sprintf(fn, PREFIX"%d/inf", idx);
	FILE *file = fopen(fn, "rb");
	if (file)
	{
		if (1 == fread(&stream->format, sizeof(unsigned), 1, file))
		{
			if (stream->format&AVI_VIDEO)
			{
				if (1 == fread(&stream->width, sizeof(int), 1, file) && 1 == fread(&stream->height, sizeof(int), 1, file))
				{
					if (stream->format == AVI_MPEG4)
					{
						if (18 != fread(stream->vol, 1, 18, file))
						{
							LOGE("Failed to read file %s", fn);
							ret = -EBADF;
						}
					}
					else if (stream->format == AVI_H264)
					{
						// TODO
					}
					else if (stream->format == AVI_H265)
					{
						// TODO
					}
				}
				else
				{
					LOGE("Failed to read file %s", fn);
					ret = -EBADF;
				}
			}
			else if (stream->format&AVI_AUDIO)
			{
				// TODO
			}
		}
		fclose(file);
	}
	else
	{
		LOGE("Failed to open file %s", fn);
		ret = -EBADF;
	}
	if (ret) return ret;

	char dir[16]; sprintf(dir, PREFIX"%d", idx);
	DIR *d = opendir(dir);
	if (d)
	{
		stream->early_frame = 0x7fffffff; stream->late_frame = 0;
		struct dirent de, *res;
		while (0 == readdir_r(d, &de, &res))
		{
			if (de.d_name[0] == 'I' || de.d_name[0] == 'J' || de.d_name[0] == 'A')
			{
				int n = atoi(de.d_name+1);
				if (n < stream->early_frame) stream->early_frame = n;
				if (n > stream->late_frame) stream->late_frame = n;
			}
		}
		closedir(d);
	}
	else
	{
		LOGE("Failed to open directory %s", dir);
		return -ENOTDIR;
 	}
	
	return ret;
}

int avi_put_frame(int idx, void *data, int size, int frame_type, double timestamp)
{
//if (idx != 0) return 0;
	int ret = 0;
//	DBG("avi_put_frame(%d, %x, %d, %x, %f)", idx, data, size, frame_type, timestamp);
	
	if (!inited) return -ENOENT;
	if (idx < 0 || idx >= NUM_STREAMS) return -EINVAL;

	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return 0;

#ifdef PROFILING
struct timeval t1, t2, t3;
gettimeofday(&t1, NULL);
#endif
	// remove old files
	pthread_mutex_lock(&lock);
	if (stream[idx].put >= stream[idx].get+MAX_FRAMES)
	{
		remove_frame(idx, stream[idx].get);
	}
	int freemem = get_free_mem();
	while (size > freemem)
	{
		int add = remove_oldest_frame();
		if (add == 0) break;
		freemem += add;
	}
	if (size > freemem)
	{
		LOGE("No space for encoded frames");
		ret = -ENOMEM;
	}
	pthread_mutex_unlock(&lock);
	if (ret) return ret;
#ifdef PROFILING
gettimeofday(&t2, NULL);
#endif
	// 
	pthread_mutex_lock(&stream[idx].lock);
	if (stream[idx].put == 0)
	{
		char fn[32]; sprintf(fn, PREFIX"%d/inf", idx);
		FILE *file = fopen(fn, "w+b");
		if (file)
		{
			fwrite(&stream[idx].format, sizeof(int), 1, file);
			if (stream[idx].format&AVI_VIDEO)
			{
				fwrite(&stream[idx].width, sizeof(int), 1, file);
				fwrite(&stream[idx].height, sizeof(int), 1, file);
				if (stream[idx].format == AVI_MPEG4 && frame_type == 1/*IFRAME*/)
				{
					fwrite(data, 1, 18, file);
				}
				else if (stream[idx].format == AVI_H264)
				{
					// TODO
				}
				else if (stream[idx].format == AVI_H265)
				{
					// TODO
				}
				fclose(file);
			}
			else if (idx == AVI_PCM)
			{
				// TODO
			}
		}
		else
		{
			LOGE("Failed to open file %s", fn);
			ret = -EBADF;
		}
	}
	frame_t *frame = &stream[idx].frame[stream[idx].put&(MAX_FRAMES-1)];
	frame->timestamp.tv_sec = ts.tv_sec;
	frame->timestamp.tv_usec = ts.tv_nsec/1000;
//LOGI("%d.%06d", frame->timestamp.tv_sec, frame->timestamp.tv_usec);
//	frame->timestamp.tv_sec = (int)(timestamp/1000);
//	frame->timestamp.tv_usec = ((int)(timestamp-frame->timestamp.tv_sec*1000.0)%1000)*1000;
	frame->size = size;
	if (stream[idx].format == AVI_MPEG4)
		sprintf(frame->name, PREFIX"%d/%c%d", idx, frame_type==1?'I':frame_type==2?'P':'U', stream[idx].put);
	else if (stream[idx].format == AVI_MJPEG)
		sprintf(frame->name, PREFIX"%d/J%d", idx, stream[idx].put);
	else if (stream[idx].format == AVI_PCM)
		sprintf(frame->name, PREFIX"%d/A%d", idx, stream[idx].put);
	char tmp[16]; sprintf(tmp, PREFIX"%d/w", idx);
	FILE *file = fopen(tmp, "w+b");
	if (file)
	{
		avi_frame_t avi_frame = {stream[idx].put, frame->timestamp, frame_type==1?AVI_MP4_IFRAME:0, stream[idx].width, stream[idx].height, stream[AVI_PCM].put, frame->size};
		if (1 != fwrite(&avi_frame, sizeof(avi_frame_t)-sizeof(unsigned char *), 1, file) ||
			size != fwrite(data, 1, size, file))
		{
			LOGE("Failed to write file");
			ret = -ENOSPC;
		}
		fclose(file);
	}
	else
	{
		LOGE("Failed to create file");
		ret = -EBADF;
	}
	if (ret == 0)
	{
		++stream[idx].put;
		rename(tmp, frame->name);
//		DBG("%s %d.%d", frame->name, frame->timestamp.tv_sec, frame->timestamp.tv_usec);
	}
	pthread_mutex_unlock(&stream[idx].lock);
#ifdef PROFILING
gettimeofday(&t3, NULL);
DBG("avi_put_frame %d: %d %d", t3.tv_usec-t1.tv_usec, t2.tv_usec-t1.tv_usec, t3.tv_usec-t2.tv_usec);
#endif
	return ret;
}

int avi_get_frame(int idx, int number, avi_frame_t *frame)
{
	int ret = 0;
	if (idx < 0 || idx >= NUM_STREAMS) return -EINVAL;
//DBG("avi_get_frame(%d,%d,%x)", idx, number, frame);
#ifdef PROFILING
struct timeval t1, t2, t3;
gettimeofday(&t1, NULL);
#endif
	char dir[16]; sprintf(dir, PREFIX"%d", idx);
	DIR *d = opendir(dir);
	if (!d) return -ENOTDIR;
	
	char fn[32] = {0};
	struct dirent de, *res;
	while (0 == readdir_r(d, &de, &res))
	{
		if (de.d_name[0] == 'I' || de.d_name[0] == 'P' || de.d_name[0] == 'J' || de.d_name[0] == 'A')
		{
			int n = atoi(de.d_name+1);
			if (n < number)
			{
				break;
			}
			if (n == number)
			{
				sprintf(fn, PREFIX"%d/%s", idx, de.d_name);
				break;
			}
			if (n >= number && de.d_name[0] != 'P')
			{
				sprintf(fn, PREFIX"%d/%s", idx, de.d_name);
			}
		}
	}
	closedir(d);
#ifdef PROFILING
gettimeofday(&t2, NULL);
#endif
	if (!fn[0])
	{
//DBG("%d", t2.tv_usec-t1.tv_usec);
		usleep(10000);
		return -ENODATA;
	}
	FILE *file = fopen(fn, "rb");
	if (file)
	{
		if (frame)
		{
			if (1 == fread(frame, sizeof(avi_frame_t)-sizeof(unsigned char *), 1, file))
			{
				frame->data = (unsigned char *) malloc(frame->size);
				if (frame->data)
				{
					if (frame->size == fread(frame->data, 1, frame->size, file))
					{
						ret = 0;
					}
					else
					{
						LOGE("Failed to read file %s", fn);
						free(frame->data);
						ret = -EBADF;
					}
				}
				else
				{
					LOGE("Failed to allocate %d bytes", frame->size);
					ret = -ENOMEM;
				}
			}
			else
			{
				LOGE("Failed to read file %s", fn);
				ret = -EBADF;
			}
		}
		else
		{
			if (1 != fread(&ret, sizeof(int), 1, file))
			{
				LOGE("Failed to read file %s", fn);
				ret = -EBADF;
			}
		}
		fclose(file);
	}
	else
	{
		LOGE("Failed to open file %s", fn);
		ret = -EBADF;
	}
#ifdef PROFILING
gettimeofday(&t3, NULL);
DBG("avi_get_frame %d: %d %d", t3.tv_usec-t1.tv_usec, t2.tv_usec-t1.tv_usec, t3.tv_usec-t2.tv_usec);
#endif
	if (ret < 0)
	{
		char str[32]; sprintf(str, "ls %s", dir);
		system(str);
	}
	return ret;
}


static int get_free_mem()
{
	int i, j;
	int s = 0;
	for (i = 0; i < NUM_STREAMS; i++)
	{
		pthread_mutex_lock(&stream[i].lock);
		for (j = stream[i].get; j < stream[i].put; j++)
		{
			s += stream[i].frame[j&(MAX_FRAMES-1)].size;
		}
		pthread_mutex_unlock(&stream[i].lock);
	}
//	DBG("get_free_mem return %d", MEM_LIMIT-s);
	return MEM_LIMIT-s;
}
static int remove_oldest_frame()
{
	int i;
	struct timeval time = {0x7fffffff, 0x7fffffff};
	int s = -1;
	for (i = 0; i < NUM_STREAMS; i++)
	{
		if (stream[i].get < stream[i].put)
		{
			if (stream[i].frame[stream[i].get&(MAX_FRAMES-1)].timestamp.tv_sec < time.tv_sec || 
				(stream[i].frame[stream[i].get&(MAX_FRAMES-1)].timestamp.tv_sec == time.tv_sec && 
				 stream[i].frame[stream[i].get&(MAX_FRAMES-1)].timestamp.tv_usec < time.tv_usec))
			{
				time = stream[i].frame[stream[i].get&(MAX_FRAMES-1)].timestamp;
				s = i;
			}
		}
	}
	int ret = 0;
	if (s >= 0)
	{
		ret = stream[s].frame[stream[s].get&(MAX_FRAMES-1)].size;
		remove_frame(s, stream[s].get);
	}
//	DBG("remove_oldest_frame return %d", ret);
	return ret;
}
static void remove_frame(int s, int f)
{
	pthread_mutex_lock(&stream[s].lock);
	if (stream[s].get <= f && f < stream[s].put)
	{
		int j;
		for (j = stream[s].get; j <= f; j++)
		{
//			DBG("remove_frame removes %s", stream[s].frame[f&(MAX_FRAMES-1)].name);
			unlink(stream[s].frame[f&(MAX_FRAMES-1)].name);
		}
		stream[s].get = f+1;
	}
	pthread_mutex_unlock(&stream[s].lock);
}

