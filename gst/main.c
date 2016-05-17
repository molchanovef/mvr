#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "tv_help.c"
/*
This program create gstreamer pipeline and send his out to named pipe.
Then open this named pipeline for read and get data from it.
Data processing YUV->RGB for display show.
*/
#define DW		800
#define DH		480
#define NUM		4
#define W		320
#define H		240
#define BORDER	5//minimum 2 cause 1 pixel used for black border
#define URL "rtsp://admin:9999@192.168.11.94:8555/Stream2"
#define TEMP "/sys/class/thermal/thermal_zone0/temp"
#define TEMPNORM 45000
#define TEMPCRIT 75000
/*
y = buf[j*W + i];
u = buf[(j/2)*(W/2)+(i/2)+(W*H)];
v = buf[(j/2)*(W/2)+(i/2)+(W*H)+((W*H)/4)];
r = y + (1140 * v / 1000);
g = y - (395 *u / 1000) - (581 * v)/1000;					
b = y + (2032 * u / 1000);

r = clamp(y + (1.370705 * (v-128)));
g = clamp( y - (0.698001 * (v-128)) - (0.337633 * (u-128)));
b = clamp(y + (1.732446 * (u-128)));

C = y - 16;
D = u - 128;
E = v - 128;
r = clamp(( 298 * C           + 409 * E + 128) >> 8);
g = clamp(( 298 * C - 100 * D - 208 * E + 128) >> 8);
b = clamp(( 298 * C + 516 * D           + 128) >> 8);
*/

void print_help(char *argv)
{
	printf("Usage %s <rtsp url> <position> \n",argv);
}

int clamp(int val){
if(val>255) return 255;
if(val<0) return 0;
return val;
}

int main(int argc, char* argv[])
{
	int ret, i, j, pos;
	int fb;
	unsigned char buf[W*H*3/2];
	unsigned int argb[W*H*4];
	char url[1024] = {0};
	char gst[1024] = {0};
	char stream[16];
	char stemp[16];
	int temp;
	int counter;
	FILE*	pfd;
	
	FILE* tfd;
	if(argc < 3)
	{
		print_help(argv[0]);
		pos = 0;
		strcpy(url, URL);
		printf("Default url %s and position %d\n",url,pos);
	}	
	else
	{
		pos = atoi(argv[2]);
		strcpy(url,argv[1]);
		printf("url %s position %d\n",url,pos);
	}
	memset(argb,0,sizeof(argb));
	sprintf(stream, "stream%d",pos);		

	ret = fork();
	if(ret == 0)
	{
		//																						1: I420
		sprintf(gst,"gst-launch rtspsrc location=%s ! gstrtpjitterbuffer ! rtph264depay ! vpudec output-format=1 ! filesink location=stream%d &",url,pos);
		printf("%s\n",gst);
		printf("sys %d\n",system(gst));
		return 0;
	}
	if(ret < 0)
		return -1;
	pfd = fopen(stream,"rb");
	counter = 0;
	fb = open("/dev/fb0",O_RDWR);
	if(fb <= 0)
	{
		printf("Failed to open fb0\n");
		return -1;
	}
	if (pos == 0)
	{
		//fill free space with black color
		lseek(fb,0,SEEK_SET);
		unsigned int black[DW-W*2];
//		memset(black,0xFFFFFFFF,sizeof(black));
		memset(black,0,sizeof(black));
		for(j = 0; j < DH; j++)
		{
			write(fb,&black,(DW-W*2)*4);
			lseek(fb,W*2*4,SEEK_CUR);
		}
		//Draw logo
		lseek(fb,DW*(DH-logo.height)*2,SEEK_SET);
		const char *l = logo.pixel_data;
		for(j = 0; j < logo.height; j++)
		{
			for(i = 0; i < logo.width; i++)
			{
				char a,r,g,b;
				unsigned int bgra;
				r = *l++;
				g = *l++;
				b = *l++;
				a = *l++;
				bgra = (0xFF << 24) | (r<<16) | (g<<8) | b;

				write(fb,&bgra,4);
//				write(fb,&logo.pixel_data[j*logo.width*4],logo.width*4);
//				lseek(fb,(DW-logo.width)*4,SEEK_CUR);
			}
			lseek(fb,(DW-logo.width)*4,SEEK_CUR);
		}
	}
//	close(fb);
	if(pfd > 0)
	{
		while(1)
		{
//			fb = open("/dev/fb0",O_RDWR);
			ret = fread(buf,1,W*H*3/2,pfd);
			if(ret != W*H*3/2)
			{
				printf("!!!ERROR wrong data size\n");
				break;
			}
			//Convert YUV to ARGB
			char r,g,b;
			int C, D, E;
			unsigned int *pout = argb;
			unsigned char *y = buf, *u = buf+W*H, *v = buf + W*H*5/4;

			for(j = 0; j < H; j++)
			{
				u = buf+W*H + (j/2)*(W/2);
				v = buf+W*H*5/4 + (j/2)*(W/2);
				for(i = 0; i < W; i++)
				{
					C = *y++ - 16;
					D = *(u+i/2) - 128;
					E = *(v+i/2) - 128;
					//black border
					if(j == 0 || i == 0 || j == H-1 || i == W-1)
					{
						r = 0; g = 0; b = 0;
					}
					//color border
					else if(j < BORDER || i < BORDER || j >= H-BORDER || i >= W-BORDER)
					{
/*						switch(pos)
						{
							case 0: r = 0xC0; g = 0; b = 0; break;
							case 1: r = 0; g = 0xC0; b = 0; break;
							case 2: r = 0; g = 0; b = 0xC0; break;
							case 3: r = 0xC0; g = 0xC0; b = 0; break;
						}
*/
							r = 0; g = 0x80; b = 0;
					}
					//frame
					else
					{
						r = clamp(( 298 * C           + 409 * E + 128) >> 8);
						g = clamp(( 298 * C - 100 * D - 208 * E + 128) >> 8);
						b = clamp(( 298 * C + 516 * D           + 128) >> 8);
					}
					*pout = (0xFF << 24) | (r<<16) | (g<<8) | b;
					pout++;
				}
			}

			switch(pos)
			{
				case 0: lseek(fb,(DW-W*2)*4,SEEK_SET); break;
				case 1: lseek(fb,(DW-W*2+W)*4,SEEK_SET); break;
				case 2: lseek(fb,(DW-W*2+DW*DH/2)*4,SEEK_SET); break;
				case 3: lseek(fb,(DW-W*2+DW*DH/2+W)*4,SEEK_SET); break;
			}
			
			for (j = 0; j < H; j++)//H-8 for 640*368
			{
				write(fb, &argb[j*W], W*4);
				lseek(fb,(DW-W)*4,SEEK_CUR);//320*240
			}
#ifdef TEMPMON
			if(pos == 0)
			{
				if(counter == 0)
				{
					counter = 100;
					tfd = fopen(TEMP,"rt");
					if(tfd)
					{
						if(NULL != fgets(stemp, 15, tfd))
						{
							if(sscanf(stemp, "%d", &temp) == 1)
							{
//								printf("temp = %d\n",temp);
							}
						}
					}

					lseek(fb,(DW*(DH - BORDER))*4,SEEK_SET);
					unsigned int fancolor[DW-W*2];
					for(i = 0; i < DW-W*2; i++)
					{
						if(temp >= TEMPCRIT)
							fancolor[i] = 0x00C00000;
						else if(temp <= TEMPNORM)
							fancolor[i] = 0x000000C0;
						else
							fancolor[i] = 0x00008000;

					}					
					for(j = 0; j < BORDER; j++)
					{
						write(fb,&fancolor,(DW-W*2)*4);
						lseek(fb,W*2*4,SEEK_CUR);
					}
					fclose(tfd);
				}
				counter--;
			}
#endif
//			close(fb);
		}//while(0)
	}
	close(fb);
	return 0;
}
