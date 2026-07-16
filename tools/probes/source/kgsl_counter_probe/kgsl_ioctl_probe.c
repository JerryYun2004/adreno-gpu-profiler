#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linux/msm_kgsl.h"

static const char *group_name(unsigned int groupid) {
    switch (groupid) {
        case KGSL_PERFCOUNTER_GROUP_CP: return "CP";
        case KGSL_PERFCOUNTER_GROUP_RBBM: return "RBBM";
        case KGSL_PERFCOUNTER_GROUP_PC: return "PC";
        case KGSL_PERFCOUNTER_GROUP_VFD: return "VFD";
        case KGSL_PERFCOUNTER_GROUP_HLSQ: return "HLSQ";
        case KGSL_PERFCOUNTER_GROUP_UCHE: return "UCHE";
        case KGSL_PERFCOUNTER_GROUP_TP: return "TP";
        case KGSL_PERFCOUNTER_GROUP_SP: return "SP";
        case KGSL_PERFCOUNTER_GROUP_RB: return "RB";
        case KGSL_PERFCOUNTER_GROUP_VBIF: return "VBIF";
        case KGSL_PERFCOUNTER_GROUP_VSC: return "VSC";
        case KGSL_PERFCOUNTER_GROUP_CCU: return "CCU";
        case KGSL_PERFCOUNTER_GROUP_LRZ: return "LRZ";
        case KGSL_PERFCOUNTER_GROUP_CMP: return "CMP";
        case KGSL_PERFCOUNTER_GROUP_ALWAYSON: return "ALWAYSON";
        default: return "UNKNOWN";
    }
}

static void print_ioctl_result(const char *label, int ret) {
    if (ret == 0) {
        printf("[OK]   %s\n", label);
    } else {
        printf("[FAIL] %s ret=%d errno=%d (%s)\n",
               label, ret, errno, strerror(errno));
    }
}

static int do_query(int fd, unsigned int groupid) {
    uint32_t countables[256];
    memset(countables, 0, sizeof(countables));

    struct kgsl_perfcounter_query q;
    memset(&q, 0, sizeof(q));

    q.groupid = groupid;
    q.countables = countables;
    q.count = 256;
    q.max_counters = 0;

    errno = 0;
    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_QUERY, &q);

    printf("\n=== QUERY group=0x%02x (%s) ===\n",
           groupid, group_name(groupid));

    if (ret != 0) {
        print_ioctl_result("PERFCOUNTER_QUERY", ret);
        return ret;
    }

    printf("[OK]   PERFCOUNTER_QUERY max_counters=%u count=%u\n",
           q.max_counters, q.count);

    printf("       first active/free countables:");
    unsigned int limit = q.count < 16 ? q.count : 16;
    for (unsigned int i = 0; i < limit; i++) {
        printf(" 0x%08x", countables[i]);
    }
    printf("\n");

    return 0;
}

static int do_get_read_put(int fd, unsigned int groupid, unsigned int countable) {
    printf("\n=== GET/READ/PUT group=0x%02x (%s), countable=%u / 0x%x ===\n",
           groupid, group_name(groupid), countable, countable);

    struct kgsl_perfcounter_get get;
    memset(&get, 0, sizeof(get));
    get.groupid = groupid;
    get.countable = countable;

    errno = 0;
    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_GET, &get);
    if (ret != 0) {
        print_ioctl_result("PERFCOUNTER_GET", ret);
        return ret;
    }

    printf("[OK]   PERFCOUNTER_GET offset=0x%x offset_hi=0x%x\n",
           get.offset, get.offset_hi);

    struct kgsl_perfcounter_read_group read_group;
    memset(&read_group, 0, sizeof(read_group));
    read_group.groupid = groupid;
    read_group.countable = countable;
    read_group.value = 0;

    struct kgsl_perfcounter_read read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.reads = &read_group;
    read_req.count = 1;

    errno = 0;
    ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_READ, &read_req);
    if (ret != 0) {
        print_ioctl_result("PERFCOUNTER_READ", ret);
    } else {
        printf("[OK]   PERFCOUNTER_READ value=%" PRIu64 " / 0x%" PRIx64 "\n",
               (uint64_t)read_group.value, (uint64_t)read_group.value);
    }

    struct kgsl_perfcounter_put put;
    memset(&put, 0, sizeof(put));
    put.groupid = groupid;
    put.countable = countable;

    errno = 0;
    int put_ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_PUT, &put);
    if (put_ret != 0) {
        print_ioctl_result("PERFCOUNTER_PUT", put_ret);
    } else {
        printf("[OK]   PERFCOUNTER_PUT\n");
    }

    return ret;
}

int main(int argc, char **argv) {
    const char *dev = "/dev/kgsl-3d0";

    unsigned int groupid = KGSL_PERFCOUNTER_GROUP_ALWAYSON;
    unsigned int countable = 0;

    if (argc >= 2) {
        groupid = (unsigned int)strtoul(argv[1], NULL, 0);
    }
    if (argc >= 3) {
        countable = (unsigned int)strtoul(argv[2], NULL, 0);
    }

    printf("=== KGSL ioctl perf-counter probe ===\n");
    printf("device: %s\n", dev);
    printf("selected group: 0x%02x (%s)\n", groupid, group_name(groupid));
    printf("selected countable: %u / 0x%x\n", countable, countable);
    printf("sizeof(struct kgsl_perfcounter_get)=%zu\n", sizeof(struct kgsl_perfcounter_get));
    printf("sizeof(struct kgsl_perfcounter_query)=%zu\n", sizeof(struct kgsl_perfcounter_query));
    printf("sizeof(struct kgsl_perfcounter_read_group)=%zu\n", sizeof(struct kgsl_perfcounter_read_group));
    printf("sizeof(struct kgsl_perfcounter_read)=%zu\n", sizeof(struct kgsl_perfcounter_read));
    printf("\n");

    int fd = open(dev, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("[FAIL] open(%s) errno=%d (%s)\n", dev, errno, strerror(errno));
        return 1;
    }

    printf("[OK]   open(%s) fd=%d\n", dev, fd);

    do_query(fd, groupid);
    int ret = do_get_read_put(fd, groupid, countable);

    close(fd);
    return ret == 0 ? 0 : 2;
}
