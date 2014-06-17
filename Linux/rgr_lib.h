#include <stdint.h>

#define B_START 255
#define B_END 254
#define B_OK 253
#define B_ADDR 252
#define B_RSSI 250
#define B_ERR_UNKNOWN 210
#define B_ERR_TO 211  // time out
#define B_ERR_OOR 212  // escape out of range
#define B_ERR_START 213  // unexpected start byte
#define B_ERR_END 214  // end without start
#define B_ERR_OVERFLOW 215 // buffer overflow
#define B_ESCAPE 200

#define MAX_PACKET_SIZE 66	// according to the datasheet

int open_ser_dev(const char * const dev);
bool send_msg(int fd, uint8_t addr, const unsigned char *p, const int len);
int recv_msg(int fd, uint8_t *addr, unsigned char **p, int *len, bool no_b_start);
const char *err_to_msg(int nr);
