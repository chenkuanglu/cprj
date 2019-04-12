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

#ifdef __cplusplus
extern "C" {
#endif 

trigg_miner_t triggm;

void* thread_trigg_miner(void *arg);
void* thread_trigg_pool(void *arg);

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

    time_t stime = 0x12345678;
    //time(&stime);
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
    (void)arg;
    char ebuf[128];

    //cur_iptbl[];
    //if new, update tbl

    int cmd;
    for (;;) {
        cmd = 0;
        if (thrq_receive(&triggm.thrq_miner, &cmd, sizeof(cmd), 1.0) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }
    }
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
    dst->coreip_ix = src->coreip_ix;
    if (dst->cand_data)
        free(dst->cand_data);
    if ((dst->cand_data = (char *)malloc(src->cand_len)) == NULL) 
        return -1;
    memcpy(dst->cand_data, src->cand_data, dst->cand_len);
    dst->cand_trailer = src->cand_trailer;
    dst->cand_len = src->cand_len;
    dst->cand_tm = src->cand_tm;
    return 0;
}

void trigg_coreip_shuffle(uint32_t *list, uint32_t len)
{
    uint32_t *ptr, *p2, temp, ix;

    if (len < 2) 
        return;
    for (ptr = &list[len - 1]; len > 1; len--, ptr--) {
        ix = rand16() % len;
        p2 = &list[ix];
        temp = *ptr;
        *ptr = *p2;
        *p2 = temp;
    }
}

void* thread_trigg_pool(void *arg)
{
    (void)arg;
    char ebuf[128];
    double que_tout = 1.0;
    double tm_nlst = 0;
    double tm_cand = 0;
    trigg_cand_t cand;
    memset(&cand, 0, sizeof(cand));
    
    int cmd;
    for (;;) {
        cmd = 0;
        if (thrq_receive(&triggm.thrq_pool, &cmd, sizeof(cmd), que_tout) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (cmd) {
            case 0:     // thrq receive timeout
                slogd(triggm.log, "Thread pool %.3fs timeout\n", que_tout);
                break;
            case 1:     // start/top miner ?
                break;
            default:    // miner submit some nonce, try download new candidate?
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

        // get new job from pool every 10s
        if (tm_cand == 0 || monotime() - tm_cand > 10) {
            int retry = triggm.retry_num;
            while (trigg_get_cblock(&cand, CORELISTLEN) != 0) {
                if (--retry <= 0)
                    process_proper_exit(errno);
            }
            tm_cand = monotime();

            // compare blk num ???
            mux_lock(&triggm.cand_lock);
            trigg_cand_copy(&triggm.candidate, &cand);
            mux_unlock(&triggm.cand_lock);
        }
    }
}

// covert string(domain or IPv4 numbers-and-dots notation) to binary ip
uint32_t str2ip(char *addrstr)
{
    if (addrstr == NULL) 
        return 0;

    struct hostent *host;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    if (addrstr[0] < '0' || addrstr[0] > '9') {
        if ((host = gethostbyname(addrstr)) == NULL)
            return 0;
        else
            memcpy((char *)&(addr.sin_addr.s_addr), host->h_addr_list[0], host->h_length);
    } else {
        addr.sin_addr.s_addr = inet_addr(addrstr);
    }
    return addr.sin_addr.s_addr;
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

    slogd(triggm.log, "peer IPs from url '%s':\n%s", triggm.nodes_lst.url, triggm.nodes_lst.filedata);
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

