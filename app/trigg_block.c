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
		sloge(CLOG, "Socket() open fail: %s", strerror(errno));
		return INVALID_SOCKET;
	}
	port = PORT;
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = ip;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	slogd(CLOG, "Trying to connect %s:%d...\n", ntoa((uint8_t *)&ip), port);
	nonblock(sd);
	timeout = monotime() + tout;

retry:
	if (connect(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr))) {
        if (errno == EISCONN) 
            goto out;
        if ( (errno == EINPROGRESS || errno == EALREADY) && (monotime() < timeout) ) 
            goto retry;
		close(sd);
		slogw(CLOG, "Connect() to coreip fail\n");
		return INVALID_SOCKET;
	}
	slogd(CLOG, "Connect() to coreip ok.\n");
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
        sloge(CLOG, "Sendtx2() fail: %s\n", strerror(errno));
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


int get_block3(NODE *np, trigg_cand_t *cand, double timeout)
{
    uint16_t len;
    int ecode;

    if (cand->cand_data != NULL) {
        free(cand->cand_data);
        cand->cand_data = NULL;
        cand->cand_trailer = NULL;
        cand->cand_len = 0;
    }

    for (;;) {
        if ((ecode = rx2(np, 1, timeout)) != VEOK)      // receive candidate data
            goto bad;
        if (get16(np->tx.opcode) != OP_SEND_BL) 
            goto bad;
        len = get16(np->tx.len);
        slogd(CLOG, "Receive a candidate section, size %d\n", len);
        if (len > TRANLEN)
            goto bad;
        if (len) {
            cand->cand_data = (char *)realloc(cand->cand_data, cand->cand_len+len);
            memcpy(cand->cand_data+cand->cand_len, TRANBUFF(&np->tx), len);
            cand->cand_len += len;
        }
        if ( (len < 1) || (len > 0 && len < TRANLEN) ) {
            cand->cand_trailer = (btrailer_t *)(cand->cand_data + (cand->cand_len - sizeof(btrailer_t)));
            cand->cand_tm = monotime();
            slogd(CLOG, "Candidate receive done, total size %d\n", cand->cand_len);
            slogd(CLOG, "Get trailer bnum: 0x%s\n", bnum2hex(cand->cand_trailer->bnum));
            for (int i=0; i<9; i++) {
                slogd(CLOG, "Get trailer phash: 0x%08x\n", ((int*)cand->cand_trailer->phash)[i]);
            }
            return 0;
        }
    }
bad:
    slogw(CLOG, "Get_block3() fail(%d): %s\n", ecode, strerror(errno));
    return -1;
}

// if block num invalid, func retry internally
int trigg_get_cblock(trigg_cand_t *cand, int retry)
{
    NODE node;
    const double timeout = 10.0;     // 3s timeout
    slogd(CLOG, "\n");
    slogd(CLOG, "Start getting new candidate...\n");

retry:
    if (callserver(&node, cand, timeout) != VEOK)
        return VERROR;

    slogd(CLOG, "Get server bnum: 0x%s\n", bnum2hex(cand->server_bnum));
    if (cmp64(cand->server_bnum, last_bnum) < 0) {      // cur_sbnum < last_sbnum ?
        slogw(CLOG, "This server doesn't have the latest block. switch a different server.\n");
        close(node.sd);
        (cand->coreip_ix)++;    // switch to next coreip
        if (--retry > 0) {
            slogw(CLOG, "Server bnum invalid, retry...\n");
            goto retry;
        } else {
            return VERROR;
        }
        memcpy(last_bnum, cand->server_bnum, 8);
    }

    put16(node.tx.len, 1);
    slogd(CLOG, "Attempting to download candidate block from network...\n");
    send_op(&node, OP_GET_CBLOCK);
    if (get_block3(&node, cand, timeout) != 0) {    // 'cand->cand_data' may be destroyed
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

    if (cmp64(cand->server_bnum, cand->cand_trailer->bnum) >= 0) {  // sbnum >= tbnum ?
        if (--retry > 0) {
            slogw(CLOG, "Trailer bnum invalid, retry...\n");
            goto retry;
        } else {
            return VERROR;
        }
    }

    return VEOK;
}

#ifdef __cplusplus
}
#endif 

