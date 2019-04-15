/**
 * @file    trigg_miner.c
 * @author  ln
 * @brief   trigg miner
 **/

#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "trigg_miner.h"
#include "trigg_util.h"
#include "trigg_crypto.h"
#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif 

trigg_miner_t triggm;

void* thread_trigg_miner(void *arg);
void* thread_trigg_pool(void *arg);
int trigg_coreipl_copy(uint32_t *dst, uint32_t *src);
int trigg_cand_copy(trigg_cand_t *dst, trigg_cand_t *src);
void trigg_solve(btrailer_t *bt, int diff, uint8_t *bnum);

int trigg_init(start_info_t *sinfo)
{
    memset(&triggm, 0, sizeof(trigg_miner_t));
    triggm.log = CLOG;

    triggm.nodes_lst.url = sinfo->argv[1];
    triggm.nodes_lst.filename = strdup("startnodes.lst");

    thrq_init(&triggm.thrq_miner);
    thrq_set_mpool(&triggm.thrq_miner, 0, 32);
    thrq_init(&triggm.thrq_pool);
    thrq_set_mpool(&triggm.thrq_pool, 0, 32);

    mux_init(&triggm.cand_lock);

    time_t stime;
    time(&stime);
    srand16(stime);
    srand2(stime, 0, 0);
    triggm.retry_num = 5;

    if (pthread_create(&triggm.thr_miner, NULL, thread_trigg_miner, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_miner' fail: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&triggm.thr_pool, NULL, thread_trigg_pool, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_pool' fail: %s\n", strerror(errno));
        return -1;
    }

    //trigg_start();

    return 0;
}

void* thread_trigg_miner(void *arg)
{
#define TRIGG_CPU_MINER

    (void)arg;
    char ebuf[128];
    double guard_period = 1.0;

    trigg_cand_t cand_mining;
    memset(&cand_mining, 0, sizeof(trigg_cand_t));

    int cmd;
    for (;;) {
        cmd = TRIGG_CMD_NOTHING;
        if (thrq_receive(&triggm.thrq_miner, &cmd, sizeof(cmd), guard_period) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (cmd) {
            case TRIGG_CMD_NOTHING:
                break;
            case TRIGG_CMD_NEW_JOB:
                mux_lock(&triggm.cand_lock);
                if (cand_mining.cand_tm < triggm.candidate.cand_tm) {
                    trigg_cand_copy(&cand_mining, &triggm.candidate);
                    slogd(triggm.log, "New job received\n");
                }
                mux_unlock(&triggm.cand_lock);
                // need clean chips?
                break;
            case TRIGG_CMD_UP_FRM:
                break;
            case TRIGG_CMD_GUARD:
                break;
            default:
                break;
        }

        if (cand_mining.cand_trailer) {
            slogd(CLOG, "\n");
            long long tgt = trigg_diff_val(cand_mining.cand_trailer->difficulty[0]);
            slogd(CLOG, "diff = %d, target = 0x%016llx\n", cand_mining.cand_trailer->difficulty[0], tgt);

            // gen one work
            trigg_solve(cand_mining.cand_trailer, cand_mining.cand_trailer->difficulty[0], cand_mining.cand_trailer->bnum);
            trigg_gen(cand_mining.cand_trailer->nonce);
            trigg_expand((uint8_t *)triggm.chain, cand_mining.cand_trailer->nonce);
            trigg_gen((byte *)&triggm.chain[32 + 256]);

            // first hash
            byte hash[32];
            sha256((byte *)triggm.chain, (32 + 256 + 16 + 8), hash);
            char *hstr= abin2hex(hash, 32);
            slogd(CLOG, "First hash: %s\n", hstr);
            free(hstr);

            // midstate
            SHA256_CTX ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, (uint8_t *)triggm.chain, 256);
            int *hp = (int *)ctx.state;
            for (int i = 0; i < 8; i++) {
                slogd(CLOG, "midstate[%02d]: 0x%08x\n", i, hp[i]);
            }

            // end msg
            hp = (int *)&triggm.chain[256];
            for (int i = 0; i < 8; i++) {
                slogd(CLOG, "end_msg[%02d]: 0x%08x\n", i, hp[i]);
            }

            nsleep(3);
        } else {
            continue;
        }
    }
}

void* thread_trigg_pool(void *arg)
{
//#define TRAILER_DEBUG

    (void)arg;
    char ebuf[128];
    double que_tout = 1.0;
    double tm_nlst = 0;
    double tm_cand = 0;
    trigg_cand_t cand;
    memset(&cand, 0, sizeof(cand));
    
    int cmd;
    for (;;) {
        cmd = TRIGG_CMD_NOTHING;
        if (thrq_receive(&triggm.thrq_pool, &cmd, sizeof(cmd), que_tout) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (cmd) {
            case TRIGG_CMD_NOTHING:
                break;
            default:
                break;
        }

        // update nodes list every 1h
        if (tm_nlst == 0 || monotime() - tm_nlst > 3600) {
            int retry = triggm.retry_num;
            while (trigg_download_lst() != 0) {
                if (--retry <= 0)
                    process_proper_exit(errno);
            }
            tm_nlst = monotime();

            cand.coreip_ix = 0;
            memset(cand.coreip_lst, 0, sizeof(cand.coreip_lst));
            trigg_nodesl_to_coreipl(triggm.nodes_lst.filedata, cand.coreip_lst);
            trigg_coreip_shuffle(cand.coreip_lst, CORELISTLEN);
            for (int i=0; i<CORELISTLEN; i++) {
                if (cand.coreip_lst[i] != 0)
                    slogd(triggm.log, "Shuffled IPs[%02d] 0x%08x\n", i, cand.coreip_lst[i]);
            }
        }

        // get new job from pool every 600s
        if (tm_cand == 0 || monotime() - tm_cand > 600) {
            int retry = triggm.retry_num;
            while (trigg_get_cblock(&cand, CORELISTLEN) != 0) {     // 'cand->cand_data' may be destroyed
                if (--retry <= 0)
                    process_proper_exit(errno);
            }
            tm_cand = monotime();

            mux_lock(&triggm.cand_lock);
            trigg_cand_copy(&triggm.candidate, &cand);
            mux_unlock(&triggm.cand_lock);

#ifdef TRAILER_DEBUG
            static btrailer_t bt;
            FILE *fp = fopen("./candidate", "rb");
            if (fp == NULL) {
                sloge(CLOG, "Open file ./candidate fail\n");
            } else {
                fseek(fp, -(sizeof(btrailer_t)), SEEK_END);
                fread(&bt, 1, sizeof(btrailer_t), fp);
            }
            fclose(fp);

            mux_lock(&triggm.cand_lock);
            memcpy(triggm.candidate.cand_trailer, &bt, sizeof(btrailer_t));
            triggm.candidate.cand_trailer->difficulty[0] = 15;  // set diff
            mux_unlock(&triggm.cand_lock);

            // print tailer info
            slogd(CLOG, "Fixed trailer debug:\n");
            slogd(CLOG, "Trailer block num: 0x%s\n", bnum2hex(triggm.candidate.cand_trailer->bnum));
            slogd(CLOG, "Trailer difficulty: %d\n", triggm.candidate.cand_trailer->difficulty[0]);
            for (int i=0; i<8; i++) {
                slogd(CLOG, "Get trailer phash[%02d]: 0x%08x\n", i, ((int*)triggm.candidate.cand_trailer->phash)[i]);
            }
            for (int i=0; i<8; i++) {
                slogd(CLOG, "Get trailer mroot[%02d]: 0x%08x\n", i, ((int*)triggm.candidate.cand_trailer->mroot)[i]);
            }
#endif

            cmd = TRIGG_CMD_NEW_JOB;
            thrq_send(&triggm.thrq_miner, &cmd, sizeof(cmd));
        }
    }
}

void trigg_solve(btrailer_t *bt, int diff, uint8_t *bnum)
{
    triggm.diff = diff;
    memset(triggm.chain + 32, 0, (256 + 16));
    memcpy(triggm.chain, bt->mroot, 32);
    memcpy(triggm.chain + 32 + 256 + 16, bnum, 8);
    put16(bt->nonce + 0, rand16());                 // tainler's nonceL[16] (low nonce) as xnonce
    put16(bt->nonce + 2, rand16());
    put16(triggm.chain + (32 + 256), rand16());     // tchain's nonceH[16] (high nonce) for hardware calc
    put16(triggm.chain + (32 + 258), rand16());
#ifdef DEBUG
    int *p = (int*)bt->nonce;
    slogd(CLOG, "tailer nonce low: 0x%08x\n", p[0]);
    p = (int*)&(triggm.chain[32 + 256]);
    slogd(CLOG, "tchain nonce high: 0x%08x\n", p[0]);
#endif
}

int trigg_coreipl_copy(uint32_t *dst, uint32_t *src)
{
    if (dst == NULL || src == NULL)
        return -1;
    for (int i=0; i<CORELISTLEN; i++) {
        dst[i] = src[i];
    }
    return 0;
}

int trigg_cand_copy(trigg_cand_t *dst, trigg_cand_t *src)
{
    if (trigg_coreipl_copy(dst->coreip_lst, src->coreip_lst) != 0)
        return -1;
    if (dst->cand_data)
        free(dst->cand_data);
    if ((dst->cand_data = (char *)malloc(src->cand_len)) == NULL) 
        return -1;
    memcpy(dst->cand_data, src->cand_data, dst->cand_len);
    memcpy(dst->server_bnum, src->server_bnum, 8);
    dst->coreip_ix = src->coreip_ix;
    dst->cand_trailer = src->cand_trailer;
    dst->cand_len = src->cand_len;
    dst->cand_tm = src->cand_tm;
    return 0;
}

// convert nodes lst to core ip lst
int trigg_nodesl_to_coreipl(char *nodesl, uint32_t *coreipl)
{
    int j;
    char *addrstr, *line;
    uint32_t ip;

    if (nodesl == NULL || *nodesl == '\0') 
        return -1;

    char *str = strdup(nodesl);
    if (str == NULL)
        return -1;
    char *sep = str;
    for (j=0; j<CORELISTLEN; ) {
        if ((line = strsep(&sep, "\n")) == NULL) {
            break;
        }
        if (*line == '#') 
            continue;
        if ((addrstr = strtok(line, " \r\n\t")) == NULL) {
            break;
        }
        if ((ip = str2ip(addrstr)) == 0)
            continue;
        slogd(triggm.log, "Convert string '%s' \tto IPs[%02d] 0x%08x\n", addrstr, j, ip);
        coreipl[j++] = ip;
    }

    free(str);
    return j;
}

// curl callback function for download nodes list
static size_t trigg_curl_write_nodes(void *src, size_t size, size_t nmemb, void *dst)
{
    (void)dst;
    int len = size*nmemb;
    if (triggm.nodes_lst.filedata == NULL) {
        triggm.nodes_lst.filedata = (char *)malloc(len+1);
    } else {
        triggm.nodes_lst.filedata = (char *)realloc(triggm.nodes_lst.filedata, triggm.nodes_lst.len+len+1);
    }
    memcpy((char*)triggm.nodes_lst.filedata + triggm.nodes_lst.len, src, len);
    triggm.nodes_lst.len += len;
    triggm.nodes_lst.filedata[triggm.nodes_lst.len] = '\0';
    slogd(triggm.log, "download %d bytes from url '%s'\n", triggm.nodes_lst.len, triggm.nodes_lst.url);

    return len;
}

// download nodes list from url
int trigg_download_lst(void)
{
    char errbuf[128];
    triggm.nodes_lst.len = 0;

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, triggm.nodes_lst.url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, trigg_curl_write_nodes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, triggm.nodes_lst.filedata);
    CURLcode res = curl_easy_perform(curl);
    if (CURLE_OK != res) {
        errno = res;
        sloge(triggm.log, "curl_easy_perform() fail: %s\n", err_string(res, errbuf, 128));
        return -1;
    }
    curl_easy_cleanup(curl);

    slogd(triggm.log, "peer IPs from url '%s':\n%s\n", triggm.nodes_lst.url, triggm.nodes_lst.filedata);
    FILE *fp = fopen(triggm.nodes_lst.filename, "w+");
    if (fp) {
        slogd(triggm.log, "write nodes list to file '%s'\n", triggm.nodes_lst.filename);
        fwrite(triggm.nodes_lst.filedata, 1, triggm.nodes_lst.len, fp);
        fclose(fp);
    } else {
        sloge(triggm.log, "fopen() fail: %s\n", err_string(errno, errbuf, 128));
    }
    return 0;
}

#ifdef __cplusplus
}
#endif 

