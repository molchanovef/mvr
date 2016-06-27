#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "getch.h"
#include "avi.h"

#define CAM_NUM		32
#define WIFI_NUM	32
#define TAG			"MVR:"
#define MIN(a,b)	((a)<(b)?(a):(b))

pthread_t pth_control;
int run;
int recEna, mosEna, uplEna;
int camcnt;
char *filename;
pid_t uploadPid;

const char *LAYER[] = {"SINGLE", "2x2", "3x3"};
typedef enum layer_s {SINGLE=1, QUAD, NINE} layer_t;
layer_t mosaic = QUAD;

typedef struct _Camera
{
	char *name;
	char *url;
	char *decoder;
	char *recdir;
	char *rectime;
	char *latency;
	int position;
	pid_t recPid;
	pid_t mosPid;
} Camera;
Camera	*camera[CAM_NUM] = {NULL};

typedef struct _Wifi
{
	char *ssid;
	char *password;
} Wifi;

Wifi *wifi[WIFI_NUM] = {NULL};

void* control_func (void *arg);
void sig_handler(int signum);
int startRec(Camera *h);
int startMos(Camera *h);
int startUpload(void);
void toggle_mosaic(void);
void shift_mosaic(unsigned int value);

void stopUpload(void)
{
	int ret;
	printf("\n\t%s Stop uploading\n", TAG);
	if(uploadPid != 0)
	{
		ret = kill(uploadPid, SIGINT);
		printf("\tKill uploadPid [%d] return %d\n",uploadPid, ret);
		uploadPid = 0;
	}
}

void freeCamera(Camera *h)
{
	if(h != NULL)
	{
		free(h->name);
		free(h->url);
		free(h->decoder);
		free(h->recdir);
		free(h->rectime);
		free(h->latency);
		free(h);
	}
}

void stopCameras(void)
{
	int i;
	int ret;
	Camera *h;
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			printf("\n\t%s Free camera[%d]\n", TAG, i);
			h = camera[i];
			if(h->recPid != 0)
			{
				ret = kill(h->recPid, SIGINT);
				printf("\tKill recPid [%d] return %d\n",h->recPid, ret);
				h->recPid = 0;
			}
			if(h->mosPid != 0)
			{
				ret = kill(h->mosPid, SIGINT);
				printf("\tKill mosPid [%d] return %d\n",h->mosPid, ret);
				h->mosPid = 0;
			}
			freeCamera(h);
		}
	}
}

void print_usage(char *arg)
{
	printf("Usage: %s [OPTIONS] xml file\n", arg);
	printf("-r disable recording\n");
	printf("-m disable mosaic\n");
	printf("-u disable upload\n");
	printf("\n######### Runtime options #########\n");
	printf("h - this help\n");
	printf("x - Exit\n");
	printf("l - switch layout (single, quad, nine)\n");
	printf("s - shift camera in layout\n");
	exit(0);
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int main(int argc, char **argv)
{
    int			i, j, retVal;
	bool		addCamera;
	Camera		*h;
	Wifi		*w;
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	
    recEna = mosEna = uplEna = 1;
    if(argc >= 2)
    {
		if(strcmp(argv[1],"help") == 0 || strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"--help") == 0 || strcmp(argv[1],"-h") == 0)
			print_usage(argv[0]);

    	for(i = 1; i < argc; i++)
    	{
    		if(argv[i][0] == '-')
    		{
				if(strcmp(argv[i],"-r") == 0)
					recEna = 0;
				else if(strcmp(argv[i],"-m") == 0)
					mosEna = 0;
				else if(strcmp(argv[i],"-u") == 0)
					uplEna = 0;
				else
				{
					printf("\n\t%sERROR!!! Unsupported option\n\n", TAG);
					print_usage(argv[0]);
				}
			}
			else
			{
				filename = argv[i];
			}
    	}
    }
    else
    {
    	print_usage(argv[0]);
    }
    	
    if(filename == NULL)
    {
    	printf("\n\t%s ERROR!!! Please specify filename\n\n", TAG);
    	print_usage(argv[0]);
    }
	else
	{
		if(strcmp(get_filename_ext(filename), "xml") != 0)
		{
	    	printf("\n\t%s ERROR!!! Please specify file with xml extension\n\n", TAG);
	    	print_usage(argv[0]);
		}
	}
	
    printf("%s filename: %s recEna = %d mosEna = %d uplEna = %d\n", TAG, filename, recEna, mosEna, uplEna);

	avi_init();
	doc = xmlParseFile(filename);
	if(doc == NULL)
	{
		printf("\t\n%s ERROR!!! Not a valid xml file\n\n", TAG);
		exit(1);
	}
	cur = xmlDocGetRootElement(doc);
//    fprintf(stdout, "Root is <%s> (%i)\n", cur->name, cur->type);
	
	cur = cur->xmlChildrenNode;
	camcnt = 0;
	while (cur != NULL)
	{
//    	fprintf(stdout, "Child is <%s> (%i)\n", cur->name, cur->type);
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Camera")))
		{
			for(i = 0; i < CAM_NUM; i++)
				if(camera[i] == NULL) break;
			h = malloc(sizeof(Camera));
			memset(h, 0, sizeof(Camera));
			h->name			= (char*)xmlGetProp(cur, (const xmlChar*)"name");
			h->url			= (char*)xmlGetProp(cur, (const xmlChar*)"url");
			h->decoder		= (char*)xmlGetProp(cur, (const xmlChar*)"decoder");
			h->recdir		= (char*)xmlGetProp(cur, (const xmlChar*)"recdir");
			h->rectime		= (char*)xmlGetProp(cur, (const xmlChar*)"rectime");
			h->latency		= (char*)xmlGetProp(cur, (const xmlChar*)"latency");
			h->recPid		= 0;
			h->mosPid		= 0;
			// Check dublicate cameras (url, name, position)
			addCamera = true;
			for(j = 0; j < camcnt; j++)
			{
				if( (strcmp(camera[j]->name, h->name) == 0) ||
					(strcmp(camera[j]->url, h->url) == 0) )
				{
					printf("\n\t!!! ERROR !!! Dublicate camera found\n");
					printf("\tCheck please name and url on your xml file.\n");
					freeCamera(h);
					addCamera = false;
				}
			}
			if(addCamera)
			{
				camcnt++;
				if(camcnt <= (mosaic*mosaic))
					h->position = camcnt;
				camera[i] = h;
//				int avi_set_stream(int idx, int format, int width, int height)
			}
		}
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"WiFi")))
		{
			for(i = 0; i < WIFI_NUM; i++)
				if(wifi[i] == NULL) break;
			w = malloc(sizeof(Wifi));
			w->ssid 	= (char*)xmlGetProp(cur, (const xmlChar*)"ssid");
			w->password = (char*)xmlGetProp(cur, (const xmlChar*)"password");
			wifi[i] 	= w;
		}
		cur = cur->next;
	}

	signal(SIGCHLD, sig_handler);

	printf("########################################\n");
	printf("\tMOSAIC STARTED AT MODE %s\n",LAYER[mosaic-1]);
	printf("########################################\n");
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			if(recEna)
				startRec(h);
			if(mosEna)
				startMos(h);
/*			printf("\t%s Camera[%d]: %s\n", TAG, i, h->name);
			printf("\t\turl: %s decoder: %s\n", h->url, h->decoder);
			printf("\t\trecdir: %s\n", h->recdir);
			printf("\t\trectime: %s latency: %s position: %d\n", h->rectime, h->latency, h->position);
			printf("\t\trecPid: %d mosPid: %d\n", h->recPid, h->mosPid);
*/		}
	}

	if(uplEna)
		startUpload();
	
	for(i = 0; i < WIFI_NUM; i++)
	{
		if(wifi[i] != NULL)
		{
			w = wifi[i];
			printf("%s WiFi: %s password %s\n", TAG, w->ssid, w->password);
		}
	}
	
	run = 1;
	retVal = pthread_create(&pth_control, NULL, &control_func, argv[0]);
	if (retVal != 0)
		printf("%s can't create thread :[%s]", TAG, strerror(retVal));
	else
	    printf("%s Control thread created successfully\n", TAG);

	while(run)
	{
		for(i = 0; i < CAM_NUM; i++)
		{
			if(camera[i] != NULL)
			{
				h = camera[i];
//				printf("Camera[%d]: rec - %d mosaic - %d\n", i, h->recPid, h->mosPid);
				if(h->recPid && recEna)
				{
					retVal = kill(h->recPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->recPid, retVal);
					if(retVal == -1)//restart recording
						retVal = startRec(h);
				}
				if(h->mosPid && mosEna)
				{
					retVal = kill(h->mosPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->mosPid, retVal);
					if(retVal == -1)//restart mosaic
						retVal = startMos(h);
				}
			}
		}
		//Monitoring upload process
		if(uploadPid && uplEna)
		{
			retVal = kill(uploadPid, 0);
//					printf("kill pid %d with 0 return %d\n", uploadPid, retVal);
			if(retVal == -1)//restart upload
				retVal = startUpload();
		}

		usleep(500000);
	}
	stopCameras();
	stopUpload();
	xmlFreeDoc(doc);
	avi_exit();
	return (0);
}

void print_help(char *argv)
{
	printf("h - this help\n");
	printf("x - Exit\n");
	printf("l - switch layout (single, quad, nine)\n");
	printf("s - shift camera in layout\n");
}

void* control_func (void *arg)
{
	char c;
	unsigned int shift;
	while(run)
	{
//		c = getc(stdin);
		c = getch();
		switch(c)
		{
			case 'h':
				print_help(arg);
				break;
			case 'x':
				run = 0;
				break;
			case 'l':
				if(mosEna)
					toggle_mosaic();
				else
					printf("\tMOSAIC DISABLED!!!\n");
				break;
			case 's':
				if(mosEna)
				{
					printf("Enter shift value in range 1-%d>", camcnt);
					scanf("%d",&shift);
					shift_mosaic(shift);
				}
				else
					printf("\tMOSAIC DISABLED!!!\n");
				break;
		}
		usleep(100000);
	}
	return NULL;
}

/*
Get SIGCHLD
*/
void sig_handler(int signum)
{
	int wstatus;
	
//    printf("\nmvr Received signal %d\n", signum);
	wait(&wstatus);
//    printf("\twait return %d\n", wstatus);
}

int startRec(Camera *h)
{
	if(h->rectime && h->recdir)
	{
		printf("%s Start recordig for camera %s\n", TAG, h->name);

		h->recPid = fork();
		if(h->recPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->recPid == 0)
		{
			execlp("record", " ", h->url, h->decoder, h->recdir, h->rectime, h->name, NULL);
		}
	}
	return 0;
}

int startMos(Camera *h)
{
	char* m;
	char* p;

	if( h->position != 0 )
	{
		printf("%s Start mosaic %s for camera %s in position %d\n", TAG, LAYER[mosaic-1], h->name, h->position);
		h->mosPid = fork();
		if(h->mosPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->mosPid == 0)
		{
			m = malloc(1);
			p = malloc(1);
			sprintf(m, "%d", mosaic);
			sprintf(p, "%d", h->position);
			execlp("mosaic", " ", h->url, h->name, h->decoder, m, p, h->latency, NULL);
			free(m);
			free(p);
		}
	}
	return 0;
}

int startUpload(void)
{
	printf("%s Start upload\n", TAG);
	uploadPid = fork();
	if(uploadPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(uploadPid == 0)
	{
		execlp("upload", " ", NULL);
	}
	return 0;
}

static void stop_mosaic(bool clearPos)
{
	int i;
	int ret;
	Camera *h;
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			//Clear positions for all cameras only when change mosaic type
			if(clearPos)
				h->position = 0;
				
			if(h->mosPid != 0)
			{
				ret = kill(h->mosPid, SIGINT);
				printf("\tKill mosPid [%d] return %d\n",h->mosPid, ret);
				h->mosPid = 0;//For stop monitoring mosaic for this camera
			}
		}
	}
}

void toggle_mosaic(void)
{
	int i;
	Camera *h;
	mosaic++;
	if (mosaic > NINE) mosaic = SINGLE;
	printf("%s Toggle mosaic mode to %s\n", TAG, LAYER[mosaic-1]);
	stop_mosaic(true);
	//Assign positions for cameras
	for(i = 0; i < MIN(mosaic*mosaic, camcnt); i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			h->position = i + 1;
			startMos(h);
		}
	}
}

void shift_mosaic(unsigned int value)
{
	int i, cnt, shift;
	printf("%s %s\n", TAG, __func__);
	Camera *h;
	int wndcnt = mosaic*mosaic;

	if(value < 0)
		shift = 1;
	else if (value > camcnt)
		shift = camcnt;
	else
		shift = value;
	cnt = MIN(wndcnt, shift);

	stop_mosaic(false);

	//Clear position for value cameras
	do{
		for(i = 0; i < CAM_NUM; i++)
		{
			if(camera[i] != NULL)
			{
				h = camera[i];
				if(h->position == cnt)
				{
					camera[i]->position = 0;
					cnt--;
					if(cnt == 0) break;
				}
			}
		}
	}while(cnt >= 1);

	if( (i + shift) > camcnt )
	{
		shift = (i + shift)%camcnt;
		i = 0;
	}
	cnt = 0;
	//Assign position in mosaic for cameras
	//Start from next camera in list
	for(i = i + shift; i < camcnt; i++)
	{
		h = camera[i];
		h->position = ++cnt;
		startMos(h);
		if(cnt == wndcnt) break;
	}
	//Workaround in case of reaching the end of the list 
	//when not busy with all the mosaic window
	if(cnt < MIN(wndcnt, camcnt))//in case of camcnt < wndcnt
	{
		for(i = 0; i < camcnt; i++)
		{
			h = camera[i];
			h->position = ++cnt;
			startMos(h);
			if(cnt == wndcnt || cnt == camcnt) break;//in case of camcnt < wndcnt
		}
	}
}

