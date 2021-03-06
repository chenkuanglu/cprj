// tty.c
// sim buffer 可以用thrq
// 发送1000,=> 发送1000/32次, 发送1 => 1次，即block=32
// sim buffer == linux io buffer
// ios buffer == app buffer （可选)
//
// io_write = no buffer ?
// tty_write = have buffer ?

#include "tty.h"
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

static int speed_arr[] = { 
    B1000000, B500000, B115200,B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300
};
static int speed_name_arr[] = {
    1000000, 500000, 115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300
};

int tty_setspeed(int fd, int speed)
{
    unsigned        i;
    struct termios  opt;
    
    tcgetattr(fd, &opt);
    for (i= 0; i < (sizeof(speed_arr) / sizeof(int)); i++) {
        if (speed == speed_name_arr[i]) {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&opt, speed_arr[i]);
            cfsetospeed(&opt, speed_arr[i]);
            if (tcsetattr(fd, TCSANOW, &opt) != 0) {
                return -1;
            }
            return 0;
     	}
    }

    errno = EINVAL;
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
//
// VMIN VTIME:
// 有效条件：阻塞 + read
//
static int tty_setattr(int fd, int databits, int parity, int stopbits, uint8_t vtime, uint8_t vmin)
{
    struct termios options;
    if (tcgetattr(fd,&options) !=  0) {
      	return -1;
    }
    
    options.c_cflag |= (CBAUDEX | CLOCAL | CREAD); 
    options.c_cflag &= ~(ONLCR | OCRNL | CSIZE | CRTSCTS); 

    options.c_oflag &= ~OPOST; 
    options.c_iflag &= ~(ICRNL | INLCR | IXON | IXOFF | IXANY); 

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 

    switch (databits) {
        case 5:
            options.c_cflag |= CS5;
            break;
        case 6:
            options.c_cflag |= CS6;
            break;
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
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
	
    switch (parity) {
        case 's':
        case 'S':
            options.c_cflag &= ~CSTOPB;
            //break;
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;         /* Clear parity enable */
            options.c_iflag &= ~INPCK;          /* Disable parity checking */
            break;
        case 'o':
        case 'O':
            options.c_cflag |= PARENB;          /* Enable parity */
            options.c_cflag |= PARODD;          /* odd */ 
            options.c_iflag |= INPCK | ISTRIP;  /* Enable parity checking on input */
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;          /* Enable parity */
            options.c_cflag &= ~PARODD;         /* even */ 
            options.c_iflag |= INPCK | ISTRIP;  /* Enable parity checking on input */
            break;
        default:
            return -1;
    }
 
    options.c_cc[VTIME] = vtime;
    options.c_cc[VMIN] = vmin;

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
		return -1;
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
//int fd = open(pathname, O_RDWR | O_NOCTTY);
//
///////////////////////////////////
// 假设：
// 1 read函数读取个数为size
// 2 VMIN参数值配置为vmin 
// 3 串口缓存t时刻个数为count
// 4 read函数返回值为ret
//
// 则有：
// 1 read函数在阻塞模式下变成非阻塞：vmin=0 && vtime=0
// 2 read函数只有满足条件返回，但没有超时返回：vtime=0 && vmin!=0
// 3 read函数既有满足条件返回，也有超时返回：vtime!=0 (vmin是否为0都可以超时返回)
// 4 read函数满足条件返回：vmin!=0 && count >= min(size,vmin) && ret = min(size,count)
// 5 read函数超时返回：( count<min(size,vmin) && (count>0 || count>=vmin) ) && ret = min(max(0,vmin), count)
//      a 如果vmin=0, 则read在超时后会返回0, 例如串口空闲时read将周期性返回
//      b 如果vmin!=0, 则read只能在count>0时超时返回
//
// 总结串口的阻塞功能：
// 1 满足指定的某个字节数vmin 或者 满足read期望值时read返回, 无超时返回
// 2 不满足指定的某个最小字节数vmin 并且 也不满足read期望值时 read超时返回系统缓存的剩余字节数，如果无剩余则不返回
// 3 不满足read期望值时 read总是超时返回，无论系统缓存有没有剩余数据
//
int tty_open(const char *pathname, int flags, long baud, uint8_t vtime, uint8_t vmin)
{
    int fd;
    if ((fd = open(pathname, flags)) == -1) {
    	return -1;
    }
    if (tty_setattr(fd, 8, 'N', 1, vtime, vmin) != 0) {
        tty_close(fd);
        return -1;
    }
    if (tty_setspeed(fd, baud) != 0) {
        tty_close(fd);
        return -1;
    }
    return fd;
}

ssize_t tty_read(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

ssize_t tty_write(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

int tty_close(int fd)
{
    return close(fd);
}

#ifdef __cplusplus
}
#endif

