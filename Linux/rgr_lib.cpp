#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "error.h"
#include "rgr_lib.h"

bool send_msg(int fd, const unsigned char *p, const int len)
{
	if (len > MAX_PACKET_SIZE)
		error_exit(false, "data packet too large (%d bytes max): %d", MAX_PACKET_SIZE, len);

	unsigned char out[MAX_PACKET_SIZE * 2 + 4] = { 0 };
	int out_offset = 0;

	out[out_offset++] = B_START;  // start

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

int recv_msg(int fd, unsigned char **p, int *len, bool no_b_start)
{
	int rc = B_ERR_UNKNOWN;

	*p = (unsigned char *)malloc(MAX_PACKET_SIZE);
	*len = 0;

	bool first = !no_b_start;
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
		else if (b == B_START)
		{
			if (!first)
			{
				rc = B_ERR_START;
				break;
			}

			first = false;
		}
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
	}

	return "?";
}
