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

#define KGSL_PERFCOUNTER_GROUP_SP       0x0A
#define KGSL_PERFCOUNTER_GROUP_ALWAYSON 0x1B

struct kgsl_perfcounter_get {
    unsigned int groupid;
    unsigned int countable;
    unsigned int offset;
    unsigned int offset_hi;
    unsigned int __pad;
};

struct kgsl_perfcounter_put {
    unsigned int groupid;
    unsigned int countable;
    unsigned int __pad[2];
};

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

#define IOCTL_KGSL_PERFCOUNTER_GET \
    _IOWR(KGSL_IOC_TYPE, 0x38, struct kgsl_perfcounter_get)

#define IOCTL_KGSL_PERFCOUNTER_PUT \
    _IOW(KGSL_IOC_TYPE, 0x39, struct kgsl_perfcounter_put)

#define IOCTL_KGSL_PERFCOUNTER_READ \
    _IOWR(KGSL_IOC_TYPE, 0x3B, struct kgsl_perfcounter_read)

static void print_errno(const char *step) {
    std::fprintf(stderr,
                 "[kgsl_get_read_probe] %s failed: errno=%d (%s)\n",
                 step, errno, std::strerror(errno));
}

static int do_read(int fd,
                   unsigned int groupid,
                   unsigned int countable,
                   unsigned long long *value_out) {
    kgsl_perfcounter_read_group rg{};
    rg.groupid = groupid;
    rg.countable = countable;
    rg.value = 0;

    kgsl_perfcounter_read req{};
    req.reads = &rg;
    req.count = 1;

    errno = 0;
    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_READ, &req);
    if (ret != 0) {
        std::fprintf(stderr,
                     "[kgsl_get_read_probe] PERFCOUNTER_READ ioctl ret=%d errno=%d (%s)\n",
                     ret, errno, std::strerror(errno));
        return -1;
    }

    *value_out = rg.value;
    return 0;
}

static int do_get(int fd,
                  unsigned int groupid,
                  unsigned int countable) {
    kgsl_perfcounter_get get_req{};
    get_req.groupid = groupid;
    get_req.countable = countable;
    get_req.offset = 0;
    get_req.offset_hi = 0;

    errno = 0;
    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_GET, &get_req);
    if (ret != 0) {
        print_errno("PERFCOUNTER_GET");
        return -1;
    }

    std::printf("[kgsl_get_read_probe] PERFCOUNTER_GET succeeded\n");
    std::printf("[kgsl_get_read_probe] returned offset=0x%x offset_hi=0x%x\n",
                get_req.offset, get_req.offset_hi);

    return 0;
}

static int do_put(int fd,
                  unsigned int groupid,
                  unsigned int countable) {
    kgsl_perfcounter_put put_req{};
    put_req.groupid = groupid;
    put_req.countable = countable;

    errno = 0;
    int ret = ioctl(fd, IOCTL_KGSL_PERFCOUNTER_PUT, &put_req);
    if (ret != 0) {
        print_errno("PERFCOUNTER_PUT");
        return -1;
    }

    std::printf("[kgsl_get_read_probe] PERFCOUNTER_PUT succeeded\n");
    return 0;
}

int main(int argc, char **argv) {
    // Make stdout/stderr ordering easier to read in adb/tee logs.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    const char *dev_path = "/dev/kgsl-3d0";

    unsigned int groupid = KGSL_PERFCOUNTER_GROUP_SP;
    unsigned int countable = 0;
    bool read_only = false;

    if (argc >= 2) {
        groupid = static_cast<unsigned int>(std::strtoul(argv[1], nullptr, 0));
    }

    if (argc >= 3) {
        countable = static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 0));
    }

    if (argc >= 4) {
        if (std::strcmp(argv[3], "read_only") == 0 ||
            std::strcmp(argv[3], "--read-only") == 0) {
            read_only = true;
        } else {
            std::fprintf(stderr,
                         "[kgsl_get_read_probe] Unknown mode: %s\n",
                         argv[3]);
            std::fprintf(stderr,
                         "Usage: %s [groupid] [countable] [read_only]\n",
                         argv[0]);
            return 10;
        }
    }

    std::printf("[kgsl_get_read_probe] Opening %s\n", dev_path);

    int fd = open(dev_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        print_errno("open");
        return 1;
    }

    std::printf("[kgsl_get_read_probe] open succeeded, fd=%d\n", fd);
    std::printf("[kgsl_get_read_probe] Testing group=0x%x countable=%u\n",
                groupid, countable);
    std::printf("[kgsl_get_read_probe] mode=%s\n",
                read_only ? "READ_ONLY" : "GET_READ_PUT");

    // Step 1: Optional GET.
    if (!read_only) {
        if (do_get(fd, groupid, countable) != 0) {
            close(fd);
            return 2;
        }
    } else {
        std::printf("[kgsl_get_read_probe] Skipping PERFCOUNTER_GET\n");
    }

    // Step 2: READ a few samples.
    unsigned long long prev = 0;

    for (int i = 0; i < 10; i++) {
        unsigned long long value = 0;

        int ret = do_read(fd, groupid, countable, &value);
        if (ret != 0) {
            print_errno("PERFCOUNTER_READ");

            // Try to clean up only if we previously did GET.
            if (!read_only) {
                do_put(fd, groupid, countable);
            }

            close(fd);
            return 3;
        }

        if (i == 0) {
            std::printf("[kgsl_get_read_probe] sample=%03d value=%llu delta=N/A\n",
                        i, value);
        } else {
            std::printf("[kgsl_get_read_probe] sample=%03d value=%llu delta=%lld\n",
                        i, value, static_cast<long long>(value - prev));
        }

        prev = value;
        usleep(100000);
    }

    // Step 3: Optional PUT.
    if (!read_only) {
        if (do_put(fd, groupid, countable) != 0) {
            close(fd);
            return 4;
        }
    } else {
        std::printf("[kgsl_get_read_probe] Skipping PERFCOUNTER_PUT\n");
    }

    close(fd);
    std::printf("[kgsl_get_read_probe] Done.\n");
    return 0;
}