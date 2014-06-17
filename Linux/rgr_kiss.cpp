// some parts were taken from the 'ax25-tools' package
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <netax25/axlib.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "rgr_lib.h"

#define FEND	0xc0
#define FESC	0xdb
#define TFEND	0xdc
#define TFESC	0xdd

#define max(x, y) ((x) > (y) ? (x) : (y))

int setifcall(int fd, const char *name)
{
        char call[7] = { 0 };

        if (ax25_aton_entry(name, call) == -1)
                return -1;

        if (ioctl(fd, SIOCSIFHWADDR, call) == -1)
		error_exit(true, "ioctl(SIOCSIFHWADDR) failed");
 
        return 0;
}

void startiface(const char *dev)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		error_exit(true, "Cannot create (dummy) socket");

	struct ifreq ifr;
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_mtu = MAX_PACKET_SIZE;

	if (ioctl(fd, SIOCSIFMTU, &ifr) == -1)
		error_exit(true, "failed setting mtu size for ax25 device");

        if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
		error_exit(true, "failed retrieving current ax25 device settings");

	ifr.ifr_flags &= IFF_NOARP;
	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;
	ifr.ifr_flags |= IFF_BROADCAST;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) == -1)
		error_exit(true, "failed setting ax25 device settings");

	close(fd);
}

void dump_hex(const unsigned char *p, int len)
{
	for(int i=0; i<len; i++)
		printf("%d[%c]%c ", p[i], p[i] > 32 && p[i] < 127 ? p[i] : '.', p[i] >= B_ESCAPE ? '+' : ' ');

	printf("\n");
}

bool recv_mkiss(int fd, unsigned char **p, int *len, bool verbose)
{
	bool first = true, ok = false, escape = false;

	*p = (unsigned char *)malloc(MAX_PACKET_SIZE);
	*len = 0;

	for(;;)
	{
		unsigned char buffer = 0;

		if (read(fd, &buffer, 1) == -1)
			error_exit(true, "failed reading from mkiss device");

		if (escape)
		{
			if (*len == MAX_PACKET_SIZE)
				break;

			if (buffer == TFEND)
				(*p)[(*len)++] = FEND;
			else if (buffer == TFESC)
				(*p)[(*len)++] = FESC;
			else if (verbose)
				printf("unexpected mkiss escape %02x\n", buffer);

			escape = false;
		}
		else if (buffer == FEND)
		{
			if (first)
				first = false;
			else
			{
				ok = true;
				break;
			}
		}
		else if (buffer == FESC)
			escape = true;
		else
		{
			if (*len == MAX_PACKET_SIZE)
				break;

			(*p)[(*len)++] = buffer;
		}
	}

	if (ok)
	{
		int cmd = (*p)[0] & 0x0f;

		if (verbose)
			printf("port: %d\n", ((*p)[0] >> 4) & 0x0f);

		if (cmd == 0x00) // data frame
		{
			(*len)--;
			memcpy(&(*p)[0], &(*p)[1], *len);
		}
		else
		{
			if (verbose)
			{
				if (cmd == 1)
					printf("TX delay: %d\n", (*p)[1] * 10);
				else if (cmd == 2)
					printf("persistance: %d\n", (*p)[1] * 256 - 1);
				else if (cmd == 3)
					printf("slot time: %dms\n", (*p)[1] * 10);
				else if (cmd == 4)
					printf("txtail: %dms\n", (*p)[1] * 10);
				else if (cmd == 5)
					printf("full duplex: %d\n", (*p)[1]);
				else if (cmd == 6)
				{
					printf("set hardware: ");
					dump_hex(&(*p)[1], *len - 1);
				}
				else if (cmd == 15)
				{
					error_exit(false, "kernal asked for shutdown");
				}
			}

			ok = false; // it is ok, we just ignore it
		}
	}

	return ok;
}

void send_mkiss(int fd, int channel, const unsigned char *p, const int len)
{
	int max_len = len * 2 + 1;
	unsigned char *out = (unsigned char *)malloc(max_len);
	int offset = 0;

	out[offset++] = FEND;
	out[offset++] = (channel << 4) | 0x00; // [channel 0][data]

	for(int i=0; i<len; i++)
	{
		if (p[i] == FEND)
		{
			out[offset++] = FESC;
			out[offset++] = TFEND;
		}
		else if (p[i] == FESC)
		{
			out[offset++] = FESC;
			out[offset++] = TFESC;
		}
		else
		{
			out[offset++] = p[i];
		}
	}

	out[offset++] = FEND;

	const unsigned char *tmp = out;
	int out_len = offset;
	while(out_len)
	{
		int rc = write(fd, tmp, out_len);

		if (rc == -1 || rc == 0)
			error_exit(true, "failed writing to mkiss device");

		tmp += rc;
		out_len -= rc;
	}

	free(out);
}

int main(int argc, char *argv[])
{
	const char *dev = "/dev/ttyAMA0", *call = "FH4GOU-1";
	bool verbose = true;

	if (argc == 3)
	{
		dev = argv[1];
		call = argv[2];
	}
	else if (argc != 1)
	{
		error_exit(false, "usage: %s dev call\n", argv[0]);
	}

	int fdradio = open_ser_dev(dev);

	int fdmaster = -1, fdslave = -1;
	if (openpty(&fdmaster, &fdslave, NULL, NULL, NULL) == -1)
		error_exit(true, "openpty failed");

	int disc = N_AX25;
	if (ioctl(fdslave, TIOCSETD, &disc) == -1)
		error_exit(true, "error setting line discipline");

	if (setifcall(fdslave, call) == -1)
		error_exit(false, "cannot set call");

	int v = 4;
	if (ioctl(fdslave, SIOCSIFENCAP, &v) == -1)
		error_exit(true, "failed to set encapsulation");

	char dev_name[64] = { 0 };
	if (ioctl(fdslave, SIOCGIFNAME, dev_name) == -1)
		error_exit(true, "failed retrieving name of ax25 network device name");

	if (verbose)
		printf("AX25 device name: %s\n", dev_name);

	startiface(dev_name);

	for(;;)
	{
		fd_set rfds;

		FD_ZERO(&rfds);
		FD_SET(fdradio, &rfds);
		FD_SET(fdmaster, &rfds);

		if (verbose)
			printf("waiting for packets of any source\n");

		if (select(max(fdradio, fdmaster) + 1, &rfds, NULL, NULL, NULL) == -1)
			error_exit(true, "select failed");

		if (verbose)
			printf("select says there's data\n");

		if (FD_ISSET(fdradio, &rfds))
		{
			unsigned char buffer = 123;

			if (read(fdradio, &buffer, 1) == -1)
				error_exit(true, "problem retrieving data from radio");

			if (buffer == B_START)
			{
				if (verbose)
					printf("recv from radio\n");

				unsigned char *p = NULL;
				int len = 0;
				uint8_t addr = 255;
				int rc = recv_msg(fdradio, &addr, &p, &len, true);

				if (rc == B_OK)
				{
					if (verbose)
					{
						printf("[%d] data (%d): ", addr, len);
						dump_hex(p, len);
					}

					send_mkiss(fdmaster, 0, p, len);
				}
				else if (verbose)
				{
					printf("error from radio: %d (%s)\n", rc, err_to_msg(rc));
				}

				free(p);
			}
			else if (buffer == B_RSSI)
			{
				unsigned char rssi = 123;

				if (read(fdradio, &rssi, 1) == -1)
					error_exit(true, "problem retrieving rssi from radio");
			}
			else
			{
				if (buffer == B_OK)
				{
					if (verbose)
						printf("message transmitted ok by radio\n");
				}
				else if (verbose)
				{
					printf("error from radio: %d (%s)\n", buffer, err_to_msg(buffer));
				}
			}
		}
		else if (FD_ISSET(fdmaster, &rfds))
		{
			if (verbose)
				printf("recv from network\n");

			unsigned char *p = NULL;
			int len = 0;

			if (recv_mkiss(fdmaster, &p, &len, verbose))
			{
				if (verbose)
				{
					printf("data (%d): ", len);
					dump_hex(p, len);
				}

				if (!send_msg(fdradio, 255, p, len))
					error_exit(false, "transmit failed");
			}

			free(p);
		}
	}

	close(fdmaster);
	close(fdslave);
	close(fdradio);

	return 0;
}
