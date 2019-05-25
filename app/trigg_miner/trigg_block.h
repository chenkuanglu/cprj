#ifndef __TRIGG_BLOCK_H__
#define __TRIGG_BLOCK_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "../../lib/log.h"
#include "../../lib/timetick.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define TXADDRLEN               2208
#define TXAMOUNT                8
#define TXSIGLEN                2144  /* WOTS */

#define SOCKET                  unsigned int   /* Borland 32-bit */
#define INVALID_SOCKET          (SOCKET)(~0)

#define PVERSION    3

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

#define TXNETWORK 0x0539
#define TXEOT     0xabcd
#define TXADDRLEN 2208
#define TXAMOUNT  8
#define TXSIGLEN  2144  /* WOTS */
#define HASHLEN 32
#define TXAMOUNT 8
#define TXBUFF(tx)   ((uint8_t *) tx)
#define TXBUFFLEN    ((2*5) + (8*2) + 32 + 32 + 32 + 2 + (TXADDRLEN*3) + (TXAMOUNT*3) + TXSIGLEN + (2+2))
#define TRANBUFF(tx) ((tx)->src_addr)
#define TRANLEN      ( (TXADDRLEN*3) + (TXAMOUNT*3) + TXSIGLEN )
#define CRC_BUFF(tx) TXBUFF(tx)
#define CRC_COUNT   (TXBUFFLEN - (2+2))  /* tx buff less crc and trailer */
#define CRC_VAL_PTR(tx)  ((tx)->crc16)

#define CORELISTLEN      32
#define HASHLEN          32

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
    uint8_t phash[HASHLEN];      /* previous block hash (32) */
    uint8_t bnum[8];                    /* this block number */
    uint8_t mfee[8];                    /* transaction fee */
    uint8_t tcount[4];                  /* transaction count */
    uint8_t time0[4];                   /* to compute next difficulty */
    uint8_t difficulty[4];
    uint8_t mroot[HASHLEN];      /* hash of all TXQENTRY's */
    uint8_t nonce[HASHLEN];
    uint8_t stime[4];                   /* unsigned start time GMT seconds */
    uint8_t bhash[HASHLEN];      /* hash of all block less bhash[] */
} btrailer_t;

/* --------------------- user definition --------------------- */
typedef struct {
    uint32_t    coreip_lst[CORELISTLEN];
    int         coreip_ix;

    char        server_bnum[8];     // callserver()

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

typedef char tchain_t[32 + 256 + 16 + 8];

/* --------------------- user definition end --------------------- */

extern int trigg_get_cblock(trigg_cand_t *cand, int retry);
extern int callserver(NODE *np, trigg_cand_t *cand, double timeout);
extern int rx2(NODE *np, int checkids, double sec);
extern SOCKET connectip(uint32_t ip, double tout);
extern int send_mblock(trigg_cand_t *cand);

#ifdef __cplusplus
}
#endif 

#endif

