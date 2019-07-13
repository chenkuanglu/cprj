// tty.c
// sim buffer 可以用thrq
// 发送1000,=> 发送1000/32次, 发送1 => 1次，即block=32
// sim buffer == linux io buffer
// ios buffer == app buffer （可选)
//
// io_write = no buffer ?
// tty_write = have buffer ?

#include "tty.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TTY_FD_MAX_NUM      32

static tty_fd_t fd_arr[TTY_FD_MAX_NUM];

int tty_open(const char *pathname, int flags)
{
    for (int i=0; i<TTY_FD_MAX_NUM; i++) {
        if (fd_arr[i].used == 0) {
            if ((fd_arr[i].fd = open(pathname, flags)) == -1) {
                return -1;
            }
            mux_init(&fd_arr[i].lock);
            fd_arr[i].read = read;
            fd_arr[i].write = write;
            fd_arr[i].close = close;
            fd_arr[i].used += 1;
            return i;
        }
    }

    return -1;
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

