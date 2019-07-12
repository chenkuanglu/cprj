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


// c_cflag: 
// CLOCAL 本地模式，不改变端口的所有者
// CREAD 表示使能数据接收器
//
// PARENB 表示奇偶校验使能
// ~PARODD 表示偶校验, |PARODD 表示奇校验
//
// CSTOPB 使用两个停止位
// CSIZE 对数据的bit使用掩码
// CS8 数据宽度是8bit
//
// c_iflag:
// ICANON 使能规范输入，否则使用原始数据
// ECHO 回送(echo)输入数据
// ECHOE 回送擦除字符
// ISIG 使能SIGINTR，SIGSUSP， SIGDSUSP和 SIGQUIT 信号
//
// IXON 使能输出软件控制
// IXOFF 使能输入软件控制
// IXANY 允许任何字符再次开启数据流
// INLCR 把字符NL(0A)映射到CR(0D)
// IGNCR 忽略字符CR(0D)
// ICRNL 把CR(0D)映射成字符NR(0A)
// c_oflag： OPOST 输出后处理，如果不设置表示原始数据（本文使用原始数据）
// c_cc[VMIN]： 最少可读数据
// c_cc[VTIME]： 等待数据时间(10秒的倍数)
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
		options.c_cflag |= PARENB;      /* Enable parity */
		options.c_cflag |= PARODD;      /* odd */ 
		options.c_iflag |= INPCK | ISTRIP;       /* Enable parity checking on input */
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;      /* Enable parity */
		options.c_cflag &= ~PARODD;     /* even */ 
		options.c_iflag |= INPCK | ISTRIP;       /* Enable parity checking on input */
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
 
//
// 非阻塞：
// O_NONBLOCK, O_NDELAY:
// 它们的差别在于：在读操作时，如果读不到数据，O_NDELAY会使I/O函数马上返回0，但这又衍生出一个问题，
// 因为读取到文件末尾(EOF)时返回的也是0，这样无法区分是哪种情况。
// 因此，O_NONBLOCK就产生出来，它在读取不到数据时会回传-1，并且设置errno为EAGAIN
//
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

