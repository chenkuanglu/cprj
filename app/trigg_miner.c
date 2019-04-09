/**
 * @file    trigg_miner.c
 * @author  ln
 * @brief   trigg miner
 **/

#include <curl/curl.h>
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
    triggm.url = sinfo->argv[1];

    if (pthread_create(&triggm.thr_miner, NULL, thread_trigg_miner, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_miner' fail\n");
        return -1;
    }
    if (pthread_create(&triggm.thr_pool, NULL, thread_trigg_pool, &triggm) != 0) {
        sloge(triggm.log, "create 'thread_trigg_pool' fail\n");
        return -1;
    }

    //trigg_start();

    return 0;
}

void* thread_trigg_miner(void *arg)
{
    (void)arg;
    for (;;) {
        slogi(triggm.log, "miner...\n");
        nsleep(3);
    }
}

void* thread_trigg_pool(void *arg)
{
    (void)arg;
    //trigg_download_lst("startnodes.lst");
    for (;;) {
        slogi(triggm.log, "pool...\n");
        nsleep(3);
    }
}

// curl callback function for download nodes list
static size_t trigg_curl_write_nodes(void *src, size_t size, size_t nmemb, void *dst)
{
    (void)dst;
    int len = size*nmemb;
    if (triggm.nodes_lst == NULL) {
        triggm.nodes_lst = (char *)malloc(len+1);
    } else {
        triggm.nodes_lst = (char *)realloc(triggm.nodes_lst, triggm.nlst_len+len+1);
    }
    memcpy((char*)triggm.nodes_lst + triggm.nlst_len, src, len);
    triggm.nlst_len += len;
    slogd(triggm.log, "download %d bytes from url '%s'\n", triggm.nlst_len, triggm.url);

    return len;
}

// download nodes list from url
int trigg_download_lst(const char *file)
{
    char errbuf[128];
    triggm.nlst_len = 0;

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, triggm.url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, trigg_curl_write_nodes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, triggm.nodes_lst);
    CURLcode res = curl_easy_perform(curl);
    if (CURLE_OK != res) {
        sloge(triggm.log, "curl_easy_perform() fail: %s\n", err_string(errno, errbuf, 128));
        return -1;
    }
    curl_easy_cleanup(curl);

    slogd(triggm.log, "peer IPs from url '%s':\n%s\n", triggm.url, triggm.nodes_lst);
    FILE *fp = fopen(file, "w+");
    if (fp) {
        slogd(triggm.log, "write nodes list to file '%s'\n", file);
        fwrite(triggm.nodes_lst, 1, triggm.nlst_len, fp);
        fclose(fp);
    } else {
        sloge(triggm.log, "fopen() fail: %s\n", err_string(errno, errbuf, 128));
    }
    return 0;
}

#ifdef __cplusplus
}
#endif 

