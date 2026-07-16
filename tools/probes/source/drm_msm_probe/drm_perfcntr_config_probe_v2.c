#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "msm_drm.h"

static unsigned next_power_of_two(unsigned x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

static unsigned ilog2_u(unsigned x) {
    unsigned r = 0;
    while (x > 1) {
        x >>= 1;
        r++;
    }
    return r;
}

static void try_config(const char *node,
                       const char *group_name,
                       uint32_t countable,
                       uint32_t flags,
                       uint32_t nr_countables) {
    printf("\n=== PERFCNTR_CONFIG test ===\n");
    printf("node=%s group=%s countable=%u flags=0x%x nr_countables=%u\n",
           node, group_name, countable, flags, nr_countables);

    int fd = open(node, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("[FAIL] open errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    uint32_t countables_storage[8];
    memset(countables_storage, 0, sizeof(countables_storage));
    countables_storage[0] = countable;

    struct drm_msm_perfcntr_group group;
    memset(&group, 0, sizeof(group));
    strncpy(group.group_name, group_name, sizeof(group.group_name) - 1);
    group.nr_countables = nr_countables;
    group.countables = (uint64_t)(uintptr_t)countables_storage;

    unsigned sample_size = sizeof(uint64_t) * (2 + nr_countables);
    unsigned bufsz = 2 * sample_size;
    unsigned bufsz_shift = ilog2_u(next_power_of_two(bufsz));

    struct drm_msm_perfcntr_config req;
    memset(&req, 0, sizeof(req));
    req.flags = flags;
    req.nr_groups = 1;
    req.groups = (uint64_t)(uintptr_t)&group;
    req.period = 1000000; /* 1 ms */
    req.bufsz_shift = bufsz_shift;
    req.group_stride = sizeof(struct drm_msm_perfcntr_group);

    printf("sample_size=%u bufsz=%u bufsz_shift=%u group_stride=%u\n",
           sample_size, bufsz, req.bufsz_shift, req.group_stride);

    errno = 0;
    int ret = ioctl(fd, DRM_IOCTL_MSM_PERFCNTR_CONFIG, &req);

    if (ret < 0) {
        printf("[FAIL] ioctl ret=%d errno=%d (%s)\n", ret, errno, strerror(errno));
    } else {
        printf("[OK] ioctl ret=%d\n", ret);

        if (flags & MSM_PERFCNTR_STREAM) {
            uint8_t buf[256];
            ssize_t n = read(ret, buf, sizeof(buf));
            if (n < 0) {
                printf("[FAIL] read stream fd=%d errno=%d (%s)\n", ret, errno, strerror(errno));
            } else {
                printf("[OK] read stream fd=%d bytes=%zd\n", ret, n);
                printf("first bytes:");
                for (ssize_t i = 0; i < n && i < 64; i++)
                    printf(" %02x", buf[i]);
                printf("\n");
            }
            close(ret);
        }
    }

    close(fd);
}

int main(void) {
    printf("=== DRM MSM PERFCNTR_CONFIG probe v2 ===\n");

    const char *nodes[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
    };

    for (unsigned i = 0; i < 2; i++) {
        const char *node = nodes[i];

        /* Mesa PPS-style: STREAM only. */
        try_config(node, "SP", 2, MSM_PERFCNTR_STREAM, 1);

        /* Try ALWAYSON too, in case SP is not accepted globally. */
        try_config(node, "ALWAYSON", 0, MSM_PERFCNTR_STREAM, 1);

        /* Try empty group list: used by Mesa reservation-style probing. */
        printf("\n=== PERFCNTR_CONFIG empty config ===\n");
        printf("node=%s flags=0 nr_groups=0\n", node);

        int fd = open(node, O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            printf("[FAIL] open errno=%d (%s)\n", errno, strerror(errno));
            continue;
        }

        struct drm_msm_perfcntr_config req;
        memset(&req, 0, sizeof(req));
        req.flags = 0;
        req.nr_groups = 0;
        req.groups = 0;
        req.group_stride = sizeof(struct drm_msm_perfcntr_group);

        errno = 0;
        int ret = ioctl(fd, DRM_IOCTL_MSM_PERFCNTR_CONFIG, &req);
        if (ret < 0) {
            printf("[FAIL] empty ioctl ret=%d errno=%d (%s)\n", ret, errno, strerror(errno));
        } else {
            printf("[OK] empty ioctl ret=%d\n", ret);
        }

        close(fd);
    }

    return 0;
}
