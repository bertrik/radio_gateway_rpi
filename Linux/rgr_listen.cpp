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

int main(int argc, char *argv[])
{
	const char *dev = "/dev/ttyAMA0";

	if (argc == 2)
		dev = argv[1];

	int fd = open_ser_dev(dev);

	for(;;)
	{

		unsigned char *p = NULL;
		int len = 0;
		uint8_t addr = 255;

		int rc = recv_msg(fd, &addr, &p, &len, false);

		if (rc == B_OK)
		{
			time_t now = time(NULL);
			char *ts = ctime(&now), *lf = strchr(ts, '\n');
			*lf = 0x00;

			char buffer[65] = { 0 };
			memcpy(buffer, p, len);
			buffer[64] = 0x00;

			printf("%s [%d] %s\n", ts, addr, buffer);

			free(p);
		}
		else if (rc == B_RSSI)
		{
			unsigned char rssi = 123;

			if (read(fd, &rssi, 1) == -1)
				error_exit(true, "problem retrieving rssi from radio");
		}
		else if (rc != B_ERR_TO)
		{
			printf("ERR: %d\n", rc);
		}
	}

	return 0;
}
