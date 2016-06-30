#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <scd.h>
#include <btr.h>
#include <pthread.h>
#include <errno.h>

#define BPP		3 //byteperpixel
#define FPS 25

#define FREE(p) {if(p != NULL) free(p);}

#define CORES	4

char *B2RGB_STR[B2RGB_MODE_NUM] = {"SIMPLE","FULL"};

enum B2RGB_MODE bMode;
pthread_t pth_b2rgb[CORES];
pthread_t pth_get_frame;
pthread_t pth_cb;

int run = 0;
int bytes_read;
mt9_setup_t setup;
char * buf;
char *argb;
int fd_scd;
int fb;
int converted[CORES];
int id[4] = {0,1,2,3};
int conversionMode = B2RGB_SIMPLE;
int WIDTH;
int HEIGHT;
btr_get_frame_cb callback;

void finish(void)
{
	run = 0;
	close(fd_scd);
	FREE(buf);
	FREE(argb);
}

static int btr_setup_cam(void)
{
	int ret;
	int gain = 40;
	ret = ioctl(fd_scd,IOCTL_SCD_CAMERA_SETUP,&setup);
	if(ret !=0)
	{
		printf("SCD return %d on CAMERA_SETUP\n",ret);
	}

	ioctl(fd_scd,IOCTL_SCD_CAMERA_GAIN,gain);
	return ret;
}
/*
				printf("Enter fps in range 1-120:\n> ");
				scanf("%d",&setup.fps);
				setup_cam();//TODO Handle error
				printf("Enter gain in range 8-63:\n> ");
				scanf("%d",&gain);
				ioctl(fd_scd,IOCTL_SCD_CAMERA_GAIN,gain);
				printf("Toggle mode to ");
				if(conversionMode == B2RGB_SIMPLE)
					conversionMode = B2RGB_FULL;
				else
					conversionMode = B2RGB_SIMPLE;
				printf("%s\n",B2RGB_STR[conversionMode]);
				if (conversionMode == B2RGB_SIMPLE)
				{
					setup.w = WIDTH*2;
					setup.h = HEIGHT*2;
					setup.fps = 15;
				}
				else if (conversionMode == B2RGB_FULL)
				{
					setup.w = WIDTH;
					setup.h = HEIGHT;
					setup.fps = 30;
				}
				FREE(buf);
				FREE(argb);
				buf = (char*)malloc(setup.w*setup.h);
				if(buf == NULL)
				{
					printf("Memory allocation error\n");
					finish();
				}
				argb = (char*)malloc(setup.w*setup.h*BPP);
				if(argb == NULL)
				{
					printf("Memory allocation error\n");
					finish();
				}
				setup_cam();//TODO Handle error
*/

static void* b2rgb_func(void *arg)
{
	int area = *((int*)arg);
	int x,y;
	unsigned char r, g, b;
	char *p;
//	printf("area %d\n",area);
//	while(run)
	{
//		printf("fb %d \n",fb);
		if(bytes_read == setup.w*setup.h && (converted[area] == 0))
		{
			p = argb + area*(WIDTH*HEIGHT)*BPP/CORES;
if(conversionMode == B2RGB_SIMPLE)
{
			//Bayer to RGB with downscale
			for(y = area*setup.h/2/CORES; y < (area+1)*setup.h/2/CORES; y++)
			{
				for(x = 0; x < setup.w/2; x++)
				{
					r = buf[(y*2+0)*setup.w+(x*2+1)];
					b = buf[(y*2+1)*setup.w+(x*2+0)];
					g = (buf[(y*2+0)*setup.w+(x*2+0)] + buf[(y*2+1)*setup.w+(x*2+1)])/2;
					*p++ = r;
					*p++ = g;
					*p++ = b;
					if(BPP == 4)
						*p++ = 0;
				}
			}
}
else if (conversionMode == B2RGB_FULL)
{
			//Bayer to RGB full processing
			for(y = area*setup.h/CORES; y < (area+1)*setup.h/CORES; y++)
			{
				for(x = 0; x < setup.w; x++)
				{
					if(x == 0 || y == 0 || x == setup.w || y == setup.h)
					{
						r = g = b = buf[y*setup.w + x];
					}
					else
					{
						if(y%2)//blue y 0,2,4,6...
						{
							if(x%2)//B
							{
								r = (buf[(y-1)*setup.w + (x-1)] + buf[(y-1)*setup.w + (x+1)] + buf[(y+1)*setup.w + (x-1)] + buf[(y+1)*setup.w + (x+1)])/4;
								g = (buf[(y-1)*setup.w + x] + buf[(y+1)*setup.w + x] + buf[y*setup.w + (x-1)] + buf[y*setup.w + (x+1)])/4;
								b = buf[y*setup.w+x];
							}
							else//G
							{
								r = (buf[(y-1)*setup.w + x] + buf[(y+1)*setup.w + x])/2;
								b = (buf[y*setup.w+(x+1)] + buf[y*setup.w+(x-1)])/2;
								g = buf[y*setup.w+x];
							}
						}
						else//red y 1,3,5,7...
						{
							if(x%2)//R
							{
								b = (buf[(y-1)*setup.w + (x-1)] + buf[(y-1)*setup.w + (x+1)] + buf[(y+1)*setup.w + (x-1)] + buf[(y+1)*setup.w + (x+1)])/4;
								g = (buf[(y-1)*setup.w + x] + buf[(y+1)*setup.w + x] + buf[y*setup.w + (x-1)] + buf[y*setup.w + (x+1)])/4;
								r = buf[y*setup.w+x];
							}
							else//G
							{
								r = (buf[y*setup.w+(x+1)] + buf[y*setup.w+(x-1)])/2;
								b = (buf[(y-1)*setup.w + x] + buf[(y+1)*setup.w + x])/2;
								g = buf[y*setup.w+x];
							}
						}
					}
					*p++ = r;
					*p++ = g;
					*p++ = b;
					if(BPP == 4)
						*p++ = 0;
				}
			}
}
			converted[area] = 1;
//			printf("converted%d %d\n",area,converted[area]);
		}
	}
//	pthread_join(pth_b2rgb[area], NULL);
	return NULL;
}

static void* get_frame_func(void *arg)
{
	int ret, i;

	while(run)
	{
		bytes_read = pread(fd_scd,buf,setup.w*setup.h,0);
		if(bytes_read == setup.w*setup.h)
		{
//			printf("%s bytes %d\n",__func__, bytes_read);
			for(i = 0; i < CORES; i++)
			{
				ret = pthread_create(&pth_b2rgb[i], NULL, &b2rgb_func, &id[i]);
				if (ret != 0)
				{
					printf("Could't create thread b2rgb_func%d\n",i);
					finish();
					break;
				}
			}

			for(i = 0; i < CORES; i++)
				pthread_join(pth_b2rgb[i], NULL);
		}
		usleep(10000/setup.fps);
	}
	return NULL;
}

void* cb_func(void *arg)
{
	int i;
	int dataReady;
	
	while(run)
	{
		dataReady = 0;
		for(i = 0; i < CORES; i++)
		{
//			printf("%d %d\n",i,converted[i]);
			if(converted[i] == 1) dataReady++;
		}
//		printf("dataReady %d\n",dataReady);
		if(dataReady == CORES)
		{
			callback(argb, WIDTH*HEIGHT*BPP);
		}
		for(i = 0; i < CORES; i++)
		{
			converted[i] = 0;
		}

		usleep(1000000/setup.fps);
	}
	return NULL;
}

int btr_init(int w, int h, int fps, int mode, btr_get_frame_cb cb)
{
	int ret;
	
	setup.cs[0] = -1;
	setup.rs[0] = -1;
	setup.cs[1] = -1;
	setup.rs[1] = -1;
	if(fps < 0 || fps > 60)
		fps = FPS;
	else
		setup.fps = fps;
		
	WIDTH = w;
	HEIGHT = h;
	if(mode < 0 || mode >= B2RGB_MODE_NUM)
		conversionMode = B2RGB_SIMPLE;
	else
		conversionMode = mode;
		
	callback = cb;
		
	if (conversionMode == B2RGB_SIMPLE)
	{
		if(w > CAMERAW/2 || h > CAMERAH/2)
			return -1;
		setup.w = WIDTH*2;
		setup.h = HEIGHT*2;
	}
	else if (conversionMode == B2RGB_FULL)
	{
		setup.w = WIDTH;
		setup.h = HEIGHT;
	}

	buf = (char*)malloc(setup.w*setup.h);
	if(buf == NULL)
	{
		printf("Memory allocation error\n");
		finish();
		return -ENOMEM;
	}
	argb = (char*)malloc(setup.w*setup.h*BPP);
	if(argb == NULL)
	{
		printf("Memory allocation error\n");
		finish();
		return -ENOMEM;
	}
	
    if ((fd_scd = open("/dev/scd",O_RDWR)) < 0) {
		printf("Failed to open scd\n");
		finish();
		return fd_scd;
    }

	btr_setup_cam();

	run = 1;
	
	ret = pthread_create(&pth_get_frame, NULL, &get_frame_func, NULL);
	if (ret != 0)
	{
		printf("\ncan't create get_frame thread :[%s]", strerror(ret));
		finish();
		return ret;
	}
	else
	    printf("\n get_frame thread created successfully\n");

	ret = pthread_create(&pth_cb, NULL, &cb_func, NULL);
	if (ret != 0)
	{
		printf("\ncan't create fb thread :[%s]", strerror(ret));
		finish();
		return ret;
	}
	else
	    printf("\n Fb thread created successfully\n");

	return 0;
}

void btr_exit(void)
{
	finish();
}

