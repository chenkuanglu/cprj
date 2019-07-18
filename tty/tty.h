// tty.h

#ifndef __TTY_X__
#define __TTY_X__

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>  
#include "../lib/mux.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mux_t           lock;
    int             fd;
} tty_fd_t;

extern int      tty_open(const char *pathname, int flags, long baud, uint8_t vtime, uint8_t vmin);
extern ssize_t  tty_read(int fd, void *buf, size_t count);
//extern ssize_t  tty_pollread(int fd, void *buf, size_t count);
extern ssize_t  tty_write(int fd, const void *buf, size_t count);
extern int      tty_close(int fd);

extern int      tty_setspeed(int fd, int speed);

#ifdef __cplusplus
}
#endif

#endif

