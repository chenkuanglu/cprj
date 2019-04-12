#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "trigg_block.h"
#include "trigg_util.h"
#include "trigg_rand.h"
#include "trigg_crc16.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define FAR
static uint32_t PORT = 2095;

int nonblock(SOCKET sd)                                                                                                           
{
    unsigned long arg = 1L;
    return ioctl(sd, FIONBIO, (unsigned long FAR *) &arg);
}

SOCKET connectip(uint32_t ip, double tout)
{
	SOCKET sd;
	struct sockaddr_in addr;
	uint16_t port;
	double timeout;
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		//loge("socket() open fail: %s", strerror(errno));
		return INVALID_SOCKET;
	}
	port = PORT;
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = ip;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	slogd("Trying to connect %s:%d...  ", ntoa((uint8_t *)&ip), port);
	nonblock(sd);
	timeout = monotime() + tout;

retry:
	if (connect(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr))) {
        if (errno == EISCONN) 
            goto out;
        if ( (errno == EINPROGRESS || errno == EALREADY) && (monotime() < timeout) ) 
            goto retry;
		close(sd);
		slogd(triggm.log, "connect() to coreip fail.");
		return INVALID_SOCKET;
	}
	slogd(triggm.log, "connect() to coreip ok.");
out:
	nonblock(sd);
	return sd;
}

int send_op(NODE *np, int opcode)
{
    put16(np->tx.opcode, opcode);
    return sendtx2(np);
}

int rx2(NODE *np, int checkids, double sec)
{
	int count, n;
	double timeout;
	TX *tx;

	tx = &np->tx;
	timeout = monotime() + sec;

	for (n = 0; ; ) {
		count = recv(np->sd, (char *)TXBUFF(tx) + n, TXBUFFLEN - n, 0);
        pthread_testcancel();
		if (count == 0) 
            return VERROR;
		if (count < 0) {
			if (monotime() >= timeout) 
                return -1;
			continue;
		}
		n += count;
		if (n == TXBUFFLEN) 
            break;
	}
	if (get16(tx->network) != TXNETWORK)
		return 2;
	if (get16(tx->trailer) != TXEOT)
		return 3;
	if (crc16(CRC_BUFF(tx), CRC_COUNT) != get16(tx->crc16))
		return 4;
	if (checkids && (np->id1 != get16(tx->id1) || np->id2 != get16(tx->id2)))
		return 5;
	return VEOK;
}

// connect & get NODE(send hello)
int callserver(NODE *np, trigg_cand_t *cand, double timeout)
{
    if (np == NULL || cand == NULL)
        return VERROR;

    uint32_t ip;
    memset(np, 0, sizeof(NODE));
    np->sd = INVALID_SOCKET;

    for (int j=0; j<CORELISTLEN; j++,(cand->coreip_ix)++) {
        pthread_testcancel();
        if (cand->coreip_ix >= CORELISTLEN) 
            cand->coreip_ix = 0;
        if ((ip = cand->coreip_lst[cand->coreip_ix]) == 0)
            continue;
        if ((np->sd = connectip(ip, timeout)) != INVALID_SOCKET)
            break;      // connect ip ok
    }
    if(np->sd == INVALID_SOCKET) 
        return VERROR;  // no ip connect ok

    np->src_ip = ip;
    np->id1 = rand16();
    send_op(np, OP_HELLO);
    int ecode = rx2(np, 0, timeout);
    if(ecode != VEOK) {
        //loge("Missing HELLO_ACK packet (%d)", ecode);
bad:
        close(np->sd);
        np->sd = INVALID_SOCKET;
        (cand->coreip_ix)++;    // switch to next coreip
        return VERROR;
    }
    np->id2 = get16(np->tx.id2);
    np->opcode = get16(np->tx.opcode);
    if(np->opcode != OP_HELLO_ACK) {
        //loge("HELLO_ACK is wrong: %d", np->opcode);
        goto bad;
    }
    put64(cand->cand_bnum, np->tx.cblock);  // copy blocknum
    return VEOK;
}

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

