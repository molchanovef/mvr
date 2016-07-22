#ifndef __SCD_H__
#define __SCD_H__

#define CAMERAW	2592
#define CAMERAH	1944

typedef struct mt9_setup_s
{
	int cs[2];//column_start
	int rs[2];//row_start
	unsigned int w;//width
	unsigned int h;//height
	unsigned fps;
	unsigned int skip;
	unsigned int bin;
}mt9_setup_t;


#define IOCTL_SCD_CAMERA_POLL	_IOR(0, 0, unsigned int *)//setup sensor
#define IOCTL_SCD_CAMERA_SETUP	_IOW(0, 1, mt9_setup_t *)//setup sensor
#define IOCTL_SCD_CAMERA_GAIN	_IOW(0, 2, int *)//set camera gain
#define IOCTL_SCD_CAMERA_MODE	_IOW(0, 3, int *)//set camera gain
#define IOCTL_SCD_CAMERA_TRIGGER	_IOW(0, 4, int *)//make snapshoot

#endif
