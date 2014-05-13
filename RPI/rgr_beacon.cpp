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
	const char *msg = "www.slimwinnen.nl";
	int interval = 5;
	// bool add_header
	bool verbose = true;

	if (argc >= 3)
	{
		dev = argv[1];
		msg = argv[2];

		if (argc == 4)
			interval = atoi(argv[3]);
	}

	int fd = open(dev, O_RDWR);
	if (fd == -1)
		error_exit(true, "cannot open %s", dev);

	setser(fd);

	int msg_len = strlen(msg);

	int buffer_len = msg_len + 2;
	char *buffer = (char *)malloc(buffer_len);
	buffer[0] = buffer[1] = 32;
	memcpy(&buffer[2], msg, msg_len);

	for(;;)
	{
		if (verbose)
		{
			time_t now = time(NULL);
			char *ts = ctime(&now), *lf = strchr(ts, '\n');

			*lf = 0x00;

			printf("%s send message\n", ts);
		}

		if (!send_msg(fd, buffer, buffer_len))
			error_exit(false, "transmit failed");

		for(;;)
		{
			struct pollfd fds[1];

			fds[0].fd = fd;
			fds[0].events = POLLIN;
			fds[0].revents = 0;

			if (poll(fds, 1, 500) == 0)
			{
				if (verbose)
					printf(" T/O waiting for reply\n");

				break;
			}

			unsigned char result = 0;
			if (read(fd, &result, 1) != 1)
				error_exit(true, "failed reading response from serial port");

			if (result == B_START) // data
			{
				if (verbose)
					printf(" receiving message...\n");

				char *p = NULL;
				int len = 0;

				int rc = recv_msg(fd, &p, &len, true);

				if (verbose)
				{
					if (rc == B_OK)
					{
						char buffer[65] = { 0 };
						memcpy(buffer, p, len);
						buffer[64] = 0x00;
						printf(" |%s|\n", buffer);
					}
					else
					{
						printf(" err: %d\n", rc);
					}
				}

				free(p);
			}
			else if (result == B_OK)
			{
				if (verbose)
					printf(" OK\n");

				break;
			}
			else
			{
				if (verbose)
					printf(" ERR: %d\n", result);

				break;
			}
		}

		sleep(interval);
	}

	return 0;
}
