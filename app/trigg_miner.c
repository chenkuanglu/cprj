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
#include "upstream.h"
#include "cr190.h"

#ifdef __cplusplus
extern "C" {
#endif 

trigg_miner_t triggm;
chip_info_t chip_info[256];

void* thread_trigg_miner(void *arg);
void* thread_trigg_pool(void *arg);
int trigg_coreipl_copy(uint32_t *dst, uint32_t *src);
int trigg_cand_copy(trigg_cand_t *dst, trigg_cand_t *src);
void trigg_solve(trigg_work_t *work);

int trigg_init(start_info_t *sinfo)
{
    memset(&chip_info, 0, sizeof(chip_info));

    memset(&triggm, 0, sizeof(trigg_miner_t));
    triggm.log = CLOG;

    triggm.nodes_lst.url = sinfo->argv[1];
    triggm.file_dev = sinfo->argv[2];
    triggm.nodes_lst.filename = strdup("startnodes.lst");

    thrq_init(&triggm.thrq_miner);
    thrq_set_mpool(&triggm.thrq_miner, 0, 128);
    thrq_init(&triggm.thrq_pool);
    thrq_set_mpool(&triggm.thrq_pool, 0, 128);

    mux_init(&triggm.cand_lock);

    time_t stime;
    time(&stime);
    stime = 0x12345678; // #################
    srand16(stime);
    srand2(stime, 0, 0);
    triggm.retry_num = 32;

    triggm.chip_num = 1;

    if ((triggm.fd_dev = ser_open(triggm.file_dev, 115200)) < 0) {
        sloge(triggm.log, "open '%s' fail: %s\n", triggm.file_dev, strerror(errno));
    }

    if (pthread_create(&triggm.thr_miner, NULL, thread_trigg_miner, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_miner' fail: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&triggm.thr_pool, NULL, thread_trigg_pool, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_pool' fail: %s\n", strerror(errno));
        return -1;
    }

    if (pthread_create(&triggm.thr_upstream, NULL, thread_upstream, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_upstream' fail: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int trigg_gen_work(trigg_work_t *work, trigg_cand_t *cand)
{
    trigg_cand_copy(&(work->cand), cand);

    btrailer_t *bt = work->cand.cand_trailer;
    trigg_solve(work);
    trigg_gen(bt->nonce);
    trigg_expand((uint8_t *)work->chain, bt->nonce);
    trigg_gen((byte *)&work->chain[32 + 256]);

    return 0;
}

#define IMG_VER_GOLDEN      0x10000000UL
#define IMG_VER_ALGO_MASK   0x00ffffffUL

bool is_golden_image(uint32_t ver)
{
    if ((ver & ~IMG_VER_ALGO_MASK) == IMG_VER_GOLDEN) {
        return true;
    }
    return false;
}

int trigg_post_constant(int id, trigg_work_t *work)
{
    char buf[84];

    work->base = 0x00000000;
    work->end  = 0x23c3ffff;
    work->target  = 0x00000000;
    slogd(CLOG, "Post id %03d constant, base=0x%08x, end=0x%08x, target=0x%08x\n", id, work->base, work->end, work->target);

    // midstate
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t *)work->chain, 256);
    int *hp = (int *)ctx.state;
    for (int i = 0; i < 8; i++) {
        slogd(CLOG, "midstate[%02d]: 0x%08x\n", i, hp[i]);
    }
    // ending_msg
    hp = (int *)(((char *)work->chain) + 256);
    for (int i = 0; i < 8; i++) {
        slogd(CLOG, "end_msg[%02d]: 0x%08x\n", i, hp[i]);
    }
    // first hash
    byte hash[32];
    sha256((byte *)work->chain, (32 + 256 + 16 + 8), hash);
    char *hstr= abin2hex(hash, 32);
    slogd(CLOG, "First hash: %s\n", hstr);
    free(hstr);

    // make constant
    memcpy(buf, &work->base, 4);
    memcpy(buf+4, &work->end, 4);
    memcpy(buf+4+4, ctx.state, 32);
    memcpy(buf+4+4+32, ((char *)work->chain) + 256, 32);
    memcpy(buf+4+4+32+32, &work->target, 4);
    memcpy(buf+4+4+32+32+4, work->cand.cand_trailer->bnum, 8);

    cr190_write_l(triggm.fd_dev, id, 0x40, buf, 84);
    return 0;
}

int trigg_start(void)
{
    for (int i=0; i<triggm.chip_num; i++) {
        cr190_write(triggm.fd_dev, i+1, 0x20, 0x00000100);
        cr190_read(triggm.fd_dev, i+1, 0x24);
        cr190_read(triggm.fd_dev, i+1, 0x0c);
    }

    return 0;
}

int trigg_upstream_proc(trigg_cand_t *cand, upstream_t *msg)
{
    int i, id;
    trigg_work_t *work;
    slogd(CLOG, "UP MSG: %03d,0x%02x,0x%08x\n", msg->id, msg->addr, msg->data);

    id = msg->id;
    switch (msg->addr) {
        case 0x0c:
            chip_info[id-1].work_rdi = 0;
            chip_info[id-1].work_wri = 0;
            for (i=0; i<CHIP_MAX_OUTSTANDING; i++) {
                if (is_golden_image(chip_info[id-1].version)) {
                    continue;
                }
                work = &chip_info[id-1].work[i];
                trigg_gen_work(work, cand);
                trigg_post_constant(id, work);

                chip_info[id-1].work_wri += 1;
                if (chip_info[id-1].work_wri >= CHIP_MAX_OUTSTANDING) {
                    chip_info[id-1].work_wri = 0;
                }
            }
            //set 0x64 timeout
            break;

        case 0x24:
            if (chip_info[id-1].version == 0) {
                chip_info[id-1].version = msg->data;
                slogd(CLOG, "Get chip %03d version 0x%08x\n", id, msg->data);
            }
            break;

        case 0x64:
            //check 1st hash
            //set 0x68 timeout
            break;

        case 0x68:
            work = &chip_info[id-1].work[chip_info[id-1].work_wri];
            trigg_gen_work(work, cand);
            trigg_post_constant(id, work);

            chip_info[id-1].work_wri += 1;
            if (chip_info[id-1].work_wri >= CHIP_MAX_OUTSTANDING) {
                chip_info[id-1].work_wri = 0;
            }

            chip_info[id-1].work_rdi += 1;
            if (chip_info[id-1].work_rdi >= CHIP_MAX_OUTSTANDING) {
                chip_info[id-1].work_rdi = 0;
            }
            break;

        case 0x69:
            break;
            
        case 0x6a:
            break;

        default:
            if (msg->addr >= 0x70 && msg->addr <= 0x7f) {
                // check hash
            }
            break;
    }

    return 0;
}

void* thread_trigg_miner(void *arg)
{
    char ebuf[128];
    char msg_buf[TRIGG_MAX_MSG_BUF];

    trigg_cand_t cand_mining;
    memset(&cand_mining, 0, sizeof(trigg_cand_t));

    core_add_guard(&triggm.thrq_miner);

    core_msg_t *pmsg = (core_msg_t *)msg_buf;
    upstream_t *upstream = (upstream_t *)pmsg->data;

    for (;;) {
        pmsg->cmd = TRIGG_CMD_NOTHING;
        if (thrq_receive(&triggm.thrq_miner, msg_buf, TRIGG_MAX_MSG_BUF, 0) < 0) {
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (pmsg->cmd) {
            case TRIGG_CMD_NOTHING:
                break;
            case TRIGG_CMD_NEW_JOB:
                mux_lock(&triggm.cand_lock);
                if (cand_mining.cand_tm < triggm.candidate.cand_tm) {
                    trigg_cand_copy(&cand_mining, &triggm.candidate);
                    slogi(triggm.log, CCL_CYAN "New block 0x%x received\n" CCL_END, *((int32_t*)(cand_mining.cand_trailer->bnum)));
                }
                mux_unlock(&triggm.cand_lock);
                if (!triggm.started) {
                    triggm.started = 1;
                    trigg_start();
                }
                break;
            case TRIGG_CMD_UP_FRM:
                trigg_upstream_proc(&cand_mining, upstream);
                break;
            case CORE_MSG_CMD_EXPIRE:
                logw("CORE_MSG_CMD_EXPIRE...\n");
                break;
            default:
                break;
        }

        //if (cand_mining.cand_trailer) {
        //    slogd(CLOG, "\n");
        //    long long tgt = trigg_diff_val(cand_mining.cand_trailer->difficulty[0]);
        //    slogd(CLOG, "diff = %d, target = 0x%016llx\n", cand_mining.cand_trailer->difficulty[0], tgt);

        //    // gen one work
        //    trigg_solve(cand_mining.cand_trailer, cand_mining.cand_trailer->difficulty[0], cand_mining.cand_trailer->bnum);
        //    trigg_gen(cand_mining.cand_trailer->nonce);
        //    trigg_expand((uint8_t *)triggm.chain, cand_mining.cand_trailer->nonce);
        //    trigg_gen((byte *)&triggm.chain[32 + 256]);


        //    nsleep(3);
        //} else {
        //    continue;
        //}
    }
}

void* thread_trigg_pool(void *arg)
{
#define TRAILER_DEBUG
    char ebuf[128];
    char msg_buf[TRIGG_MAX_MSG_BUF];
    const double QUE_TIMEOUT = 1.0;     // 1s
    double tm_nlst = 0;
    double tm_cand = 0;

    trigg_cand_t cand;
    memset(&cand, 0, sizeof(cand));
    
    core_msg_t *pmsg = (core_msg_t *)msg_buf;
    core_msg_t msgx;

    for (;;) {
        pmsg->cmd = TRIGG_CMD_NOTHING;
        if (thrq_receive(&triggm.thrq_pool, msg_buf, TRIGG_MAX_MSG_BUF, QUE_TIMEOUT) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (pmsg->cmd) {
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
                    core_proper_exit(errno);
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
#ifndef TRAILER_DEBUG
            int retry = triggm.retry_num;
            while (trigg_get_cblock(&cand, CORELISTLEN) != 0) {     // 'cand->cand_data' may be destroyed
                if (--retry <= 0)
                    core_proper_exit(errno);
            }
            tm_cand = monotime();

            mux_lock(&triggm.cand_lock);
            trigg_cand_copy(&triggm.candidate, &cand);
            mux_unlock(&triggm.cand_lock);
#else
            tm_cand = monotime();
            triggm.candidate.cand_data = (char *)realloc(triggm.candidate.cand_data, 1024);
            triggm.candidate.cand_len = 1024;
            triggm.candidate.cand_tm = monotime();
            triggm.candidate.cand_trailer = (btrailer_t*)(triggm.candidate.cand_data + (triggm.candidate.cand_len - sizeof(btrailer_t)));

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

            msgx.type = 0;
            msgx.cmd = TRIGG_CMD_NEW_JOB;
            msgx.tm = monotime();
            msgx.len = 0;
            thrq_send(&triggm.thrq_miner, &msgx, sizeof(msgx)+msgx.len);
        }
    }
}

void trigg_solve(trigg_work_t *work)
{
    btrailer_t *bt = work->cand.cand_trailer;

    memset(work->chain + 32, 0, (256 + 16));
    memcpy(work->chain, bt->mroot, 32);
    memcpy(work->chain + 32 + 256 + 16, bt->bnum, 8);

    put16(bt->nonce + 0, rand16());                 // tainler's nonceL[16] (low nonce) as xnonce
    put16(bt->nonce + 2, rand16());
    put16(work->chain + (32 + 256), rand16());      // tchain's nonceH[16] (high nonce) for hardware calc
    put16(work->chain + (32 + 258), rand16());

    slogd(CLOG, "Generate work, trailer nonceL 0x%08x, tchain nonceH 0x%08x\n", 
                    *((int *)(bt->nonce)), *((int *)(&(work->chain[32 + 256]))));
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
    memcpy(dst->cand_data, src->cand_data, src->cand_len);
    memcpy(dst->server_bnum, src->server_bnum, 8);
    dst->coreip_ix = src->coreip_ix;
    dst->cand_len = src->cand_len;
    dst->cand_trailer = (btrailer_t *)(dst->cand_data + (dst->cand_len - sizeof(btrailer_t)));
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

