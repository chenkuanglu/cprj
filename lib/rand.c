/**
 * @file    rand.c
 * @author  ln
 * @brief   random 
 **/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include "../crypto/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif 

unsigned int rnd_gen_seed(const void *seed, size_t len, char *file)
{
    uint32_t hashout[8];
    struct timespec tms;
    struct stat fstat;
    size_t file_size;

    clock_gettime(CLOCK_MONOTONIC, &tms);
    stat(file, &fstat);
    file_size = fstat.st_size;

    unsigned buf_size = len + sizeof(struct timespec) + file_size;
    char *buf = (char*)malloc(buf_size);
    memcpy(buf, seed, len);
    memcpy(buf, &tms, sizeof(struct timespec));
    FILE *fp = fopen(file, "r");
    if (fp) {
        fread(buf + len + sizeof(struct timespec), 1, file_size, fp);
        fclose(fp);
    }

    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, buf, buf_size);
    sha256_final(&ctx, hashout);

    free(buf);
    return hashout[7];
}

#ifdef __cplusplus
}
#endif 

