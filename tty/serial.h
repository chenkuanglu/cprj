#ifndef __SERIAL_H__
#define __SERIAL_H__

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define SERIAL_PORT_DEFAULT     "/dev/ttyAMA0"
#define BAUD_RATE_DEFAULT       115200

extern int ser_open(const char* device, int speed);
extern int ser_close(int fd);

extern int ser_send(int fd, const void *data, int len);
extern int ser_receive(int fd, uint8_t *str_dat, int n);

#ifdef __cplusplus
}
#endif

#endif

