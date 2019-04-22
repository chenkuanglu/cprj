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

#define TRIGG_CMD_NOTHING       0
#define TRIGG_CMD_NEW_JOB       1

#define TRIGG_CMD_UP_FRM        2

#define TRIGG_CMD_START         3
#define TRIGG_CMD_STOP          4

#define TRIGG_MAX_MSG_BUF       ( 1024 + sizeof(core_msg_t) )

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

    pthread_t       thr_upstream;

    int             retry_num;

    char            *file_dev;
    int             fd_dev;
    int             chip_num;

    int             started;
} trigg_miner_t;

typedef struct {
    trigg_cand_t    cand;
    tchain_t        chain;

    uint32_t        base;
    uint32_t        end;
    uint32_t        target;
    char            midstate[32];
    char            *ending_msg;
} trigg_work_t;

#define CHIP_MAX_OUTSTANDING    1

#define CHIP_MSG_TIMEOUT        6.0

typedef struct {
    trigg_work_t    work[CHIP_MAX_OUTSTANDING];
    int             work_wri;
    int             work_rdi;
    uint32_t        count_hit;
    uint32_t        count_err;

    uint32_t        version;
    double          hashrate;

    int             msgid_guard;
    double          tout_guard;

    double          last_hashstart;
} chip_info_t;

extern trigg_miner_t triggm;

extern int trigg_init(start_info_t *sinfo);
extern int trigg_download_lst(void);

extern uint32_t str2ip(char *addrstr);
extern int trigg_nodesl_to_coreipl(char *nodesl, uint32_t *coreipl);

#ifdef __cplusplus
}
#endif

#endif

