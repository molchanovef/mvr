#include <stdio.h>

void print_usage(char *app)
{
	printf("%s application connects to IPCAM using given url\n", app);
	printf("for get and save frames in tmpfs directory.\n");
	printf("Another programs (mosaic, record) reads frames\n");
	printf("from tmpfs for them own purposes.\n");
	printf("Usage %s <rtsp url> <name>\n", app);
}

int main(int argc, char* argv[])
{
	if(argc < 2)
		print_usage(argv[0]);
	return 0;
}
