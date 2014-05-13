#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "rgr_lib.h"

void setser(int fd) 
{
	struct termios newtio;

	if (tcgetattr(fd, &newtio) == -1)
		error_exit(true, "tcgetattr failed");

	newtio.c_iflag = IGNBRK; // | ISTRIP;
	newtio.c_oflag = 0;
	newtio.c_cflag = B115200 | CS8 | CREAD | CLOCAL | CSTOPB;
	newtio.c_lflag = 0;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;

	tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1)
		error_exit(true, "tcsetattr failed");
}

int main(int argc, char *argv[])
{
	const char *dev = "/dev/ttyAMA0";

	if (argc == 2)
		dev = argv[1];

	int fd = open(dev, O_RDWR);
	if (fd == -1)
		error_exit(true, "cannot open %s", dev);

	setser(fd);

	for(;;)
	{

		char *p = NULL;
		int len = 0;

		int rc = recv_msg(fd, &p, &len, false);

		if (rc == B_OK)
		{
			time_t now = time(NULL);
			char *ts = ctime(&now), *lf = strchr(ts, '\n');
			*lf = 0x00;

			char buffer[65] = { 0 };
			memcpy(buffer, p, len);
			buffer[64] = 0x00;

			printf("%s %s\n", ts, buffer);

			free(p);
		}
		else if (rc != B_ERR_TO)
		{
			printf("ERR: %d\n", rc);
		}
	}

	return 0;
}
