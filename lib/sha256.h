/**
 * @file    sha256.h
 * @author  ln
 * @brief   sha256 hash
 */

#ifndef __SHA256_X_H__
#define __SHA256_X_H__

#include <stddef.h>
#define USE_OPENSSL

#ifndef USE_OPENSSL
#ifndef WORD32
#define WORD32
typedef unsigned char byte;      /* 8-bit byte */
typedef unsigned short word16;   /* 16-bit word */
typedef unsigned int word32;     /* 32-bit word  */
// for 16-bit machines:
// typedef unsigned long word32;
#endif  /* WORD32 */

typedef struct {
    byte data[64];
    unsigned datalen;
    unsigned long bitlen;
    unsigned long bitlen2;
    word32 state[8];
} SHA256_CTX;

#define SHA256_BLOCK_SIZE 32     /* SHA256 outputs a byte hash[32] digest */

/* Prototypes */
void SHA256_Init(SHA256_CTX *ctx);
void SHA256_Update(SHA256_CTX *ctx, const byte* data, size_t len);
void SHA256_Final(byte *hash, SHA256_CTX *ctx);  /* hash is 32 bytes */
#else
#include <openssl/sha.h>
#endif  /* USE_OPENSSL */

void sha256(void *hashout, const void *in, size_t inlen);

#endif   /* SHA256_H */

