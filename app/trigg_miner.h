/**
 * @file    trigg_miner.h
 * @author  ln
 * @brief   trigg miner
 **/

#ifndef __TRIGG_MINER_H__
#define __TRIGG_MINER_H__

#include "../core/core.h"
#include "trigg_block.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRIGG_CMD_NOTHING       0x00000000
#define TRIGG_CMD_NEW_JOB       0x00000001

// miner.h
#define TRIGG_CMD_UP_FRM        0x00000002
#define TRIGG_CMD_GUARD         0x00000003
#define TRIGG_CMD_START         0x00000004
#define TRIGG_CMD_STOP          0x00000005

typedef struct {
    log_cb_t        *log;

    nodes_lst_t     nodes_lst;

    trigg_cand_t    candidate;
    mux_t           cand_lock;

    tchain_t        chain;
    int             diff;

    pthread_t       thr_miner;
    thrq_cb_t       thrq_miner;

    pthread_t       thr_pool;
    thrq_cb_t       thrq_pool;

    int             retry_num;
} trigg_miner_t;

extern trigg_miner_t triggm;

extern int trigg_init(start_info_t *sinfo);
extern int trigg_download_lst(void);

extern uint32_t str2ip(char *addrstr);
extern int trigg_nodesl_to_coreipl(char *nodesl, uint32_t *coreipl);

#ifdef __cplusplus
}
#endif

#endif

