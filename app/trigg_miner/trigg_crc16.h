/* crc16.c  16-bit CRC-CCITT checksum table  (poly 0x1021)
 *
 * Date: 20 December 2017
 *
 * Copyright (c) 2018 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.TXT   **** NO WARRANTY ****
 *
 * Date: 17 February 2018
 *
 */

#ifndef __CRC16_H__
#define __CRC16_H__

#include <stdint.h>

extern uint16_t Crc16table[];

#define update_crc16(crc, c) \
   ( ((uint16_t) (crc) << 8) ^ Crc16table[ ((uint16_t) (crc) >> 8) ^ (uint8_t) (c) ] )

extern uint16_t crc16(void *buff, int len);

#endif

