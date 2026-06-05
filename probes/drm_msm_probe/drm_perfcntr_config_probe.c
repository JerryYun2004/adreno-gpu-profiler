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

static void try_perfcntr_config(const char *node, uint32_t flags) {
    printf("\n=== DRM PERFCNTR_CONFIG probe ===\n");
    printf("node:  %s\n", node);
    printf("flags: 0x%x\n", flags);

    int fd = open(node, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("[FAIL] open failed: errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    printf("[OK] open fd=%d\n", fd);

    /*
     * Target:
     *   group_name = "SP"
     *   countable  = 2
     *
     * From Mesa A8XX definitions:
     *   A8XX_PERF_SP_ALU_WORKING_CYCLES = 2
     */
    uint32_t sp_countables[1] = { 2 };

    struct drm_msm_perfcntr_group groups[1];
    memset(groups, 0, sizeof(groups));

    strncpy(groups[0].group_name, "SP", sizeof(groups[0].group_name) - 1);
    groups[0].nr_countables = 1;
    groups[0].pad = 0;
    groups[0].countables = (uint64_t)(uintptr_t)sp_countables;

    struct drm_msm_perfcntr_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.flags = flags;
    cfg.nr_groups = 1;
    cfg.groups = (uint64_t)(uintptr_t)groups;
    cfg.period = 1000000;          /* 1 ms, only meaningful for STREAM */
    cfg.bufsz_shift = 12;          /* 4096 bytes */
    cfg.group_stride = sizeof(struct drm_msm_perfcntr_group);

    printf("sizeof(struct drm_msm_perfcntr_group)  = %zu\n",
           sizeof(struct drm_msm_perfcntr_group));
    printf("sizeof(struct drm_msm_perfcntr_config) = %zu\n",
           sizeof(struct drm_msm_perfcntr_config));
    printf("group_name=%s nr_countables=%u countable[0]=%u\n",
           groups[0].group_name, groups[0].nr_countables, sp_countables[0]);
    printf("period=%" PRIu64 " ns bufsz_shift=%u group_stride=%u\n",
           (uint64_t)cfg.period, cfg.bufsz_shift, cfg.group_stride);

    errno = 0;
    int ret = ioctl(fd, DRM_IOCTL_MSM_PERFCNTR_CONFIG, &cfg);

    if (ret < 0) {
        printf("[FAIL] DRM_IOCTL_MSM_PERFCNTR_CONFIG ret=%d errno=%d (%s)\n",
               ret, errno, strerror(errno));
    } else {
        printf("[OK] DRM_IOCTL_MSM_PERFCNTR_CONFIG ret=%d\n", ret);

        if (flags & MSM_PERFCNTR_STREAM) {
            printf("[INFO] STREAM mode: ioctl return value may be a stream fd.\n");

            uint8_t buf[256];
            ssize_t n = read(ret, buf, sizeof(buf));

            if (n < 0) {
                printf("[FAIL] read(stream_fd=%d) errno=%d (%s)\n",
                       ret, errno, strerror(errno));
            } else {
                printf("[OK] read(stream_fd=%d) returned %zd bytes\n", ret, n);
                printf("first bytes:");
                for (ssize_t i = 0; i < n && i < 64; i++) {
                    printf(" %02x", buf[i]);
                }
                printf("\n");
            }

            if (ret != fd) {
                close(ret);
            }
        }
    }

    close(fd);
}

int main(void) {
    printf("=== MSM DRM perf-counter config ioctl probe ===\n");
    printf("Target counter: group SP, countable 2 = A8XX_PERF_SP_ALU_WORKING_CYCLES\n");
    printf("DRM_MSM_PERFCNTR_CONFIG = 0x%x\n", DRM_MSM_PERFCNTR_CONFIG);
    printf("MSM_PERFCNTR_STREAM     = 0x%x\n", MSM_PERFCNTR_STREAM);
    printf("MSM_PERFCNTR_UPDATE     = 0x%x\n", MSM_PERFCNTR_UPDATE);
    printf("\n");

    const char *nodes[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
    };

    for (unsigned i = 0; i < 2; i++) {
        /*
         * Test without STREAM first. If supported, this may simply configure/update.
         */
        try_perfcntr_config(nodes[i], MSM_PERFCNTR_UPDATE);

        /*
         * Test STREAM mode. If supported, ioctl may return a readable stream fd.
         */
        try_perfcntr_config(nodes[i], MSM_PERFCNTR_STREAM | MSM_PERFCNTR_UPDATE);
    }

    return 0;
}
