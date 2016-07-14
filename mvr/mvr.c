#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
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

#define CHECK_DUBLICATE
#define CAM_NUM		32
#define WIFI_NUM	32
#define TAG			"MVR:"
#define MIN(a,b)	((a)<(b)?(a):(b))
#define XMLFILE		"/tvh/xml.xml"
#define RECDIR		"/media/sda1/MVR"
#define RECTIME		60				//seconds
#define LATENCY		0				//ms

pthread_t pth_control;
int run;
int recEna, mosEna, uplEna;
int camcnt;
pid_t uploadPid;

const char *LAYER[] = {"SINGLE", "2x2", "3x3"};
typedef enum layer_s {SINGLE=1, QUAD, NINE} layer_t;
layer_t mosaic = QUAD;

typedef struct _Camera
{
	char *name;
	char *ipaddr;
	char *login;
	char *password;
	char *decoder;
	char *stream[3];
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
void toggle_mosaic(unsigned int value);
void shift_mosaic(unsigned int value);
void addLoginPasswd(Camera *h);
int mvr_get_streams(Camera *h);

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
		free(h->ipaddr);
		free(h->login);
		free(h->password);
		free(h->decoder);
		free(h->stream[0]);
		free(h->stream[1]);
		free(h->stream[2]);
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
    int			i, retVal;
	bool		addCamera;
	char 		filename[64] = {0};
	Camera		*h;
	Wifi		*w;
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	
    recEna = mosEna = uplEna = 1;
    if(argc > 1)
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
				sprintf(filename,"%s",argv[i]);
			}
    	}
    }
/*    else
    {
    	print_usage(argv[0]);
    }
*/
    if(strlen(filename) == 0)
    {
		printf("\n\t%s Using default xml file %s\n\n", TAG, XMLFILE);
		sprintf(filename,"%s",XMLFILE);
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
			h->stream[0]	= (char*)xmlGetProp(cur, (const xmlChar*)"stream0");
			h->stream[1]	= (char*)xmlGetProp(cur, (const xmlChar*)"stream1");
			h->stream[2]	= (char*)xmlGetProp(cur, (const xmlChar*)"stream2");
			h->ipaddr		= (char*)xmlGetProp(cur, (const xmlChar*)"ipaddr");
			h->login		= (char*)xmlGetProp(cur, (const xmlChar*)"login");
			h->password		= (char*)xmlGetProp(cur, (const xmlChar*)"password");
			h->decoder		= (char*)xmlGetProp(cur, (const xmlChar*)"decoder");
			h->recPid		= 0;
			h->mosPid		= 0;

			// Check dublicate cameras (url, name, position)
			addCamera = true;
#ifdef CHECK_DUBLICATE
			for(i = 0; i < camcnt; i++)
			{
				if( (strcmp(camera[i]->name, h->name) == 0) ||
					(strcmp(camera[i]->ipaddr, h->ipaddr) == 0) )
				{
					printf("\n\t!!! ERROR !!! Dublicate camera found\n");
					printf("\tCheck please name and url on your xml file.\n");
					addCamera = false;
				}
			}
#endif
			if(h->stream[0] == NULL || h->stream[1] == NULL || h->stream[2] == NULL)
			{
				if(mvr_get_streams(h) != 0)
					addCamera = false;
			}
			else
			{
				addLoginPasswd(h);
			}

			if(addCamera)
			{
				camcnt++;
				if(camcnt <= (mosaic*mosaic))
					h->position = camcnt;
				camera[i] = h;
//				int avi_set_stream(int idx, int format, int width, int height)
			}
			else
			{
				freeCamera(h);
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
			printf("\t\tipaddr: %s decoder: %s\n", h->ipaddr, h->decoder);
			printf("\t\tposition: %d\n", h->position);
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

	for(i = 0; i < WIFI_NUM; i++)
	{
		if(wifi[i] != NULL)
		{
			free(wifi[i]);
		}
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
	unsigned int value;
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
				{
					printf("Enter mosaic type %d for %s %d for %s %d for %s>", SINGLE, LAYER[0], QUAD, LAYER[1], NINE, LAYER[2]);
					scanf("%d",&value);
					toggle_mosaic(value);
				}
				else
					printf("\tMOSAIC DISABLED!!!\n");
				break;
			case 's':
				if(mosEna)
				{
					printf("Enter shift value in range 1-%d>", camcnt);
					scanf("%d",&value);
					shift_mosaic(value);
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
	char r[2];
	printf("%s Start recordig for camera %s\n", TAG, h->name);

	h->recPid = fork();
	if(h->recPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(h->recPid == 0)
	{
		sprintf(r, "%d", RECTIME);
		execlp("record", " ", h->stream[0], h->decoder, RECDIR, r, h->name, NULL);
	}
	return 0;
}

int startMos(Camera *h)
{
	char m[2];
	char p[2];
	char l[2];
	if( h->position != 0 )
	{
		printf("%s Start mosaic %s for %s at position %d\n", TAG, LAYER[mosaic-1], h->name, h->position);
		h->mosPid = fork();
		if(h->mosPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->mosPid == 0)
		{
			sprintf(m, "%d", mosaic);
			sprintf(p, "%d", h->position);
			sprintf(l, "%d", LATENCY);
			execlp("mosaic", " ", h->stream[1], h->name, h->decoder, m, p, l, NULL);
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

void toggle_mosaic(unsigned int type)
{
	int i;
	Camera *h;
	if(type < SINGLE || type > NINE)
		return;
	mosaic = type;
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
#if 0
static void searchIPinURL(char *url, char *ip)
{
	char *p = url;
	int index = 7;

	if(strncmp(url,"rtsp://",index) == 0)
	{
		p = url + index;
		strcpy(ip, p);
	//	printf("%s %s %s\n", TAG, __func__, ip);
	}
}
#endif
static void searchStreamInString(Camera *h, const char* str)
{
	int i, j;
	char *rtsp;
	char params[3][32];
	
	for(i = 0; i < 3; i++)
	{
		sprintf(params[0], "streamurl%d", i+1);
		sprintf(params[1], "streamname%d", i+1);
		sprintf(params[2], "stream%dname", i+1);

//		printf("%s %s search nodes: %s len %d, %s len %d, %s len %d\n",
//		 TAG, __func__, params[0], strlen(params[0]), params[1], strlen(params[1]), params[2], strlen(params[2]));
		for(j = 0; j < 3; j++)
		{
			if(strncmp(str, params[j], strlen(params[j])) == 0)
			{
//				printf("compare %s %s\n",str, params[j]);
				rtsp = strstr(str, "rtsp");
				if(rtsp)
				{
					if(h->stream[i] == NULL)
					{
						h->stream[i] = malloc(256);
						if(strcpy(h->stream[i], rtsp))
						{
//							printf("%s\n", h->stream[i]);
						}
					}
				}
			}
		}
	}
}

//Add login and password in streams
void addLoginPasswd(Camera *h)
{
	int i;
	char *p;
	char tmp[256];
	printf("%s %s\n", TAG, __func__);
	for(i = 0; i < 3; i++)
	{
		p = h->stream[i];
		if(p)
		{
			if(strncmp(h->stream[i], "rtsp://", 7) == 0)
			{
				p += 7;
				strcpy(tmp, p);
				free(h->stream[i]);
				h->stream[i] = malloc(256);
				sprintf(h->stream[i], "rtsp://%s:%s@%s", h->login, h->password, tmp);
			}
		}
	}
	printf("Streams for %s:\n", h->name);
	for(i = 0; i < 3; i++)
		printf("\t%s\n", h->stream[i]);
}

//Parse ini.htm for get streams url
int mvr_get_streams(Camera *h)
{
	char str[1024];
//	char ip[64];
	char filename[16] = "ini.htm";
	htmlDocPtr	doc;
	htmlNodePtr	currentNode;
	bool beginOfNode = true;
	xmlAttrPtr attrNode;
	int i;
	FILE *fd;

	printf("%s %s\n", TAG, __func__);
//	searchIPinURL(h->url, ip);

	sprintf(str, "wget http://%s:%s@%s/ini.htm -O %s", h->login, h->password, h->ipaddr, filename);
	system(str);

	doc = htmlParseFile(filename, NULL);
	if(!doc) return -1;
	currentNode = doc->children;

	while (currentNode)
	{
		// output node if it is an element
		if (beginOfNode)
		{
		    if (currentNode->type == XML_ELEMENT_NODE)
		    {

		        for (attrNode = currentNode->properties;
		             attrNode; attrNode = attrNode->next)
		        {
		            xmlNodePtr contents = attrNode->children;

		            printf("%s='%s'\n", attrNode->name, contents->content);
		        }

		    }
		    else if (currentNode->type == XML_TEXT_NODE)
		    {
				searchStreamInString(h, (const char*)currentNode->content);
//	            printf("%s\n", currentNode->content);
		    }
		    else if (currentNode->type == XML_COMMENT_NODE)
		    {
		        printf("/* %s */\n", currentNode->name);
		    }
		}

		if (beginOfNode && currentNode->children)
		{
		    currentNode = currentNode->children;
		    beginOfNode = true;
		}
		else if (beginOfNode && currentNode->next)
		{
		    currentNode = currentNode->next;
		    beginOfNode = true;
		}
		else
		{
		    currentNode = currentNode->parent;
		    beginOfNode = false; // avoid going to siblings or children

		    // close node
		    if (currentNode && currentNode->type == XML_ELEMENT_NODE)
		    {
//		        printf("</%s>", currentNode->name);
		    }
		}
	}

	char *p;
	//Workaround for DM355
	if(h->stream[0] == NULL)
	{
		fd = fopen(filename, "r+b");
		while(fgets(str, sizeof(str), fd))
		{
			searchStreamInString(h, (const char*)str);
		}
		fclose(fd);
		//Remove <br>
		for(i = 0; i < 3; i++)
		{
			p = h->stream[i];
			while(*p != '<') p++;
			*p = 0;
		}
	}

	addLoginPasswd(h);
	xmlFreeDoc(doc);
	unlink(filename);

	if(h->stream[0] == NULL || h->stream[1] == NULL || h->stream[2] == NULL)
		return -2;

	return 0;
}

