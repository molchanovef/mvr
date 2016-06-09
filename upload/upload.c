#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
//#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>

#define BASE_DIR		"/media/sda1/MVR"
#define FTP_SERVER		"192.168.1.2"
#define FTP_USER		"tvhelp"
#define FTP_PASSWD		"tvhelp"
#define TAG				"UPLOAD:"
//#define WITH_CONTROL_THREAD
//#define WITH_FTP_FORK

int run;
pthread_t pth_control;
char baseDir[256];
char ftpServer[256];
char ftpUser[256];
char ftpPasswd[256];
char oldDir[256];
#ifdef WITH_FTP_FORK
pid_t ftpPid = 0;
#endif
void sig_handler(int signum);
#ifdef WITH_CONTROL_THREAD
void* control_func (void *arg);
#endif

void print_help(char *argv)
{
	printf("Usage %s <base_dir> <ftp_server> <ftp_user> <ftp_password>\n",argv);
}

int uploadToFTPServer(char *file)
{
	int ret;
	char cmd[1024];
//	printf("%s %s\n",__func__, file);
#ifdef WITH_FTP_FORK
	ret = kill(ftpPid, 0);
//	printf("kill pid %d with 0 return %d\n", ftpPid, ret);
	if(ret == -1 || ftpPid == 0)//previous ftpput complete
	{
		ftpPid = fork();
		if(ftpPid > 0)
		{
			printf("%s create child process with PID %d\n", TAG, ftpPid);
		}
		if(ftpPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(ftpPid == 0)
		{
			printf("Start of child ftp process\n");
#endif
			sprintf(cmd, "ftpput -u %s -p %s %s %s", ftpUser, ftpPasswd, ftpServer, file);
			printf("%s ftpput %s\n",TAG, file);
			ret = system(cmd);
			//TODO check ftpput return remove file
			if(ret != 0)
				printf("\t\nftpput return ERROR %d\n", ret);
			if(ret == 256) exit(1);
			if(ret != -1 && ret != 127)
			{
				if(ret == EXIT_SUCCESS)
				{
					sprintf(cmd, "rm -f %s", file);
					printf("%s remove %s\n",TAG, file);
					system(cmd);
				}
				else if(ret == EXIT_FAILURE)
					printf("ERROR!!! Can't upload %s to ftp %s\n", file, ftpServer);
			}
#ifdef WITH_FTP_FORK
			printf("End of child ftp process\n");
			return 0;
		}
	}
#endif
	return 0;
}

void removeDir(char *path)
{
	char cmd[1024];
	printf("%s %s\n",__func__, path);
	sprintf(cmd, "rm -rf %s", path);
	printf("%s\n",cmd);
	system(cmd);
	sprintf(cmd, "sync");
	printf("%s\n",cmd);
	system(cmd);
}

int searchOldestFile(char *path)
{
	int currFile;
	int temp = 0x7FFFFFFF;
	char oldFile[256] = {0};
	char str[256] = {0};
	char recfile[256] = {0};
	int hour, min, sec;
	DIR *dp;
	struct dirent *ep;
	hour = min = sec = 0;
	time_t ct = time(NULL);
	struct tm *t = localtime(&ct);
	int year, month, day;
	int currDay, currDir;
	currDay = currDir = 0;
//	printf("%s in %s\n",__func__, path);
	
	dp = opendir(path);
	if( dp != NULL)
	{
		while ((ep = readdir (dp)))
		{
			if (3 == sscanf(ep->d_name, "%d_%d_%d", &hour, &min, &sec))
			{
//				printf ("%s hour %d min %d sec %d\n", ep->d_name, hour, min, sec);
				currFile = hour << 12 | min << 6 | sec;
				if(temp > currFile)
				{
					temp = currFile;
					strcpy(oldFile, ep->d_name);
				}
			}
		}
		(void) closedir (dp);
	}
	else
	{
		perror ("searchOldFile: Couldn't open the directory");
		return -ENOTDIR;
	}
	
	sprintf(recfile,"%s/rec",path);

	currDay = (1900+t->tm_year) << 9 | (t->tm_mon+1) << 5 | t->tm_mday;
	if (3 == sscanf(oldDir, "%d_%d_%d", &year, &month, &day))
	{
		currDir = year << 9 | month << 5 | day;
	}

	if(strlen(oldFile))
	{
//		printf("oldest file %s\n",oldFile);
		sprintf(str,"%s/%s",path, oldFile);
		uploadToFTPServer(str);
	}
	else if( currDay != 0 && currDir != 0 && currDay > currDir ) //check if directory not current day and remove it
	{
		printf("\t\n%s !!! not today dir !!!\n", __func__);
		removeDir(path);
	}
/*	else if( access( recfile, F_OK ) == -1 )
	{
		printf("\t\n%s !!! rec file missing !!!\n", __func__);
		removeDir(path);
	}
*/	return 0;
}

int searchOldestDir(char *path, char *dir)
{
	int year, month, day;
	int currDir;
	int temp = 0x7FFFFFFF;
	DIR *dp;
	struct dirent *ep;   
	year = month = day = 0;
	oldDir[0] = '\0';  
	dp = opendir (path);
	if (dp != NULL)
	{
		while ((ep = readdir (dp)))
		{
			if (3 == sscanf(ep->d_name, "%d_%d_%d", &year, &month, &day))
			{
//				printf ("%s year %d month %d day %d\n", ep->d_name, year, month, day);
				currDir = year << 9 | month << 5 | day;
				if(temp > currDir)
				{
					temp = currDir;
					strcpy(oldDir,ep->d_name);
				}
			}
		}
		(void) closedir (dp);
	}
	else
	{
		perror ("searchOldDir: Couldn't open the directory");
		return -ENOTDIR;
	}

	if(strlen(oldDir))
	{
//		printf("oldest dir %s\n",oldDir);
		sprintf(dir, "%s/%s", path, oldDir);
		return 0;
	}
	return -ENOENT;
}

//Search in baseDir old files and upload them on ftp server
int main (int argc, char *argv[])
{
	int ret, oldestDirEmpty;
	char dir[256];
	char camera[256];
	
	DIR *dp;
	struct dirent *ep;   
	
	print_help(argv[0]);
	if(argc < 2)
		strcpy(baseDir, BASE_DIR);
	else
		strcpy(baseDir,argv[1]);
	if(argc < 3)
		strcpy(ftpServer, FTP_SERVER);
	else
		strcpy(ftpServer, argv[2]);
	if(argc < 4)
		strcpy(ftpUser, FTP_USER);
	else
		strcpy(ftpUser, argv[3]);
	if(argc < 5)
		strcpy(ftpPasswd, FTP_PASSWD);
	else
		strcpy(ftpPasswd, argv[4]);

	printf("baseDir (%s) FTP: server (%s), user (%s), passwd (%s)\n", baseDir, ftpServer, ftpUser, ftpPasswd);
	
//	sprintf(cmd, "ping %s", ftpServer);
//	system(cmd);
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\t\n%s can't catch SIGINT\n", TAG);
#ifdef WITH_FTP_FORK
	signal(SIGCHLD, sig_handler);
#endif	
	run = 1;
#ifdef WITH_CONTROL_THREAD	
	ret = pthread_create(&pth_control, NULL, &control_func, argv[0]);
	if (ret != 0)
	{
		printf("\n%s can't create control thread :[%s]", TAG, strerror(ret));
		return -1;
	}
	else
	    printf("\n%s control thread created successfully\n", TAG);
#endif
	while(run)
	{
		//Search for oldest day
//		printf("%s while run\n", TAG);
		dir[0] = '\0';
		ret = searchOldestDir(baseDir, dir);
		if(ret == 0)
		{
			dp = opendir(dir);
			if( dp != NULL)
			{
				camera[0] = 0;
				oldestDirEmpty = 0;//for check if any camera directory exist in oldest dir
				//In each camera folder search oldest file
				while ((ep = readdir (dp)) && run)
				{
					int retVal;
					if( (0 == strcmp(ep->d_name,".") )|| ( 0 == strcmp(ep->d_name,"..")) ) continue;
//					printf ("camera dir %s\n", ep->d_name);
					sprintf(camera,"%s/%s", dir, ep->d_name);
					retVal = searchOldestFile(camera);
					if(retVal == -ENOTDIR) exit(-1);
					oldestDirEmpty++;
				}
				(void) closedir (dp);
				if(oldestDirEmpty == 0)//oldest dir empty so remove it
					removeDir(dir);
			}
			else
			{
				printf("Couldn't open the directory %s\n",dir);
				ret = -1;
				break;
			}
		}
/*		else if(ret == -ENOENT)
		{
			printf("All files uploaded! Exit.\n");
			run = 0;
			break;
		}
*/		else if(ret == -ENOTDIR)
		{
			printf("Directory %s missing. Exit.\n",baseDir);
			run = 0;
//			break;
		}
		usleep(10000);
	}
#ifdef WITH_CONTROL_THREAD
	if(pth_control)
	    pthread_join(pth_control, NULL);
#endif
//	sprintf(cmd, "killall -9 ftpput");
//	system(cmd);

	printf("\t\n%s STOPPED.\n", TAG);
	return 0;
}

void sig_handler(int signum)
{
#ifdef WITH_FTP_FORK
	char cmd[256];
	int wstatus;
	if(signum == SIGCHLD)	
	{
//	    printf("\t\n%s Received signal %d\n", TAG, signum);
		wait(&wstatus);
//	    printf("\t\n%s wait return %d\n", TAG, wstatus);
	}
#endif
	if(signum == SIGINT)
	{
	    printf("\t\n%s Received signal %d\n", TAG, signum);
#ifdef WITH_FTP_FORK
		sprintf(cmd, "kill %d", ftpPid);
		printf("%s %s\n", TAG, cmd);
		system(cmd);
#endif
    	run = 0;
    }
}

#ifdef WITH_CONTROL_THREAD
void* control_func (void *arg)
{
	char c;
	while(run)
	{
		c = getc(stdin);
		switch(c)
		{
			case 'h':
				break;
			case 'x':
				run = 0;
				break;
		}
		usleep(10000);
	}
	printf("\t\n%s control thread STOPPED.\n", TAG);
	return arg;
}
#endif
