#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// From Mesa/Freedreno msm_kgsl.h.
// Confirmed in third_party/mesa/src/freedreno/vulkan/msm_kgsl.h.
#define KGSL_IOC_TYPE 0x09

#define KGSL_PERFCOUNTER_GROUP_ALWAYSON 0x1B

struct kgsl_perfcounter_read_group {
    unsigned int groupid;
    unsigned int countable;
    unsigned long long value;
};

struct kgsl_perfcounter_read {
    struct kgsl_perfcounter_read_group *reads;
    unsigned int count;
    unsigned int __pad[2];
};

#define IOCTL_KGSL_PERFCOUNTER_READ \
    _IOWR(KGSL_IOC_TYPE, 0x3B, struct kgsl_perfcounter_read)

static int read_alwayson_counter(int fd, unsigned long long *value_out) {
    kgsl_perfcounter_read_group group{};
    group.groupid = KGSL_PERFCOUNTER_GROUP_ALWAYSON;
    group.countable = 0;
    group.value = 0;

    kgsl_perfcounter_read req{};
    req.reads = &group;
    req.count = 1;

    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_READ, &req);
    if (ret != 0) {
        return -1;
    }

    *value_out = group.value;
    return 0;
}

int main(int argc, char **argv) {
    const char *dev_path = "/dev/kgsl-3d0";
    int samples = 20;
    int sleep_us = 100000; // 100 ms

    if (argc >= 2) {
        samples = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        sleep_us = std::atoi(argv[2]);
    }

    std::printf("[kgsl_alwayson_probe] Opening %s\n", dev_path);

    int fd = open(dev_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::fprintf(stderr,
                     "[kgsl_alwayson_probe] open failed: errno=%d (%s)\n",
                     errno, std::strerror(errno));
        return 1;
    }

    std::printf("[kgsl_alwayson_probe] open succeeded, fd=%d\n", fd);
    std::printf("[kgsl_alwayson_probe] Reading ALWAYSON group=0x%x countable=0\n",
                KGSL_PERFCOUNTER_GROUP_ALWAYSON);

    unsigned long long prev = 0;

    for (int i = 0; i < samples; i++) {
        unsigned long long value = 0;

        int ret = read_alwayson_counter(fd, &value);
        if (ret != 0) {
            std::fprintf(stderr,
                         "[kgsl_alwayson_probe] ioctl READ failed at sample %d: errno=%d (%s)\n",
                         i, errno, std::strerror(errno));
            close(fd);
            return 2;
        }

        if (i == 0) {
            std::printf("[kgsl_alwayson_probe] sample=%03d value=%llu delta=N/A\n",
                        i, value);
        } else {
            std::printf("[kgsl_alwayson_probe] sample=%03d value=%llu delta=%lld\n",
                        i, value, (long long)(value - prev));
        }

        prev = value;
        usleep(sleep_us);
    }

    close(fd);
    std::printf("[kgsl_alwayson_probe] Done.\n");
    return 0;
}
