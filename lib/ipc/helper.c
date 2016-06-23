#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "helper.h"
#define MOD "***"
#include "mylog.h"

// restarts process if possible? otherwise terminate process
void restart(char **argv)
{
	char path[256];
	int n = readlink("/proc/self/exe", path, 255);
	if (n > 0)
	{
		path[n] = 0;
		LOGI("restart %s", path);
		pid_t pid = fork();
		if (pid == 0)
		{
			usleep(100000);
			execv(path, argv);
		}
		else if (pid > 0)
		{
			abort();
		}
	}
	LOGE("terminate %s (%s)", path, strerror(errno));
	exit(0);
}
