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
out:
	slogd(CLOG, "Connect() to coreip ok.\n");
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

    ip = cand->coreip_lst[cand->coreip_ix];
    if (ip == 0 || ip == (uint32_t)0x0100007f)    // 0 & 127.0.0.1
        return VERROR;
    if ((np->sd = connectip(ip, timeout)) == INVALID_SOCKET) {
        slogw(CLOG, "ignore core ip[%02d]\n", cand->coreip_ix);
        cand->coreip_lst[cand->coreip_ix] = 0;
        return VERROR;
    }

    np->src_ip = ip;
    np->id1 = rand16();
    send_op(np, OP_HELLO);
    int ecode = rx2(np, 0, timeout);
    if(ecode != VEOK) {
        sloge(CLOG, "missing HELLO_ACK packet (%d)\n", ecode);
        slogw(CLOG, "ignore core ip[%02d]\n", cand->coreip_ix);
        cand->coreip_lst[cand->coreip_ix] = 0;
bad:
        close(np->sd);
        np->sd = INVALID_SOCKET;
        return VERROR;
    }
    np->id2 = get16(np->tx.id2);
    np->opcode = get16(np->tx.opcode);
    if(np->opcode != OP_HELLO_ACK) {
        sloge(CLOG, "HELLO_ACK is wrong: %d\n", np->opcode);
        slogw(CLOG, "ignore core ip[%02d]\n", cand->coreip_ix);
        cand->coreip_lst[cand->coreip_ix] = 0;
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

#ifdef DEBUG
            FILE *fp;
            if ((fp = fopen("./candidate.tmp", "w+b")) != NULL) {
                fwrite(cand->cand_data, 1, cand->cand_len, fp);
                fclose(fp);
            }
#endif
            // print tailer info
            slogd(CLOG, "Trailer block num: 0x%s\n", bnum2hex(cand->cand_trailer->bnum));
            slogd(CLOG, "Trailer difficulty: %d\n", cand->cand_trailer->difficulty[0]);
            for (int i=0; i<8; i++) {
                slogd(CLOG, "Get trailer phash[%02d]: 0x%08x\n", i, ((int*)cand->cand_trailer->phash)[i]);
            }
            for (int i=0; i<8; i++) {
                slogd(CLOG, "Get trailer mroot[%02d]: 0x%08x\n", i, ((int*)cand->cand_trailer->mroot)[i]);
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
    const double timeout = 10.0;     // 10s timeout
    slogd(CLOG, "\n");
    slogd(CLOG, "Start getting new candidate...\n");

    for (int j=0; j<CORELISTLEN; j++) {
        if (callserver(&node, cand, timeout) != VEOK) {
            (cand->coreip_ix)++;    // switch to next coreip
            if (cand->coreip_ix >= CORELISTLEN)
                cand->coreip_ix = 0;
            continue;
        }

        slogd(CLOG, "get server block num: 0x%s\n", bnum2hex(cand->server_bnum));
        if (cmp64(cand->server_bnum, last_bnum) < 0) {      // cur_sbnum < last_sbnum ?
            slogw(CLOG, "this server doesn't have the latest block, switch a different server.\n");
            close(node.sd);
            (cand->coreip_ix)++;    // switch to next coreip
            if (cand->coreip_ix >= CORELISTLEN)
                cand->coreip_ix = 0;
            continue;
        }
        memcpy(last_bnum, cand->server_bnum, 8);

        put16(node.tx.len, 1);
        slogi(CLOG, "attempting to download candidate block from core ip[%02d]...\n", cand->coreip_ix);
        send_op(&node, OP_GET_CBLOCK);
        if (get_block3(&node, cand, timeout) != 0) {    // 'cand->cand_data' may be destroyed
            close(node.sd);

            slogw(CLOG, "ignore core ip[%02d]\n", cand->coreip_ix);
            cand->coreip_lst[cand->coreip_ix] = 0;

            (cand->coreip_ix)++;    // switch to next coreip
            if (cand->coreip_ix >= CORELISTLEN)
                cand->coreip_ix = 0;
            continue;
        }
        close(node.sd);

        if (cmp64(cand->server_bnum, cand->cand_trailer->bnum) >= 0) {  // sbnum >= tbnum ?
            // donot switch coreip, try again...
            slogw(CLOG, "trailer block num invalid, retry...\n");
            continue;
        }
    
        return VEOK;
    }

    return VERROR;
}

int blocking(SOCKET sd)
{
    u_long arg = 0L;
    return ioctl(sd, FIONBIO, (u_long FAR *) &arg);
}

int send_cand_data(NODE *np, char *data, int len)
{
    TX *tx;
    int n = TRANLEN, status;
    tx = &np->tx;

    blocking(np->sd);
    for (;;) {
        if (n > len)
            n = len;
        memcpy(TRANBUFF(tx), data, n);
        len -= n;
        put16(tx->len, n);
        status = send_op(np, OP_SEND_BL);
        if (n < TRANLEN) {
            return status;
        }
        if (status != VEOK) 
            break;
    }
    return VERROR;
}

int send_mblock(trigg_cand_t *cand)
{
    NODE node;
    int status;
    const double timeout = 10.0;     // 10s timeout

    if (callserver(&node, cand, timeout) != VEOK)
        return VERROR;
    put16(node.tx.len, 1);
    send_op(&node, OP_MBLOCK);
    status = send_cand_data(&node, cand->cand_data, cand->cand_len);
    if (status == VEOK) {
        slogd(CLOG, "send core ip[%d] ok, total size %d bytes\n", cand->coreip_ix, cand->cand_len);
    }

    close(node.sd);
    return status;
}

#ifdef __cplusplus
}
#endif 
