/* rand.h  High speed random number generation.
*/

#ifndef __TRIGG_RAND_H__
#define __TRIGG_RAND_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

extern uint32_t srand16(uint32_t x);
extern uint32_t getrand16(void);
extern void srand2(uint32_t x, uint32_t y, uint32_t z);
extern void getrand2(uint32_t *x, uint32_t *y, uint32_t *z);
extern uint32_t rand16(void);
extern uint32_t rand2(void);

#ifdef __cplusplus
}
#endif 

#endif

