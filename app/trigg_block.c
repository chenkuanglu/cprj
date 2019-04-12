// 
// trigg_block.c
//
#include "trigg_block.h"
#include "trigg_util.h"
#include "trigg_rand.h"
#include "trigg_crc16.h"

#ifdef __cplusplus
extern "C" {
#endif 

static uint32_t PORT = 2095;
static uint32_t PORT2 = 2096;

static char last_bnum[8] = {0};

SOCKET connectip(uint32_t ip, double tout)
{
	SOCKET sd;
	struct sockaddr_in addr;
	uint16_t port;
	double timeout;
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		sloge(CLOG, "socket() open fail: %s", strerror(errno));
		return INVALID_SOCKET;
	}
	port = PORT;
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = ip;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	slogd(CLOG, "Trying to connect %s:%d...  ", ntoa((uint8_t *)&ip), port);
	nonblock(sd);
	timeout = monotime() + tout;

retry:
	if (connect(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr))) {
        if (errno == EISCONN) 
            goto out;
        if ( (errno == EINPROGRESS || errno == EALREADY) && (monotime() < timeout) ) 
            goto retry;
		close(sd);
		sloge(CLOG, "connect() to coreip fail.");
		return INVALID_SOCKET;
	}
	slogd(CLOG, "connect() to coreip ok.");
out:
	nonblock(sd);
	return sd;
}

void crctx(TX *tx)
{
    put16(CRC_VAL_PTR(tx), crc16(CRC_BUFF(tx), CRC_COUNT));
}

// send operation
int sendtx2(NODE *np)
{
    int count;
    TX *tx;

    tx = &np->tx;

    put16(tx->version, PVERSION);
    put16(tx->network, TXNETWORK);
    put16(tx->trailer, TXEOT);
    put16(tx->id1, np->id1);
    put16(tx->id2, np->id2);
    crctx(tx);
    errno = 0;
    count = send(np->sd, (const char *)TXBUFF(tx), TXBUFFLEN, 0);
    if (count != TXBUFFLEN) {
        sloge(CLOG, "sendtx2() fail: %s\n", strerror(errno));
        return VERROR;
    }
    return VEOK;
}

int send_op(NODE *np, int opcode)
{
    put16(np->tx.opcode, opcode);
    return sendtx2(np);
}

// receive data after 'connect' & 'send_op'
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

// connect ip & get NODE(send hello)
// callserver() before send 'operation'
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
        sloge(CLOG, "Missing HELLO_ACK packet (%d)", ecode);
bad:
        close(np->sd);
        np->sd = INVALID_SOCKET;
        (cand->coreip_ix)++;    // switch to next coreip
        return VERROR;
    }
    np->id2 = get16(np->tx.id2);
    np->opcode = get16(np->tx.opcode);
    if(np->opcode != OP_HELLO_ACK) {
        sloge(CLOG, "HELLO_ACK is wrong: %d", np->opcode);
        goto bad;
    }
    put64(cand->server_bnum, np->tx.cblock);  // copy blocknum
    return VEOK;
}

// if block num invalid, func retry internally
int trigg_get_cblock(trigg_cand_t *cand, int retry)
{
    NODE node;
    slogd(CLOG, "start getting new candidate...\n");

retry:
    if (callserver(&node, cand, 3) != VEOK)     // 3s timeout
        return VERROR;

    if (cmp64(cand->server_bnum, last_bnum) < 0) {
        slogw(CLOG, "This server doesn't have the latest block. switch a different server.\n");
        close(node.sd);
        (cand->coreip_ix)++;    // switch to next coreip
        if (--retry > 0) {
            slogw(CLOG, "Block num invalid, retry...\n");
            goto retry;
        } else {
            return VERROR;
        }
        memcpy(last_bnum, cand->server_bnum, 8);
    }

    put16(node.tx.len, 1);
    slogw(CLOG, "Attempting to download candidate block from network...\n");
    send_op(&node, OP_GET_CBLOCK);
    if (get_block3(&node, cand) != 0) {
        close(node.sd);
        (cand->coreip_ix)++;    // switch to next coreip
        if (--retry > 0) {
            slogw(CLOG, "Get candidate fail, retry...\n");
            goto retry;
        } else {
            return VERROR;
        }
    }
    close(node.sd);

    // date trailer len
    cand->cand_tm = monotime();
    return VEOK;
}

#ifdef __cplusplus
}
#endif 

