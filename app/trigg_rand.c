/* rand.c  High speed random number generation.
 *
 * Copyright (c) 2018 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.PDF   **** NO WARRANTY ****
 *
 * Date: 2 January 2018
 *
*/

#include "trigg_rand.h"

#ifdef __cplusplus
extern "C" {
#endif 

/* Default initial seed for random generator */
static uint32_t Lseed = 1;
static uint32_t Lseed2 = 1;
static uint32_t Lseed3 = 362436069;
static uint32_t Lseed4 = 123456789;

/* Seed the generator */
uint32_t srand16(uint32_t x)
{
   uint32_t r;

   r = Lseed;
   Lseed = x;
   return r;
}

/* Return random seed to caller */
uint32_t getrand16(void)
{
   return Lseed;
}

void srand2(uint32_t x, uint32_t y, uint32_t z)
{
   Lseed2 = x;
   Lseed3 = y;
   Lseed4 = z;
}

/* Return random seed to caller */
void getrand2(uint32_t *x, uint32_t *y, uint32_t *z)
{
   *x = Lseed2;
   *y = Lseed3;
   *z = Lseed4;
}

/* Period: 2**32 randl4() -- returns 0-65535 */
uint32_t rand16(void)
{
   Lseed = Lseed * 69069L + 262145L;
   return (Lseed >> 16);
}


/* Based on Dr. Marsaglia's Usenet post */
uint32_t rand2(void)
{
   Lseed2 = Lseed2 * 69069L + 262145L;  /* LGC */
   if(Lseed3 == 0) Lseed3 = 362436069;
   Lseed3 = 36969 * (Lseed3 & 65535) + (Lseed3 >> 16);  /* MWC */
   if(Lseed4 == 0) Lseed4 = 123456789;
   Lseed4 ^= (Lseed4 << 17);
   Lseed4 ^= (Lseed4 >> 13);
   Lseed4 ^= (Lseed4 << 5);  /* LFSR */
   return (Lseed2 ^ (Lseed3 << 16) ^ Lseed4) >> 16;
}

#ifdef __cplusplus
}
#endif 

