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

#define TXADDRLEN               2208
#define TXAMOUNT                8
#define TXSIGLEN                2144  /* WOTS */

#define SOCKET                  unsigned int   /* Borland 32-bit */
#define INVALID_SOCKET          (SOCKET)(~0)

#define VEOK        0      /* No error                    */
#define VERROR      1      /* General error               */
#define VEBAD       2      /* client was bad              */

/* Protocol Definitions */
#define OP_HELLO          1
#define OP_HELLO_ACK      2
#define OP_GETIPL         6
#define OP_SEND_BL        7
#define OP_RESOLVE        14
#define OP_GET_CBLOCK     15
#define OP_MBLOCK         16

/* Communications Protocol Definitions*/
typedef struct {
    uint8_t version[2];  /* 0x01, 0x00 PVERSION  */
    uint8_t network[2];  /* 0x39, 0x05 TXNETWORK */
    uint8_t id1[2];
    uint8_t id2[2];
    uint8_t opcode[2];
    uint8_t cblock[8];        /* current block num  64-bit */
    uint8_t blocknum[8];      /* block num for I/O in progress */
    uint8_t cblockhash[32];   /* sha-256 hash of our current block */
    uint8_t pblockhash[32];   /* sha-256 hash of our previous block */
    uint8_t weight[32];       /* sum of block difficulties */
    uint8_t len[2];  /* length of data in transaction buffer for I/O op's */
    /* start transaction buffer */
    uint8_t src_addr[TXADDRLEN];
    uint8_t dst_addr[TXADDRLEN];
    uint8_t chg_addr[TXADDRLEN];
    uint8_t send_total[TXAMOUNT];
    uint8_t change_total[TXAMOUNT];
    uint8_t tx_fee[TXAMOUNT];
    uint8_t tx_sig[TXSIGLEN];
    /* end transaction buffer */
    uint8_t crc16[2];
    uint8_t trailer[2];  /* 0xcd, 0xab */
} TX;

/* stripped-down NODE for rx2() and callserver(): */
typedef struct {
    TX tx;  /* transaction buffer */
    uint16_t id1;      /* from tx */
    uint16_t id2;      /* from tx */
    int opcode;      /* from tx */
    uint32_t src_ip;
    SOCKET sd;
    pid_t pid;     /* process id of child -- zero if empty slot */
} NODE;


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
    int         coreip_ix;

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

    int             retry_num;
} trigg_miner_t;

extern trigg_miner_t triggm;

extern int trigg_init(start_info_t *sinfo);
extern int trigg_download_lst(void);

extern uint32_t str2ip(char *addrstr);
extern int trigg_nodesl_to_coreipl(char *nodesl, uint32_t *coreipl);

extern uint32_t srand16(uint32_t x);
extern void srand2(uint32_t x, uint32_t y, uint32_t z);
extern uint32_t rand16(void);


#ifdef __cplusplus
}
#endif

#endif

