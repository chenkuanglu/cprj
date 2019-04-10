/**
 * @file    trigg_miner.c
 * @author  ln
 * @brief   trigg miner
 **/

#include <curl/curl.h>
#include <sys/stat.h>
#include "trigg_miner.h"

#ifdef __cplusplus
extern "C" {
#endif 

trigg_miner_t triggm;

void* thread_trigg_miner(void *arg);
void* thread_trigg_pool(void *arg);

int trigg_init(start_info_t *sinfo)
{
    memset(&triggm, 0, sizeof(trigg_miner_t));
    triggm.log = core_getlog();

    triggm.nodes_lst.url = sinfo->argv[1];
    triggm.nodes_lst.filename = strdup("startnodes.lst");

    thrq_init(&triggm.thrq_miner);
    thrq_set_mpool(&triggm.thrq_miner, 0, 32);
    thrq_init(&triggm.thrq_pool);
    thrq_set_mpool(&triggm.thrq_pool, 0, 32);

    mux_init(&triggm.cand_lock);

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

        slogd(triggm.log, "tiime to check uart msg timeout...\n");
        // submit cur tbl , not new tbl
    }
}

void* thread_trigg_pool(void *arg)
{
    (void)arg;
    char ebuf[128];
    double tm_nlst = monotime();
    double tm_job = monotime();
    
#ifdef DEBUG
    if (access(triggm.nodes_lst.filename, R_OK) != 0) {
        slogd(triggm.log, "No file '%s'\n", triggm.nodes_lst.filename);
        trigg_download_lst();
    }
#else 
    trigg_download_lst();
#endif

    int cmd;
    for (;;) {
        cmd = 0;
        if (thrq_receive(&triggm.thrq_pool, &cmd, sizeof(cmd), 1.0) < 0) {  // 1s timeout
            if (errno != ETIMEDOUT) {
                sloge(triggm.log, "thrq_receive() from triggm.thrq_pool fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
                nsleep(0.1);
                continue;
            }
        }

        switch (cmd) {
            case 0:     // nothing
                break;
            case 1:     // start/top miner ?
                break;
            default:    // miner submit some nonce, try download new candidate?
                break;
        }

        if (monotime() - tm_nlst > 3600) {              // update nodes list every 1h
            slogd(triggm.log, "update triggm.nodes_lst\n");
            trigg_download_lst();
            tm_nlst = monotime();
        }
        if (monotime() - tm_job > 10) {                 // get new job from pool every 10s
            slogd(triggm.log, "get new job\n");
            tm_job = monotime();
        }
    }
}

uint32_t str2ip(char *addrstr)
{
    struct hostent *host;
    struct sockaddr_in addr;

    if (addrstr == NULL) return 0;

    memset(&addr, 0, sizeof(addr));
    if (addrstr[0] < '0' || addrstr[0] > '9') {
        host = gethostbyname(addrstr);
        if (host == NULL) {
            printf("str2ip(): gethostbyname() failed\n");
            return 0;
        }
        memcpy((char *) &(addr.sin_addr.s_addr),
                host->h_addr_list[0], host->h_length);
    }
    else
        addr.sin_addr.s_addr = inet_addr(addrstr);

    return addr.sin_addr.s_addr;
}

// convert nodes lst to core ip lst
int nodesl_to_coreipl(char *nodesl, uint32_t *cpl)
{
    int j;
    char *addrstr, *line;
    uint32_t ip;

    if (nodesl == NULL || *nodesl == '\0') 
        return -1;

    char *list = strdup(nodesl);
    for (j = 0; j < TRIGG_CORELIST_LEN; ) {
        if ((line = strline(&list)) == NULL) {
            free(list);
            break;
        }
        if (*line == '#') 
            continue;
        addrstr = strtok(line, " \r\n\t");
        slogd(triggm.log, "parse ip string '%s'\n", addrstr);
        if (addrstr == NULL) {
            free(list);
            break;
        }
        ip = str2ip(addrstr);
        if (!ip) continue;
        cpl[j++] = ip;
    }
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
    slogd(triggm.log, "download %d bytes from url '%s'", triggm.nodes_lst.len, triggm.nodes_lst.url);

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
        sloge(triggm.log, "curl_easy_perform() fail: %s\n", err_string(errno, errbuf, 128));
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

