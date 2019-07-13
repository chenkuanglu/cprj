// tty.h

#ifndef __TTY_X__
#define __TTY_X__

#include <unistd.h>
#include <fcntl.h>
#include "../lib/mux.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ssize_t (*tty_write_t)(int fd, const void *buf, size_t count);
typedef ssize_t (*tty_read_t)(int fd, void *buf, size_t count);
typedef int (*tty_close_t)(int fd);

typedef struct {
    int             used;   // user counter
    mux_t           lock;

    int             fd;
    tty_read_t      read;
    tty_write_t     write;
    tty_close_t     close;
} tty_fd_t;

extern int      tty_open(const char *pathname, int flags);
extern ssize_t  tty_read(int fd, void *buf, size_t count);
extern ssize_t  tty_write(int fd, const void *buf, size_t count);
extern int      tty_close(int fd);

#ifdef __cplusplus
}
#endif

#endif

