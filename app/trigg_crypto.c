// 
// trigg_crypto.c
//
#include "trigg_util.h"
#include "trigg_rand.h"
#include "trigg_crc16.h"
#include "trigg_crypto.h"
#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif 

long long trigg_diff_val(byte d)
{
    int64_t val = ~((int64_t)0x0);
    byte *bp = (byte*)&val, n;

    n = d >> 3;
    for (int i=0; i<n; i++) {
        bp[(7-i) & 7] = 0;
    }
    if ((d & 7) == 0) 
        return val;
    else 
        bp[(7-n) & 7] = 0xff >> (d & 7);
    return val;
}

int trigg_eval(byte *h, byte d)
{
    byte *bp, n;

    n = d >> 3;
    for (bp = h; n; n--) {
        if (*bp++ != 0) return NIL;
    }
    if ((d & 7) == 0) return T;
    if ((*bp & (~(0xff >> (d & 7)))) != 0)
        return NIL;
    return T;
}

int trigg_step(byte *in, int n)
{
    byte *bp;

    for (bp = in; n; n--, bp++) {
        bp[0]++;
        if (bp[0] != 0) break;
    }
    return T;
}

byte *trigg_gen(byte *in)
{
    byte *hp;
    FE *fp;
    int j, widx;

    fp = FRAME();
    hp = in;
    for (j = 0; j < 16; j++, fp++) {
        if (*fp == NIL) {
            NCONC(hp, NIL);
            continue;
        }
        if (MEMQ(F_XLIT, *fp)) {
            widx = CDR(*fp);
        }
        else {
            for (;;) {
                widx = TOKEN();
                if (CAT(widx, *fp)) break;
            }
        }
        NCONC(hp, widx);
    }
    return in;
}

char *trigg_expand(byte *chain, byte *in)
{
    int j;
    byte *bp, *w;

    bp = &chain[32];
    memset(bp, 0, 256);
    for (j = 0; j < 16; j++, in++) {
        if (*in == NIL) break;
        w = TPTR(*in);
        while (*w) *bp++ = *w++;
        if (bp[-1] != '\n') *bp++ = ' ';
    }
    return (char *)&chain[32];
}

char *trigg_generate(byte *chain, byte *nonce, int diff)
{
    byte h[32];

    trigg_gen(&chain[32 + 256]);
    sha256(chain, (32 + 256 + 16 + 8), h);
    if (trigg_eval(h, diff) == NIL) {
        trigg_step((chain + 32 + 256), 16);
        return NULL;
    }
    memcpy(nonce + 16, &chain[32 + 256], 16);

    return (char *)nonce;
}

#ifdef __cplusplus
}
#endif 
