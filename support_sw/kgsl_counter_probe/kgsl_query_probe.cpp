#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// From Mesa/Freedreno msm_kgsl.h.
#define KGSL_IOC_TYPE 0x09

#define KGSL_PERFCOUNTER_GROUP_MAX 0x39

struct kgsl_perfcounter_query {
    unsigned int groupid;
    unsigned int *countables;
    unsigned int count;
    unsigned int max_counters;
    unsigned int __pad[2];
};

#define IOCTL_KGSL_PERFCOUNTER_QUERY \
    _IOWR(KGSL_IOC_TYPE, 0x3A, struct kgsl_perfcounter_query)

static const char *group_name(unsigned int groupid) {
    switch (groupid) {
        case 0x00: return "CP";
        case 0x01: return "RBBM";
        case 0x02: return "PC";
        case 0x03: return "VFD";
        case 0x04: return "HLSQ";
        case 0x05: return "VPC";
        case 0x06: return "TSE";
        case 0x07: return "RAS";
        case 0x08: return "UCHE";
        case 0x09: return "TP";
        case 0x0A: return "SP";
        case 0x0B: return "RB";
        case 0x0C: return "PWR";
        case 0x0D: return "VBIF";
        case 0x0E: return "VBIF_PWR";
        case 0x0F: return "MH";
        case 0x10: return "PA_SU";
        case 0x11: return "SQ";
        case 0x12: return "SX";
        case 0x13: return "TCF";
        case 0x14: return "TCM";
        case 0x15: return "TCR";
        case 0x16: return "L2";
        case 0x17: return "VSC";
        case 0x18: return "CCU";
        case 0x19: return "LRZ";
        case 0x1A: return "CMP";
        case 0x1B: return "ALWAYSON";
        case 0x1C: return "SP_PWR";
        case 0x1D: return "TP_PWR";
        case 0x1E: return "RB_PWR";
        case 0x1F: return "CCU_PWR";
        case 0x20: return "UCHE_PWR";
        case 0x21: return "CP_PWR";
        case 0x22: return "GPMU_PWR";
        case 0x23: return "ALWAYSON_PWR";
        case 0x24: return "GLC";
        case 0x25: return "FCHE";
        case 0x26: return "MHUB";
        case 0x27: return "GMU_XOCLK";
        case 0x28: return "GMU_GMUCLK";
        case 0x29: return "GMU_PERF";
        case 0x2A: return "SW";
        case 0x2B: return "UFC";
        case 0x2C: return "BV_CP";
        case 0x2D: return "BV_PC";
        case 0x2E: return "BV_VFD";
        case 0x2F: return "BV_VPC";
        case 0x30: return "BV_TP";
        case 0x31: return "BV_SP";
        case 0x32: return "BV_UFC";
        case 0x33: return "BV_TSE";
        case 0x34: return "BV_RAS";
        case 0x35: return "BV_LRZ";
        case 0x36: return "BV_HLSQ";
        case 0x37: return "BV_CCU";
        case 0x38: return "BV_RB";
        default: return "UNKNOWN";
    }
}

int main() {
    const char *dev_path = "/dev/kgsl-3d0";

    std::printf("[kgsl_query_probe] Opening %s\n", dev_path);

    int fd = open(dev_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::fprintf(stderr,
                     "[kgsl_query_probe] open failed: errno=%d (%s)\n",
                     errno, std::strerror(errno));
        return 1;
    }

    std::printf("[kgsl_query_probe] open succeeded, fd=%d\n", fd);
    std::printf("[kgsl_query_probe] Sweeping group IDs 0x00 to 0x38\n\n");

    int success_count = 0;
    int fail_count = 0;

    for (unsigned int group = 0; group < KGSL_PERFCOUNTER_GROUP_MAX; group++) {
        unsigned int countables[256];
        std::memset(countables, 0, sizeof(countables));

        kgsl_perfcounter_query q{};
        q.groupid = group;
        q.countables = countables;
        q.count = 256;
        q.max_counters = 0;

        errno = 0;
        int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_QUERY, &q);

        if (ret == 0) {
            success_count++;

            std::printf("[OK]   group=0x%02x %-14s max_counters=%u active_countables=",
                        group, group_name(group), q.max_counters);

            unsigned int to_print = q.max_counters;
            if (to_print > q.count) {
                to_print = q.count;
            }
            if (to_print > 16) {
                to_print = 16;
            }

            std::printf("[");
            for (unsigned int i = 0; i < to_print; i++) {
                if (i != 0) std::printf(", ");
                std::printf("%u", countables[i]);
            }
            if (q.max_counters > to_print) {
                std::printf(", ...");
            }
            std::printf("]\n");
        } else {
            fail_count++;

            std::printf("[FAIL] group=0x%02x %-14s errno=%d (%s)\n",
                        group, group_name(group), errno, std::strerror(errno));
        }
    }

    std::printf("\n[kgsl_query_probe] Summary: success=%d fail=%d\n",
                success_count, fail_count);

    close(fd);
    std::printf("[kgsl_query_probe] Done.\n");
    return 0;
}
