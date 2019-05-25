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
#include "../../lib/mux.h"

#define SER_PORT_DEFAULT        "/dev/ttyAMA0"
#define SER_BAUDRATE_DEFAULT    115200

#define SER_SIM_EN              0
#define SER_SIM_CONST_LEN       84

typedef struct {
    int         baud;
    char        dev[128];

    int         const_count;
    int         const_len;
    uint8_t     addr[256];
    uint8_t     id_tbl[256];

    mux_t       lock;
    int         up_count;
    uint8_t     upstream[6];

    uint32_t    version;
    uint32_t    first_hash;
    uint32_t    nonce_done;
    double      done_delay;
    uint32_t    nonce_hit;
    double      hit_delay;
} ser_sim_t;

extern int ser_open(const char* device, int speed);
extern int ser_close(int fd);

extern int ser_send(int fd, const void *data, int len);
extern int ser_receive(int fd, uint8_t *str_dat, int n);

#ifdef __cplusplus
}
#endif

#endif

