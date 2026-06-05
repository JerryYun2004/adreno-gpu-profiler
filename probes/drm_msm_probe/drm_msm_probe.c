#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <drm/drm.h>

static void probe_node(const char *path) {
    int fd = open(path, O_RDWR | O_CLOEXEC);
    printf("\n=== Probe %s ===\n", path);

    if (fd < 0) {
        printf("open failed: errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    char name[128] = {0};
    char date[128] = {0};
    char desc[512] = {0};

    struct drm_version ver = {
        .name_len = sizeof(name),
        .name = name,
        .date_len = sizeof(date),
        .date = date,
        .desc_len = sizeof(desc),
        .desc = desc,
    };

    int ret = ioctl(fd, DRM_IOCTL_VERSION, &ver);
    if (ret == 0) {
        printf("DRM_IOCTL_VERSION ok\n");
        printf("  name: %s\n", name);
        printf("  date: %s\n", date);
        printf("  desc: %s\n", desc);
    } else {
        printf("DRM_IOCTL_VERSION failed: errno=%d (%s)\n", errno, strerror(errno));
    }

    uint64_t value = 0;
    struct drm_get_cap cap = {
        .capability = DRM_CAP_SYNCOBJ,
        .value = 0,
    };

    ret = ioctl(fd, DRM_IOCTL_GET_CAP, &cap);
    if (ret == 0) {
        printf("DRM_CAP_SYNCOBJ: %llu\n", (unsigned long long)cap.value);
    } else {
        printf("DRM_IOCTL_GET_CAP(DRM_CAP_SYNCOBJ) failed: errno=%d (%s)\n", errno, strerror(errno));
    }

    close(fd);
}

int main(void) {
    probe_node("/dev/dri/renderD128");
    probe_node("/dev/dri/card0");
    return 0;
}
