/**
 * @file    trigg_miner.h
 * @author  ln
 * @brief   trigg miner
 **/

#ifndef __TRIGG_MINER_H__
#define __TRIGG_MINER_H__

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    log_cb_t    *log;

    char        *url;
    char        *nodes_lst;
    int         nlst_len;

    char        *candidate;
    int         cand_len;

    pthread_t   thr_miner;
    pthread_t   thr_pool;
} trigg_miner_t;

extern trigg_miner_t triggm;

extern int trigg_init(start_info_t *sinfo);
extern int trigg_download_lst(const char *file);

#ifdef __cplusplus
}
#endif

#endif

