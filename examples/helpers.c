#include "examples/helpers.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

lmqtt_io_result_t get_time(long *secs, long *nsecs)
{
    struct timespec tim;

    if (clock_gettime(CLOCK_MONOTONIC, &tim) == 0) {
        *secs = tim.tv_sec;
        *nsecs = tim.tv_nsec;
        return LMQTT_IO_SUCCESS;
    }

    return LMQTT_IO_ERROR;
}

lmqtt_io_result_t socket_read(void *data, void *buf, size_t buf_len,
    size_t *bytes_read)
{
    int socket_fd = *((int *) data);
    int res;

    res = read(socket_fd, buf, buf_len);
    if (res >= 0) {
        *bytes_read = res;
        return LMQTT_IO_SUCCESS;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *bytes_read = 0;
        return LMQTT_IO_AGAIN;
    }

    *bytes_read = 0;
    return LMQTT_IO_ERROR;
}

lmqtt_io_result_t socket_write(void *data, void *buf, size_t buf_len,
    size_t *bytes_written)
{
    int socket_fd = *((int *) data);
    int res;

    res = write(socket_fd, buf, buf_len);
    if (res >= 0) {
        *bytes_written = res;
        return LMQTT_IO_SUCCESS;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
        *bytes_written = 0;
        return LMQTT_IO_AGAIN;
    }

    *bytes_written = 0;
    return LMQTT_IO_ERROR;
}

int socket_open(const char *address, unsigned short port)
{
    struct sockaddr_in sin;

    int result = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(result, F_SETFL, O_NONBLOCK);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &sin.sin_addr) == 0) {
        close(result);
        return -1;
    }

    if (connect(result, (struct sockaddr *) &sin, sizeof(sin)) != 0 &&
            errno != EINPROGRESS) {
        close(result);
        return -1;
    }

    return result;
}

void socket_close(int fd)
{
    close(fd);
}
