#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "error.h"

#define B_START 255
#define B_END 254
#define B_OK 253
#define B_ERR_UNKNOWN 210
#define B_ERR_TO 211  // time out
#define B_ERR_OOR 212  // escape out of range
#define B_ERR_START 213  // unexpected start byte
#define B_ERR_END 214  // end without start
#define B_ERR_OVERFLOW 215 // buffer overflow
#define B_ESCAPE 200

bool send_msg(int fd, const char *p, const int len)
{
	if (len > 64)
		error_exit(false, "data packet too large (64 bytes max): %d", len);

	uint8_t out[64 * 2 + 4] = { 0 };
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

int recv_msg(int fd, char **p, int *len, bool no_b_start)
{
	int rc = B_ERR_UNKNOWN;

	*p = (char *)malloc(64);
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

		if (escape)
		{
			int new_val = b + B_ESCAPE;

			if (new_val > 255)
			{
				rc = B_ERR_OOR;
				break;
			}

			if (*len == 64)
			{
				rc = B_ERR_OVERFLOW;
				break;
			}

			(*p)[(*len)++] = new_val;

			escape = false;
		}
		else if (b == B_OK || (b >= 210 && b < 225))
		{
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
		else if (b >= B_ESCAPE)
		{
			escape = true;
		}
		else
		{
			if (*len == 64)
			{
				rc = B_ERR_OVERFLOW;
				break;
			}

			(*p)[(*len)++] = b;
		}
	}

	return rc;
}
