/**
 * @file    upstream.h
 * @author  ln
 * @brief   application init
 **/

#ifndef __UPSTREAM_H__
#define __UPSTREAM_H__

#include "../../core/core.h"
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

// upstream msg 
typedef struct {
    int         id;
    int         addr;
    uint32_t    data;
} upstream_t;

// hardware msg packet 
struct __up_frame {
    uint32_t    data;
    uint8_t     id;
    uint8_t     addr;
} __attribute__((__packed__));
typedef struct __up_frame up_frame_t;

extern void* thread_upstream(void *arg);

#ifdef __cplusplus
}
#endif

#endif

