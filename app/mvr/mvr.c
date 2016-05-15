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

#define NUM_OF_CAM		32

pthread_t pth_control;
int run;

typedef struct _Camera
{
	char *name;
	char *url;
	char *recdir;
	char *rectime;
	char *latency;
	char *position;
	char *mosaic;
	pid_t recPid;
	pid_t mosPid;
} Camera;
Camera	*camera[NUM_OF_CAM];

void* control_func (void *arg);
void sig_handler(int signum);
int startRec(Camera *h);
int startMos(Camera *h);

void free_cameras(void)
{
	int i;
	int ret;
	Camera *h;
	for(i = 0; i < NUM_OF_CAM; i++)
	{
		if(camera[i] != NULL)
		{
			printf("\n\tFree camera[%d]\n",i);
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
			free(h->recdir);
			free(h->rectime);
			free(h->latency);
			free(h->mosaic);
			free(h->position);
			free(h);
		}
	}
}

int main(int argc, char **argv)
{
    char *ssid, *password;
    int i;
	Camera *h;
	int retVal;
	
    if (argc < 2) {
    	fprintf(stderr, "Usage: %s filename.xml\n", argv[0]);
    	return 1;
    }
	xmlDocPtr    doc;
	xmlNodePtr   cur;

	doc = xmlParseFile(argv[1]);
	cur = xmlDocGetRootElement(doc);
    fprintf(stdout, "Root is <%s> (%i)\n", cur->name, cur->type);
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
//    	fprintf(stdout, "Child is <%s> (%i)\n", cur->name, cur->type);

		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Camera")))
		{
			for(i = 0; i < NUM_OF_CAM; i++)
				if(camera[i] == NULL) break;
			h = malloc(sizeof(Camera));
			h->name			= (char*)xmlGetProp(cur, (const xmlChar*)"name");
			h->url			= (char*)xmlGetProp(cur, (const xmlChar*)"url");
			h->recdir		= (char*)xmlGetProp(cur, (const xmlChar*)"recdir");
			h->rectime		= (char*)xmlGetProp(cur, (const xmlChar*)"rectime");
			h->latency		= (char*)xmlGetProp(cur, (const xmlChar*)"latency");
			h->mosaic	 	= (char*)xmlGetProp(cur, (const xmlChar*)"mosaic");
			h->position 	= (char*)xmlGetProp(cur, (const xmlChar*)"position");
			h->recPid		= 0;
			h->mosPid		= 0;
			camera[i] = h;

		}
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"WiFi")))
		{
			ssid =		(char*)xmlGetProp(cur, (const xmlChar*)"ssid");
			password =	(char*)xmlGetProp(cur, (const xmlChar*)"password");
			printf("\tWiFi: %s password %s\n", ssid, password);
		}
		cur = cur->next;
	}
	
	signal(SIGCHLD, sig_handler);

	for(i = 0; i < NUM_OF_CAM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			startRec(h);
			startMos(h);
			printf("\tCamera[%d]: %s\n", i, h->name);
			printf("\t\turl: %s\n", h->url);
			printf("\t\trecdir: %s\n", h->recdir);
			printf("\t\trectime: %s latency: %s mosaic %s position: %s\n", h->rectime, h->latency, h->mosaic, h->position);
			printf("\t\trecPid: %d mosPid: %d\n", h->recPid, h->mosPid);
		}
	}
	
	run = 1;
	retVal = pthread_create(&pth_control, NULL, &control_func, argv[0]);
	if (retVal != 0)
		printf("\ncan't create thread :[%s]", strerror(retVal));
	else
	    printf("\n Control thread created successfully\n");

	while(run)
	{
		for(i = 0; i < NUM_OF_CAM; i++)
		{
			if(camera[i] != NULL)
			{
				h = camera[i];
//				printf("Camera[%d]: rec - %d mosaic - %d\n", i, h->recPid, h->mosPid);
				if(h->recPid)
				{
					retVal = kill(h->recPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->recPid, retVal);
					if(retVal == -1)//restart recording
						retVal = startRec(h);
				}
				if(h->mosPid)
				{
					retVal = kill(h->mosPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->mosPid, retVal);
					if(retVal == -1)//restart mosaic
						retVal = startMos(h);
				}
			}
		}
		usleep(1000000);
	}
	free_cameras();
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
	h->recPid = fork();
	if(h->recPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(h->recPid == 0)
	{
		execl("record", " ", h->rectime, h->recdir, h->url, NULL);
	}
	return 0;
}

int startMos(Camera *h)
{
	h->mosPid = fork();
	if(h->mosPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(h->mosPid == 0)
	{
		execl("mosaic", " ", h->url, h->mosaic, h->position, h->latency, NULL);
	}
	return 0;
}
