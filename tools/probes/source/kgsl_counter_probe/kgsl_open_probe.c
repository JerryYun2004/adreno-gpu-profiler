#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    const char *path = "/dev/kgsl-3d0";

    printf("=== KGSL open probe ===\n");
    printf("target: %s\n", path);

    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("open O_RDWR failed: errno=%d (%s)\n", errno, strerror(errno));
    } else {
        printf("open O_RDWR success: fd=%d\n", fd);
        close(fd);
    }

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("open O_RDONLY failed: errno=%d (%s)\n", errno, strerror(errno));
    } else {
        printf("open O_RDONLY success: fd=%d\n", fd);
        close(fd);
    }

    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("open O_WRONLY failed: errno=%d (%s)\n", errno, strerror(errno));
    } else {
        printf("open O_WRONLY success: fd=%d\n", fd);
        close(fd);
    }

    return 0;
}
