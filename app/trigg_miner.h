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

#define TRIGG_CORELIST_LEN      32
#define TRIGG_HASH_LEN          32

/* The block trailer at end of block file */
typedef struct {
    uint8_t phash[TRIGG_HASH_LEN];      /* previous block hash (32) */
    uint8_t bnum[8];                    /* this block number */
    uint8_t mfee[8];                    /* transaction fee */
    uint8_t tcount[4];                  /* transaction count */
    uint8_t time0[4];                   /* to compute next difficulty */
    uint8_t difficulty[4];
    uint8_t mroot[TRIGG_HASH_LEN];      /* hash of all TXQENTRY's */
    uint8_t nonce[TRIGG_HASH_LEN];
    uint8_t stime[4];                   /* unsigned start time GMT seconds */
    uint8_t bhash[TRIGG_HASH_LEN];      /* hash of all block less bhash[] */
} btrailer_t;

typedef struct {
    uint32_t    coreip_lst[TRIGG_CORELIST_LEN];
    char        *cand_data;
    btrailer_t  *cand_trailer;
    int         cand_len;
    double      cand_tm;
} trigg_cand_t;

typedef struct {
    char    *url;
    char    *filename;

    char    *filedata;
    int     len;
} nodes_lst_t;

typedef struct {
    log_cb_t        *log;

    nodes_lst_t     nodes_lst;

    trigg_cand_t    candidate;
    mux_t           cand_lock;

    pthread_t       thr_miner;
    thrq_cb_t       thrq_miner;

    pthread_t       thr_pool;
    thrq_cb_t       thrq_pool;
} trigg_miner_t;

extern trigg_miner_t triggm;

extern int trigg_init(start_info_t *sinfo);
extern int trigg_download_lst(void);

#ifdef __cplusplus
}
#endif

#endif

