/**
 * @file    rand.c
 * @author  ln
 * @brief   random 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "rand.h"
#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif 

// malloc memory
unsigned int rnd_genseed(const void *seed, size_t len, const char *file)
{
    uint32_t hashout[8];
    struct timespec tms;
    struct stat fstat;
    size_t file_size = 0;

    if (file != NULL) {
        stat(file, &fstat);
        file_size = fstat.st_size % 1024*1024;  // file_size < 1M
    }
    if (seed == NULL || len == 0) {
        seed = NULL;
        len = 64;
    }

    unsigned buf_size = len + sizeof(struct timespec) + file_size;
    char *buf = (char*)malloc(buf_size);
    if (buf) {
        clock_gettime(CLOCK_MONOTONIC, &tms);
        memcpy(buf, &tms, sizeof(struct timespec));
        if (seed) {
            memcpy(buf + sizeof(struct timespec), seed, len);
        }
        if (file_size) {
            FILE *fp = fopen(file, "rb");
            if (fp) {
                fread(buf + len + sizeof(struct timespec), 1, file_size, fp);
                fclose(fp);
            }
        }
        sha256(hashout, buf, buf_size);
        free(buf);
    } else {
        sha256(hashout, &tms, sizeof(struct timespec));
    }

    return *((unsigned int *)hashout);
}

// -1 or 0
int rnd_srandom(unsigned int seed, rnd_buf_t *buf)
{
    return srandom_r(seed, buf);
}

// -1 or 0
int rnd_random(rnd_buf_t *buf, int32_t *result)
{
    return random_r(buf, result);
}

#ifdef __cplusplus
}
#endif 

