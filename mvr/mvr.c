#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define CAM_NUM		32
#define WIFI_NUM	32
#define TAG			"MVR:"

pthread_t pth_control;
int run;
int recEna, mosEna, uplEna;
char *filename;
pid_t uploadPid;

typedef struct _Camera
{
	char *name;
	char *url;
	char *decoder;
	char *recdir;
	char *rectime;
	char *latency;
	char *position;
	char *mosaic;
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

void stopUpload(void)
{
	int ret;
	printf("\n\t%s Stop uploading\n", TAG);
	if(uploadPid != 0)
	{
		ret = kill(uploadPid, SIGINT);
		printf("\tKill uploadPid [%d] return %d\n",uploadPid, ret);
	}
}

void free_cameras(void)
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
			}
			if(h->mosPid != 0)
			{
				kill(h->mosPid, SIGINT);
				printf("\tKill mosPid [%d] return %d\n",h->mosPid, ret);
			}
			free(h->name);
			free(h->url);
			free(h->decoder);
			free(h->recdir);
			free(h->rectime);
			free(h->latency);
			free(h->mosaic);
			free(h->position);
			free(h);
		}
	}
}

void print_usage(char *arg)
{
	fprintf(stderr, "Usage: %s [OPTIONS] xml file\n", arg);
	fprintf(stderr, "-r disable recording\n");
	fprintf(stderr, "-m disable mosaic\n");
	fprintf(stderr, "-u disable upload\n");
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

	doc = xmlParseFile(filename);
	if(doc == NULL)
	{
		printf("\t\n%s ERROR!!! Not a valid xml file\n\n", TAG);
		exit(1);
	}
	cur = xmlDocGetRootElement(doc);
    fprintf(stdout, "Root is <%s> (%i)\n", cur->name, cur->type);
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
//    	fprintf(stdout, "Child is <%s> (%i)\n", cur->name, cur->type);
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Camera")))
		{
			for(i = 0; i < CAM_NUM; i++)
				if(camera[i] == NULL) break;
			h = malloc(sizeof(Camera));
			h->name			= (char*)xmlGetProp(cur, (const xmlChar*)"name");
			h->url			= (char*)xmlGetProp(cur, (const xmlChar*)"url");
			h->decoder		= (char*)xmlGetProp(cur, (const xmlChar*)"decoder");
			h->recdir		= (char*)xmlGetProp(cur, (const xmlChar*)"recdir");
			h->rectime		= (char*)xmlGetProp(cur, (const xmlChar*)"rectime");
			h->latency		= (char*)xmlGetProp(cur, (const xmlChar*)"latency");
			h->mosaic	 	= (char*)xmlGetProp(cur, (const xmlChar*)"mosaic");
			h->position 	= (char*)xmlGetProp(cur, (const xmlChar*)"position");
			h->recPid		= 0;
			h->mosPid		= 0;
			camera[i] 		= h;
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

	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			if(recEna)
				startRec(h);
			if(mosEna)
				startMos(h);
			printf("\t%s Camera[%d]: %s\n", TAG, i, h->name);
			printf("\t\turl: %s decoder: %s\n", h->url, h->decoder);
			printf("\t\trecdir: %s\n", h->recdir);
			printf("\t\trectime: %s latency: %s mosaic %s position: %s\n", h->rectime, h->latency, h->mosaic, h->position);
			printf("\t\trecPid: %d mosPid: %d\n", h->recPid, h->mosPid);
		}
	}

	if(uplEna)
		startUpload();
	
	for(i = 0; i < WIFI_NUM; i++)
	{
		if(wifi[i] != NULL)
		{
			w = wifi[i];
			printf("\t%s WiFi: %s password %s\n", TAG, w->ssid, w->password);
		}
	}
	
	run = 1;
	retVal = pthread_create(&pth_control, NULL, &control_func, argv[0]);
	if (retVal != 0)
		printf("\n%s can't create thread :[%s]", TAG, strerror(retVal));
	else
	    printf("\n%s Control thread created successfully\n", TAG);

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
	free_cameras();
	stopUpload();
	xmlFreeDoc(doc);
	return (0);
}

void print_help(char *argv)
{
	printf("Usage %s <setup.xml>\n",argv);
	printf("h - this help\n");
	printf("x - Exit\n");
}

void* control_func (void *arg)
{
	char c;
	while(run)
	{
		c = getc(stdin);
		switch(c)
		{
			case 'h':
				print_help(arg);
				break;
			case 'x':
				run = 0;
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
		printf("\n\t%s Start recordig for camera %s\n", TAG, h->name);

		h->recPid = fork();
		if(h->recPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->recPid == 0)
		{
			execl("record", " ", h->rectime, h->recdir, h->url, h->decoder, h->name, NULL);
		}
//		sleep(1);
	}
	return 0;
}

int startMos(Camera *h)
{
	if(h->mosaic && h->position)
	{
		printf("\n\t%s Start mosaic for camera %s\n", TAG, h->name);
		h->mosPid = fork();
		if(h->mosPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->mosPid == 0)
		{
			execl("mosaic", " ", h->url, h->decoder, h->mosaic, h->position, h->latency, h->name, NULL);
		}
	}
//	sleep(1);
	return 0;
}

int startUpload(void)
{
	printf("\n\t%s Start upload\n", TAG);
	uploadPid = fork();
	if(uploadPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(uploadPid == 0)
	{
		execl("upload", " ", NULL);
	}
	return 0;
}

