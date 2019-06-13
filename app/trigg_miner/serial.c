// serial driver

#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h>   
#include <fcntl.h>      
#include <termios.h>  
#include <pthread.h>  
#include <errno.h>  

#include "serial.h"
#include "../../timer/timer.h"
#include "../../lib/timetick.h"

#ifdef __cplusplus
extern "C"{
#endif

int speed_arr[] = { 
    B500000, B115200,B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
	B38400, B19200, B9600, B4800, B2400, B1200, B300, 
};

int name_arr[] = {
    500000, 115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300,
	38400,  19200,  9600, 4800, 2400, 1200,  300, 
};

#if SER_SIM_EN > 0
ser_sim_t ser_sim;
#endif 

static int set_speed(int fd, int speed)
{
    unsigned        i;
    int             status;
    struct termios  opt;
    
    tcgetattr(fd, &opt);
    for (i= 0;  i < (sizeof(speed_arr) / sizeof(int)); i++) {
        if (speed == name_arr[i]) {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&opt, speed_arr[i]);
            cfsetospeed(&opt, speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &opt);
            if  (status != 0) {
                return -1;
            }
            return 0;
     	}
    }

    return -1;
}


static int set_parity(int fd, int databits, int stopbits, int parity)
{
    struct termios options;
    
    if (tcgetattr(fd,&options) !=  0) {
      	return -1;
    }
    
	options.c_cflag |= CBAUDEX;
  	options.c_cflag &= ~CSIZE;
    options.c_cflag &= ~CSIZE; 
    options.c_cflag |= (CLOCAL | CREAD); 
    options.c_cflag &= ~CRTSCTS; 
    options.c_oflag &= ~OPOST; 
    options.c_cflag &= ~(ONLCR|OCRNL); 
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 
    options.c_iflag &= ~(ICRNL | INLCR); 
    options.c_iflag &= ~(IXON | IXOFF | IXANY); 

    switch (databits) {
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
            break;
        default:
            return -1;
    }

    switch (parity) {
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;     /* Clear parity enable */
            options.c_iflag &= ~INPCK;      /* Enable parity checking */
            break;
        case 'o':
        case 'O':
            options.c_cflag |= (PARODD | PARENB);   /* odd */ 
            options.c_iflag |= INPCK;               /* Disnable parity checking */
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;      /* Enable parity */
            options.c_cflag &= ~PARODD;       
            options.c_iflag |= INPCK;       /* Disnable parity checking */
            break;
        case 'S':
        case 's':                           /* as no parity*/
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~CSTOPB;
            break;
        default:
            return -1;
    }
 
    switch (stopbits) {
        case 1:
            options.c_cflag &= ~CSTOPB;
            break;
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            return -1;
    }
	
    /* Set input parity option */
    if (parity != 'n') {
  		options.c_iflag |= INPCK;
  	}
    options.c_cc[VTIME] = 5;    // 500ms
    options.c_cc[VMIN] = 0;

    tcflush(fd,TCIFLUSH);           /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0) {
		return (-1);
	}
	
    return 0;
}
 
#if SER_SIM_EN > 0
static void ser_sim_callback_70(void *arg)
{
    mux_lock(&ser_sim.lock);
    while (ser_sim.up_count != 0) {
        mux_unlock(&ser_sim.lock);
        nsleep(0.1);
        mux_lock(&ser_sim.lock);
    }
    mux_unlock(&ser_sim.lock);

    mux_lock(&ser_sim.lock);
    ser_sim.upstream[5] = 0x70;
    *((uint32_t *)ser_sim.upstream) = ser_sim.nonce_hit;
    ser_sim.upstream[4] = *((uint8_t *)arg);
    ser_sim.up_count = 6;
    mux_unlock(&ser_sim.lock);
}
static void ser_sim_callback_68(void *arg)
{
    mux_lock(&ser_sim.lock);
    while (ser_sim.up_count != 0) {
        mux_unlock(&ser_sim.lock);
        nsleep(0.1);
        mux_lock(&ser_sim.lock);
    }
    mux_unlock(&ser_sim.lock);

    mux_lock(&ser_sim.lock);
    ser_sim.upstream[5] = 0x68;
    *((uint32_t *)ser_sim.upstream) = ser_sim.nonce_done;
    ser_sim.upstream[4] = *((uint8_t *)arg);
    ser_sim.up_count = 6;
    mux_unlock(&ser_sim.lock);
}
#endif 

int ser_open(const char* device, int speed)
{
#if SER_SIM_EN > 0
    memset(&ser_sim, 0, sizeof(ser_sim));
    for (int i=0; i<256; i++) {
        ser_sim.id_tbl[i] = i;
    }
    mux_init(&ser_sim.lock);
    strcpy(ser_sim.dev, device);
    ser_sim.baud = speed;
    ser_sim.const_len = SER_SIM_CONST_LEN;
    ser_sim.first_hash = 0x92b3c90e;
    ser_sim.nonce_done = 0x23c3ffff;
    ser_sim.nonce_hit = 0x03636ef9;
    ser_sim.version = 0x2800000d;
    ser_sim.hit_delay = 2.0;
    ser_sim.done_delay = 3.0;
    return 1;
#else 
	int fd = open(device, O_RDWR | O_NOCTTY);
	if (fd > 0) {
    	if (set_parity(fd, 8, 1, 'N') < 0) {
            return -1;
    	}
	    set_speed(fd, speed);
	} else {
		return -1;
	}
	return fd;
#endif
}

int ser_close(int fd)
{
    return close(fd);
}

int ser_send(int fd, const void *data, int len)
{
#if SER_SIM_EN > 0
    static int wr_0x40 = 0;
    uint8_t *pdata = (uint8_t *)data;
    int id = pdata[1];
    int addr = pdata[3];
    
    if (wr_0x40) {
        wr_0x40 = 0;
        return len;
    }

    mux_lock(&ser_sim.lock);
    while (ser_sim.up_count != 0) {
        mux_unlock(&ser_sim.lock);
        nsleep(0.1);
        mux_lock(&ser_sim.lock);
    }
    mux_unlock(&ser_sim.lock);

    switch (addr) {
        case 0x40:
            ser_sim.upstream[5] = 0x64;
            *((uint32_t *)ser_sim.upstream) = ser_sim.first_hash;
            wr_0x40 = 1;
            break;
        case 0x24:
            ser_sim.upstream[5] = addr;
            *((uint32_t *)ser_sim.upstream) = ser_sim.version;
            break;
        case 0x0c:
            ser_sim.upstream[5] = addr;
            *((uint32_t *)ser_sim.upstream) = 0;
            break;
        default:
            mux_unlock(&ser_sim.lock);
            return len;
            break;
    }
    ser_sim.upstream[4] = id;
    ser_sim.up_count = 6;
    static int queid = -1;

    if (addr == 0x40) {
        tmr_add(&stdtmr, queid--, TMR_EVENT_TYPE_ONESHOT, ser_sim.hit_delay, ser_sim_callback_70, &ser_sim.id_tbl[id]);
        tmr_add(&stdtmr, queid--, TMR_EVENT_TYPE_ONESHOT, ser_sim.done_delay, ser_sim_callback_68, &ser_sim.id_tbl[id]);
    }
    mux_unlock(&ser_sim.lock);
    return len;
#else
	if (len > 0) {
        return write(fd, data, len);
	}
    return -1;
#endif
}

int ser_receive(int fd, uint8_t *str_dat, int n)
{
#if SER_SIM_EN > 0
    mux_lock(&ser_sim.lock);
    if (ser_sim.up_count == 6) {
        memcpy(str_dat, ser_sim.upstream, ser_sim.up_count);
    }
    if (ser_sim.up_count) {
        ser_sim.up_count -= 1;
        mux_unlock(&ser_sim.lock);
        return 1;
    }
    mux_unlock(&ser_sim.lock);
    nsleep(0.1);
    return 0;
#else
    return read(fd, str_dat, n);
#endif
}


#ifdef __cplusplus
}
#endif

