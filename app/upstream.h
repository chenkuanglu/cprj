/**
 * @file    upstream.h
 * @author  ln
 * @brief   application init
 **/

#ifndef __UPSTREAM_H__
#define __UPSTREAM_H__

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

// upstream msg 
typedef struct {
    int         id;
    int         reg;
    uint32_t    data;
} chip_msg_t;

// hardware msg packet 
struct protocal_packet {
    uint32_t    data;
    uint8_t     id;
    uint8_t     reg;
} __attribute__((__packed__));
typedef struct protocal_packet chip_pack_t;

#ifdef __cplusplus
}
#endif

#endif

