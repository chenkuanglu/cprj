//
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "trigg_miner.h"

#ifdef __cplusplus
extern "C" {
#endif 

static uint8_t sigint = 0;

SOCKET connectip(uint32_t ip)
{
    return 0;
}

int send_op(NODE *np, int opcode)
{
    put16(np->tx.opcode, opcode);
    return sendtx2(np);
}

int callserver(NODE *np, trigg_cand_t *cand)
{
    int ecode, j;
    uint32_t ip;

    memset(np, 0, sizeof(NODE));
    np->sd = INVALID_SOCKET;

    for (j=0; j<TRIGG_CORELIST_LEN && sigint == 0; j++,(cand->coreip_ix)++) {
        if (cand->coreip_ix >= TRIGG_CORELIST_LEN) 
            cand->coreip_ix = 0;
        ip = cand->coreip_lst[cand->coreip_ix];
        if (ip == 0)
            continue;
        np->sd = connectip(ip);
        if (np->sd != INVALID_SOCKET) 
            break;
    }

    if(np->sd == INVALID_SOCKET) 
        return VERROR;
    np->src_ip = ip;
    np->id1 = rand16();
    if(send_op(np, OP_HELLO) != VEOK) goto bad;

    ecode = rx2(np, 0, ACK_TIMEOUT);
    if(ecode != VEOK) {
        if(Trace) plog("   *** missing HELLO_ACK packet (%d)", ecode);
bad:
        closesocket(np->sd);
        np->sd = INVALID_SOCKET;
        return VERROR;
    }
    np->id2 = get16(np->tx.id2);
    np->opcode = get16(np->tx.opcode);
    if(np->opcode != OP_HELLO_ACK || get16(np->tx.id1) != np->id1) {
      printf("   *** HELLO_ACK is wrong: %d", np->opcode);
      pinklist(ip);   /* protocol violator! */
      epinklist(ip);
      goto bad;
   }
   return VEOK;
}  /* end callserver() */

int trigg_get_cblock(trigg_cand_t *cand)
{
    slogd(triggm.log, "get new candidate...\n");
    // date trailer len
    cand->cand_tm = monotime();
    return 0;
}


#ifdef __cplusplus
}
#endif 

