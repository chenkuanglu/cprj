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

static int Z_PREP[8] = { 12,13,14,15,16,17,12,13 }; /* Confirmed */
static int Z_ING[32] = { 18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,23,24,31,32,33,34 }; /* Confirmed */
static int Z_INF[16] = { 44,45,46,47,48,50,51,52,53,54,55,56,57,58,59,60 }; /* Confirmed */
static int Z_ADJ[64] = { 61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,94,95,96,97,98,99,100,101,102,103,104,105,107,108,109,110,112,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128 }; /* Confirmed */
static int Z_AMB[16] = { 77,94,95,96,126,214,217,218,220,222,223,224,225,226,227,228 }; /* Confirmed */
static int Z_TIMED[8] = { 84,243,249,250,251,252,253,255 }; /* Confirmed */
static int Z_NS[64] = { 129,130,131,132,133,134,135,136,137,138,145,149,154,155,156,157,177,178,179,180,182,183,184,185,186,187,188,189,190,191,192,193,194,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,241,244,245,246,247,248,249,250,251,252,253,254,255 }; /* Confirmed */
static int Z_NPL[32] = { 139,140,141,142,143,144,146,147,148,150,151,153,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,181 }; /* Confirmed */
static int Z_MASS[32] = { 214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,242,214,215,216,219 }; /* Confirmed */
static int Z_INGINF[32] = { 18,19,20,21,22,25,26,27,28,29,30,36,37,38,39,40,41,42,44,46,47,48,49,51,52,53,54,55,56,57,58,59 }; /* Confirmed */
static int Z_TIME[16] = { 82,83,84,85,86,87,88,243,249,250,251,252,253,254,255,253 }; /* Confirmed */
static int Z_INGADJ[64] = { 18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,23,24,31,32,33,34,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92 };/* Confirmed */

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

byte *trigg_gen_seed(byte *seed)
{
    uint32_t thread = *((uint32_t *)seed);
    memset(seed, 0, 16);
    
    if (thread > 600047615UL) {
        return NULL;
    }

    if (0 < thread && thread <= 131071) { /* Total Permutations, this frame: 131,072 */
        seed[0] = Z_PREP[(thread & 7)];
        seed[1] = Z_TIMED[(thread >> 3) & 7];
        seed[2] = 1;
        seed[3] = 5;
        seed[4] = Z_NS[(thread >> 6) & 63];
        seed[5] = 1;
        seed[6] = Z_ING[(thread >> 12) & 31];
    }
    if (131071 < thread && thread <= 262143) { /* Total Permutations, this frame: 131,072 */
        seed[0] = Z_TIME[(thread & 15)];
        seed[1] = Z_MASS[(thread >> 4) & 31];
        seed[2] = 1;
        seed[3] = Z_INF[(thread >> 9) & 15];
        seed[4] = 9;
        seed[5] = 2;
        seed[6] = 1;
        seed[7] = Z_AMB[(thread >> 13) & 15];
    }
    if (262143 < thread && thread <= 4456447) { /* Total Permutations, this frame: 4,194,304 */
        seed[0] = Z_PREP[(thread & 7)];
        seed[1] = Z_TIMED[(thread >> 3) & 7];
        seed[2] = 1;
        seed[3] = Z_ADJ[(thread >> 6) & 63];
        seed[4] = Z_NPL[(thread >> 12) & 31];
        seed[5] = 1;
        seed[6] = Z_INGINF[(thread >> 17) & 31];
    }
    if (4456447 < thread && thread <= 12845055) { /* Total Permutations, this frame: 8,388,608 */
        seed[0] = 5;
        seed[1] = Z_NS[(thread & 63)];
        seed[2] = 1;
        seed[3] = Z_PREP[(thread >> 6) & 7];
        seed[4] = Z_TIMED[(thread >> 9) & 7];
        seed[5] = Z_MASS[(thread >> 12) & 31];
        seed[6] = 3;
        seed[7] = 1;
        seed[8] = Z_ADJ[(thread >> 17) & 63];
    }
    if (12845055 < thread && thread <= 29622271) { /* Total Permutations, this frame: 16,777,216 */
        seed[0] = Z_PREP[thread & 7];
        seed[1] = Z_ADJ[(thread >> 3) & 63];
        seed[2] = Z_MASS[(thread >> 9) & 31];
        seed[3] = 1;
        seed[4] = Z_NPL[(thread >> 14) & 31];
        seed[5] = 1;
        seed[6] = Z_INGINF[(thread >> 19) & 31];
    }
    if (29622271 < thread && thread <= 46399487) { /* Total Permutations, this frame: 16,777,216 */
        seed[0] = Z_PREP[(thread & 7)];
        seed[1] = Z_MASS[(thread >> 3) & 31];
        seed[2] = 1;
        seed[3] = Z_ADJ[(thread >> 8) & 63];
        seed[4] = Z_NPL[(thread >> 14) & 31];
        seed[5] = 1;
        seed[6] = Z_INGINF[(thread >> 19) & 31];
    }
    if (46399487 < thread && thread <= 63176703) { /* Total Permutations, this frame: 16,777,216 */
        seed[0] = Z_TIME[(thread & 15)];
        seed[1] = Z_AMB[(thread >> 4) & 15];
        seed[2] = 1;
        seed[3] = Z_ADJ[(thread >> 8) & 63];
        seed[4] = Z_MASS[(thread >> 14) & 31];
        seed[5] = 1;
        seed[6] = Z_ING[(thread >> 19) & 31];
    }
    if (63176703 < thread && thread <= 600047615) { /* Total Permutations, this frame: 536,870,912 */
        seed[0] = Z_TIME[(thread & 15)];
        seed[1] = Z_AMB[(thread >> 4) & 15];
        seed[2] = 1;
        seed[3] = Z_PREP[(thread >> 8) & 7];
        seed[4] = 5;
        seed[5] = Z_ADJ[(thread >> 11) & 63];
        seed[6] = Z_NS[(thread >> 17) & 63];
        seed[7] = 3;
        seed[8] = 1;
        seed[9] = Z_INGADJ[(thread >> 23) & 63];
    }

    return seed;
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

