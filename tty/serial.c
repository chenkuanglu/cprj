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

#ifdef __cplusplus
extern "C"{
#endif

int speed_arr[] = { 
    B1000000, B500000, B115200,B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
	B38400, B19200, B9600, B4800, B2400, B1200, B300, 
};

int name_arr[] = {
    1000000, 500000, 115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300,
	38400,  19200,  9600, 4800, 2400, 1200,  300, 
};

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
 
    switch (stopbits)
  	{
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
    options.c_cc[VTIME] = 5;
    options.c_cc[VMIN] = 0;

    tcflush(fd,TCIFLUSH);           /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0)
  	{
		return (-1);
	}
	
    return 0;
}
 
int ser_open(const char* device, int speed)
{
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
}

int ser_close(int fd)
{
    return close(fd);
}

int ser_send(int fd, const void *data, int len)
{
	if (len > 0) {
        return write(fd, data, len);
	}
    return -1;
}

int ser_receive(int fd, uint8_t *str_dat, int n)
{
    return read(fd, str_dat, n);
}


#ifdef __cplusplus
}
#endif

