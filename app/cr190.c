/**
 * @file    cr190.c
 * @author  ln
 * @brief   hardware definition
 **/

#include "cr190.h"
#include "serial.h"
#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FPGA_REG_190_FREQ 0x34 

static pthread_mutex_t cr190_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;

int cr190_reset_init(void)
{
#define CR190_RESET_PIN     18
    const char *cmd_exist = "ls /sys/class/gpio | grep gpio" MAKE_CSTR(CR190_RESET_PIN) " | wc -l";
    const char *cmd_new = "echo " MAKE_CSTR(CR190_RESET_PIN) " > /sys/class/gpio/export";
    const char *cmd_out = "echo out > /sys/class/gpio/gpio" MAKE_CSTR(CR190_RESET_PIN) "/direction";
    const char *cmd_set = "echo 1 > /sys/class/gpio/gpio" MAKE_CSTR(CR190_RESET_PIN) "/value";
    const char *cmd_clr = "echo 0 > /sys/class/gpio/gpio" MAKE_CSTR(CR190_RESET_PIN) "/value";

    FILE *fp;
    char buf[8] = {0};
    if ((fp = popen(cmd_exist, "r")) == NULL) {
        return -1;
    }
    if (fgets(buf, sizeof(buf)-1, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    if (atoi(buf) == 0) {
        if ((fp = popen(cmd_new, "r")) == NULL) {   // create gpio
            return -1;
        }
        pclose(fp);
        if ((fp = popen(cmd_out, "r")) == NULL) {   // set output
            return -1;
        }
        pclose(fp);
        if ((fp = popen(cmd_clr, "r")) == NULL) {   // set 0 (reset)
            return -1;
        }
        pclose(fp);
        usleep(500*1000);
        if ((fp = popen(cmd_set, "r")) == NULL) {   // set 1 (restore)
            return -1;
        }
        pclose(fp);
    }

    return 0;
}

int cr190_hw_reset(void)
{
#define CR190_RESET_PIN     18
    const char *cmd_exist = "ls /sys/class/gpio | grep gpio" MAKE_CSTR(CR190_RESET_PIN) " | wc -l";
    const char *cmd_set = "echo 1 > /sys/class/gpio/gpio" MAKE_CSTR(CR190_RESET_PIN) "/value";
    const char *cmd_clr = "echo 0 > /sys/class/gpio/gpio" MAKE_CSTR(CR190_RESET_PIN) "/value";

    FILE *fp;
    char buf[8] = {0};
    if ((fp = popen(cmd_exist, "r")) == NULL) {
        return -1;
    }
    if (fgets(buf, sizeof(buf)-1, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    if (atoi(buf) == 0) {
        return -1;
    }

    if ((fp = popen(cmd_clr, "r")) == NULL) {   // set 0 (reset)
        return -1;
    }
    pclose(fp);
    usleep(500*1000);
    if ((fp = popen(cmd_set, "r")) == NULL) {   // set 1 (restore)
        return -1;
    }
    pclose(fp);

    return 0;
}

int cr190_read(int fd, uint8_t id, uint8_t reg)
{
    uint8_t buffer[4] = {0};

    buffer[0] = 0;
    buffer[1] = id;
    buffer[2] = 0x2a;
    buffer[3] = reg;

    pthread_mutex_lock(&cr190_cmd_mutex);
    ser_send(fd, buffer, sizeof(buffer));
    pthread_mutex_unlock(&cr190_cmd_mutex);

    slogd(CLOG, "read  id=%03d, reg=0x%02x\n", id, reg);

    return 0;
}

int cr190_write(int fd, uint8_t id, uint8_t reg, uint32_t data)
{
    uint8_t buffer[8] = {0};

    buffer[0] = 0;
    buffer[1] = id;
    buffer[2] = 0x32;
    buffer[3] = reg;
    memcpy(&buffer[4], &data, 4);

    pthread_mutex_lock(&cr190_cmd_mutex);
    ser_send(fd, buffer, sizeof(buffer));
    pthread_mutex_unlock(&cr190_cmd_mutex);

    slogd(CLOG, "write id=%03d, reg=0x%02x, data=0x%08x\n", id, reg, data);

    return 0;
}

// only constant has the 'length' field !!!
// if data is the constant, the begining of data should be the 'length' field 
int cr190_write_l(int fd, uint8_t id, uint8_t reg, char *data, int len)
{
    if (data == NULL || len <= 0)
        return -1;

    uint8_t buffer[4] = {0};

    buffer[0] = 0;
    buffer[1] = id;
    buffer[2] = 0x32;
    buffer[3] = reg;

    pthread_mutex_lock(&cr190_cmd_mutex);
    ser_send(fd, buffer, sizeof(buffer));
    ser_send(fd, data, len);
    pthread_mutex_unlock(&cr190_cmd_mutex);

    slogd(CLOG, "write id=%03d, reg=0x%02x,  len=%d\n", id, reg, len);

    return 0;
}

int cr190_fcalc(uint32_t freq, uint32_t max, cr190_freq_t *fcb)
{
    if (fcb == NULL || freq == 0)
        return -1;

    int reg_count;
    int reg_08 = 0;
    int reg_09 = 0;
    int reg_07 = 0;
    if (max % freq == 0) {
        reg_count = 2;
        int div = max/freq;
        int edge = div % 2;
        int htime = div/2;
        int ltime = div/2 + edge;
        reg_08 = (1 << 12) | (htime << 6) | ltime;
        reg_09 = edge << 7;
    } else {
        reg_count = 3;
        int div = max/freq;
        int div_frac = ((double)max/freq - div) * 8;
        int edge = div % 2;
        int htime = div/2;
        int ltime = div/2;
        int edge_frac = edge*8 + div_frac;
        if (edge_frac <= 8) {
            htime--;
            ltime--;
        } else if (edge_frac <= 9) {
            ltime--;
        }
        int pmfall = (edge*4 + div_frac/2) % 8;
        int wf_fall = 0;
        int wf_rise = 0;
        if ((edge_frac <= 9 && edge_frac >= 2) || (double)max/freq == 2.125) {
            wf_fall = 1;
        }
        if ((edge_frac <= 8) && (edge_frac >= 1)) {
            wf_rise = 1;
        }
        reg_08 = (1 << 12) | (htime << 6) | ltime;
        reg_09 = (div_frac << 12) | (1 << 11) | (wf_rise << 10);
        reg_07 = (pmfall << 11) | (wf_fall << 10);
    }
    fcb->DADDR_08 = (0x08 << 24) | (reg_08 << 8) | reg_count;
    fcb->DADDR_09 = (0x09 << 24) | (reg_09 << 8) | reg_count;
    fcb->DADDR_07 = (0x07 << 24) | (reg_07 << 8) | reg_count;
    fcb->len = reg_count;
    fcb->div = ((double)max)/freq;

    return 0;
}

int cr190_fwrite(int fd, int id, cr190_freq_t *fcb)
{
    for (int i=0; i<fcb->len; i++) {
        cr190_write(fd, id, FPGA_REG_190_FREQ, (fcb->DADDR[i] & ~0xf0UL) |  0x10UL);
        cr190_write(fd, id, FPGA_REG_190_FREQ, (fcb->DADDR[i] & ~0xf0UL) & ~0x10UL);
    }
    return 0;
}

int cr190_freq_config(int fd, int id, uint32_t freq, cr190_freq_t *fcb)
{
    if ((freq == 0) || (freq > CR190_MAX_FREQ)) {
        return -1;
    }

    cr190_freq_t cb;
    if (fcb == NULL)
        fcb = &cb;
    cr190_fcalc(freq, CR190_MAX_FREQ, fcb);
    cr190_fwrite(fd, id, fcb);
    return 0;
}

#ifdef __cplusplus
}
#endif

