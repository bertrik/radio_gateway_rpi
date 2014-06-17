#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "rgr_lib.h"

int open_ser_dev(const char * const dev)
{
	int fd = open(dev, O_RDWR);
	if (fd == -1)
		error_exit(true, "cannot open %s", dev);

	// code taken from
	// http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
	struct termios tty;
        memset(&tty, 0, sizeof tty);

        if (tcgetattr(fd, &tty) == -1)
                error_exit(true, "error from tcgetattr");

        cfsetospeed (&tty, B115200);
        cfsetispeed (&tty, B115200);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        // tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) == -1)
                error_exit(true, "error from tcsetattr");

	return fd;
}

bool send_msg(int fd, uint8_t addr, const unsigned char *p, const int len)
{
	if (len > MAX_PACKET_SIZE)
		error_exit(false, "data packet too large (%d bytes max): %d", MAX_PACKET_SIZE, len);

	unsigned char out[MAX_PACKET_SIZE * 2 + 4] = { 0 };
	int out_offset = 0;

	out[out_offset++] = B_START;  // start

	out[out_offset++] = B_ADDR;
	out[out_offset++] = addr;

	for(int i=0; i<len; i++) {
		if (p[i] >= B_ESCAPE) { // need to escape
			out[out_offset++] = B_ESCAPE; // escape
			out[out_offset++] = p[i] - B_ESCAPE;
		}
		else {
			out[out_offset++] = p[i];
		}
	}

	out[out_offset++] = B_END; // end

	// printf("out len: %d\n", out_offset);

	const char *psend = (const char *)out;
	int len_send = out_offset;
	while(len_send > 0)
	{
		int rc = write(fd, psend, len_send);

		if (rc == -1)
			error_exit(true, "write to serial port failed");
		if (rc == 0)
			error_exit(false, "serial port closed?");

		psend += rc;
		len_send -= rc;
	}

	return true;
}

int wait_for_byte(int fd, int to)
{
	struct pollfd fds[1];

	fds[0].fd = fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	if (poll(fds, 1, to) == 0)
		return -1;

	unsigned char result = 0;
	if (read(fd, &result, 1) != 1)
		error_exit(true, "failed reading response from serial port");

	return result;
}

int recv_msg(int fd, uint8_t *addr, unsigned char **p, int *len, bool no_b_start)
{
	int rc = B_ERR_UNKNOWN;

	*p = (unsigned char *)malloc(MAX_PACKET_SIZE);
	*len = 0;

	bool first = !no_b_start, isAddress = false;
	bool escape = false;

	for(;;)
	{
		int b = wait_for_byte(fd, 1000);
		if (b == -1)
		{
			rc = B_ERR_TO;
			break;
		}

		// printf("%d %d\n", escape, b);

		if (escape)
		{
			int new_val = b + B_ESCAPE;

			if (new_val > 255)
			{
				rc = B_ERR_OOR;
				break;
			}

			if (*len == MAX_PACKET_SIZE)
			{
				rc = B_ERR_OVERFLOW;
				break;
			}

			(*p)[(*len)++] = new_val;

			escape = false;
		}
		else if (isAddress)
		{
			*addr = b;
			isAddress = false;
		}
		else if (b == B_START)
		{
			if (!first)
			{
				rc = B_ERR_START;
				break;
			}

			first = false;
		}
		else if (b == B_ADDR)
			isAddress = true;
		else if (b == B_END)
		{
			if (first)
			{
				rc = B_ERR_END;
				break;
			}

			rc = B_OK;
			break;
		}
		else if (b == B_ESCAPE)
		{
			escape = true;
		}
		else
		{
			if (*len == MAX_PACKET_SIZE)
			{
				rc = B_ERR_OVERFLOW;
				break;
			}

			(*p)[(*len)++] = b;
		}
	}

	return rc;
}

const char *err_to_msg(int nr)
{
	switch(nr)
	{
		case B_ERR_UNKNOWN:
			return "unknown";
		case B_ERR_TO:
			return "time out";
		case B_ERR_OOR:
			return "escape out of range";
		case B_ERR_START:
			return "unexpected start byte";
		case B_ERR_END:
			return "unexpected end byte";
		case B_ERR_OVERFLOW:
			return "buffer overflow";
		case B_RSSI:
			return "(rssi info)";
	}

	return "?";
}
