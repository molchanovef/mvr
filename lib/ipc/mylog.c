#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "mylog.h"

void mylog(char c, char *str)
{
	char *p = str, *q = str;
	do {
		if (*p != '\n' && *p != '\r') *q++ = *p; 
	} while (*p++);
	int level = 0;
	switch (c)
	{
	case 'R': fprintf(stderr, "\033[31m%s\033[0m\n", str); level = 1; break;
	case 'G': fprintf(stderr, "\033[32m%s\033[0m\n", str); level = 2; break;
	case 'B': fprintf(stderr, "\033[34m%s\033[0m\n", str); level = 3; break;
	case 'Y': fprintf(stderr, "\033[33m%s\033[0m\n", str); level = 5; break;
	case 'M': fprintf(stderr, "\033[35m%s\033[0m\n", str); level = 5; break;
	case 'C': fprintf(stderr, "\033[36m%s\033[0m\n", str); level = 5; break;
	default:  fprintf(stderr, "%s\n", str); level = 4; break;
	}
	char fn[64];
	struct stat st;
	if (level <= 1 && stat("/mnt/mmc/log.err", &st) == 0) strcpy(fn, "/mnt/mmc/log.err");
	else if (level <= 2 && stat("/mnt/mmc/log.wrn", &st) == 0) strcpy(fn, "/mnt/mmc/log.wrn");
	else if (level <= 3 && stat("/mnt/mmc/log.inf", &st) == 0) strcpy(fn, "/mnt/mmc/log.inf");
	else if (level <= 4 && stat("/mnt/mmc/log.trc", &st) == 0) strcpy(fn, "/mnt/mmc/log.trc");
	else if (stat("/mnt/mmc/log.txt", &st) == 0) strcpy(fn, "/mnt/mmc/log.txt");
	else return;
	FILE *log = fopen(fn, "at");
	if (log)
	{
		time_t ct = time(NULL);
		struct tm *t = localtime(&ct);
		fprintf(log, "%02d.%02d.%02d %02d:%02d:%02d %s\r\n", t->tm_mday, t->tm_mon+1, 1900+t->tm_year, t->tm_hour, t->tm_min, t->tm_sec, str);
		fflush(log);
		fclose(log);
	}
}
