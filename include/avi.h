#pragma once

#include <errno.h>
#include <sys/time.h>

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct avi_stream_s
{
#define AVI_VIDEO	0x10
#define AVI_AUDIO	0x20
#define AVI_MJPEG	(AVI_VIDEO|1)
#define AVI_MPEG4	(AVI_VIDEO|2)
#define AVI_H264	(AVI_VIDEO|3)
#define AVI_H265	(AVI_VIDEO|4)
#define AVI_PCM		(AVI_AUDIO|1)
#define AVI_NOTPCM	(AVI_AUDIO|2)
	int format;
	union
	{
		struct
		{
			int width;
			int height;
			char vol[18];
		};
		struct
		{
			int samplerate;
		};
	};
	int early_frame;
	int late_frame;
} avi_stream_t;
#define AVI_MP4_IFRAME	1
typedef struct avi_frame_s
{
	int number;
	struct timeval timestamp;
	int type;
	int width;
	int height;
	int anumber;
	int size;
	unsigned char *data;
} avi_frame_t;

int avi_init();
int avi_exit();
int avi_set_stream(int idx, int format, int width, int height);
int avi_get_stream(int idx, avi_stream_t *stream);
int avi_put_frame(int, void *data, int size, int frame_type, double timestamp);
int avi_get_frame(int, int number, avi_frame_t *frame);

#if defined (__cplusplus)
}
#endif

