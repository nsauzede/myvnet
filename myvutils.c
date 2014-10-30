#include <unistd.h>

// read from fd and blocks until whole count bytes received
// returns -1 or 0 if any reads returned so
// else returns bytes read
ssize_t read_full( int fd, void *buf, size_t count)
{
    ssize_t ret = -1;
    ssize_t done = 0;

    while (done < count)
    {
        ret = read( fd, buf + done, count - done);
        if (ret <= 0)
            break;
        done += ret;
    }
    if (ret > 0)
        ret = done;

    return ret;
}
