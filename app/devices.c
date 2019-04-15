/**
 * @file    deveces.c
 * @author  ln
 * @brief   hardware rd/wr/ctrl
 **/

#include "global.h"
#include "devices.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BOARD_CR184_SOCKET = 0,

    BOARD_CR184_HASHRATE,
    BOARD_CR184_HASHRATE_NP,        // no power i2c

    BOARD_CR190_HASHRATE,

    BOARD_MODEL_COUNT               // number of board
};

int board_model = BOARD_CR190_HASHRATE;
int fpga_model = -1;

//fpga_model: 190(n:420 355 325 ...) + 184(1)

int dev_write(int fd, int id, int reg, uint32_t *pdata, int len)
{
#define MAX_DEV_WR_BUF_SIZE     512
    if (pdata == NULL || (len + 4) > MAX_DEV_WR_BUF_SIZE)
        return -1;

    char const_buf[MAX_DEV_WR_BUF_SIZE];

    switch (board_model) {
        case BOARD_CR184_SOCKET:        // socket test
        case BOARD_CR184_HASHRATE:      // k1
        case BOARD_CR184_HASHRATE_NP:   // k2
        case BOARD_CR190_HASHRATE:      // fpga
            switch (reg) {
                case FPGA_REG_IMAGE_DATA:
                case FPGA_REG_ALGO_STAGE:
                    cr190_write_l(fd, id, reg, (char*)pdata, len);
                    break;
                case FPGA_REG_CONSTANT:
                    *((int *)const_buf) = len;  // 4 bytes
                    memcpy(const_buf+4, pdata, len);
                    cr190_write_l(fd, id, reg, const_buf, len);
                    break;
                default:
                    cr190_write(fd, id, reg, *pdata);
                    break;
            }
            break;

        default:
            return -1;
            break;
    }

    return 0;
}

//
//reset on different BORD* 
//

#ifdef __cplusplus
}
#endif

