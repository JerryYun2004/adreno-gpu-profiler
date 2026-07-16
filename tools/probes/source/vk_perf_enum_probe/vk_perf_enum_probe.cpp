#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK_VK(x, msg) do { \
    VkResult r = (x); \
    if (r != VK_SUCCESS) { \
        std::fprintf(stderr, "%s failed: VkResult=%d\n", msg, r); \
        return 1; \
    } \
} while (0)

static const char* storage_to_str(VkPerformanceCounterStorageKHR s) {
    switch (s) {
        case VK_PERFORMANCE_COUNTER_STORAGE_INT32_KHR: return "INT32";
        case VK_PERFORMANCE_COUNTER_STORAGE_INT64_KHR: return "INT64";
        case VK_PERFORMANCE_COUNTER_STORAGE_UINT32_KHR: return "UINT32";
        case VK_PERFORMANCE_COUNTER_STORAGE_UINT64_KHR: return "UINT64";
        case VK_PERFORMANCE_COUNTER_STORAGE_FLOAT32_KHR: return "FLOAT32";
        case VK_PERFORMANCE_COUNTER_STORAGE_FLOAT64_KHR: return "FLOAT64";
        default: return "UNKNOWN";
    }
}

static const char* unit_to_str(VkPerformanceCounterUnitKHR u) {
    switch (u) {
        case VK_PERFORMANCE_COUNTER_UNIT_GENERIC_KHR: return "GENERIC";
        case VK_PERFORMANCE_COUNTER_UNIT_PERCENTAGE_KHR: return "PERCENTAGE";
        case VK_PERFORMANCE_COUNTER_UNIT_NANOSECONDS_KHR: return "NANOSECONDS";
        case VK_PERFORMANCE_COUNTER_UNIT_BYTES_KHR: return "BYTES";
        case VK_PERFORMANCE_COUNTER_UNIT_BYTES_PER_SECOND_KHR: return "BYTES_PER_SECOND";
        case VK_PERFORMANCE_COUNTER_UNIT_KELVIN_KHR: return "KELVIN";
        case VK_PERFORMANCE_COUNTER_UNIT_WATTS_KHR: return "WATTS";
        case VK_PERFORMANCE_COUNTER_UNIT_VOLTS_KHR: return "VOLTS";
        case VK_PERFORMANCE_COUNTER_UNIT_AMPS_KHR: return "AMPS";
        case VK_PERFORMANCE_COUNTER_UNIT_HERTZ_KHR: return "HERTZ";
        case VK_PERFORMANCE_COUNTER_UNIT_CYCLES_KHR: return "CYCLES";
        default: return "UNKNOWN";
    }
}

static const char* scope_to_str(VkPerformanceCounterScopeKHR s) {
    switch (s) {
        case VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_BUFFER_KHR: return "COMMAND_BUFFER";
        case VK_PERFORMANCE_COUNTER_SCOPE_RENDER_PASS_KHR: return "RENDER_PASS";
        case VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_KHR: return "COMMAND";
        default: return "UNKNOWN";
    }
}

int main() {
    std::printf("[vk_perf_enum_probe] Starting\n");

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "vk_perf_enum_probe";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;

    VkInstance instance = VK_NULL_HANDLE;
    CHECK_VK(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");

    auto pfnEnumCounters =
        (PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)
        vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");

    auto pfnGetPasses =
        (PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");

    if (!pfnEnumCounters || !pfnGetPasses) {
        std::fprintf(stderr, "[vk_perf_enum_probe] Required KHR performance query functions not found\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    uint32_t gpu_count = 0;
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr),
             "vkEnumeratePhysicalDevices count");

    if (gpu_count == 0) {
        std::fprintf(stderr, "[vk_perf_enum_probe] No Vulkan devices found\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<VkPhysicalDevice> gpus(gpu_count);
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data()),
             "vkEnumeratePhysicalDevices list");

    for (uint32_t dev_i = 0; dev_i < gpu_count; dev_i++) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpus[dev_i], &props);

        std::printf("\n[vk_perf_enum_probe] Device %u\n", dev_i);
        std::printf("  name: %s\n", props.deviceName);
        std::printf("  vendorID: 0x%04x\n", props.vendorID);
        std::printf("  deviceID: 0x%08x\n", props.deviceID);

        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[dev_i], &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[dev_i], &q_count, qprops.data());

        std::printf("  queue families: %u\n", q_count);

        for (uint32_t q = 0; q < q_count; q++) {
            std::printf("\n  [queue family %u]\n", q);
            std::printf("    flags=0x%x count=%u\n", qprops[q].queueFlags, qprops[q].queueCount);

            uint32_t counter_count = 0;
            VkResult r = pfnEnumCounters(gpus[dev_i], q, &counter_count, nullptr, nullptr);

            if (r != VK_SUCCESS) {
                std::printf("    enumerate counter count failed: VkResult=%d\n", r);
                continue;
            }

            std::printf("    performance counters: %u\n", counter_count);

            if (counter_count == 0) {
                continue;
            }

            std::vector<VkPerformanceCounterKHR> counters(counter_count);
            std::vector<VkPerformanceCounterDescriptionKHR> descs(counter_count);

            for (uint32_t i = 0; i < counter_count; i++) {
                counters[i].sType = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR;
                counters[i].pNext = nullptr;

                descs[i].sType = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR;
                descs[i].pNext = nullptr;
            }

            r = pfnEnumCounters(gpus[dev_i], q, &counter_count,
                                counters.data(), descs.data());

            if (r != VK_SUCCESS) {
                std::printf("    enumerate counter list failed: VkResult=%d\n", r);
                continue;
            }

            VkQueryPoolPerformanceCreateInfoKHR perf_info{};
            perf_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR;
            perf_info.pNext = nullptr;
            perf_info.queueFamilyIndex = q;
            perf_info.counterIndexCount = counter_count;
            std::vector<uint32_t> indices(counter_count);
            for (uint32_t i = 0; i < counter_count; i++) indices[i] = i;
            perf_info.pCounterIndices = indices.data();

            uint32_t passes = 0;
            pfnGetPasses(gpus[dev_i], &perf_info, &passes);
            std::printf("    passes needed for all counters: %u\n", passes);

            for (uint32_t i = 0; i < counter_count; i++) {
                std::printf("\n    counter[%u]\n", i);
                std::printf("      name:        %s\n", descs[i].name);
                std::printf("      category:    %s\n", descs[i].category);
                std::printf("      description: %s\n", descs[i].description);
                std::printf("      unit:        %s\n", unit_to_str(counters[i].unit));
                std::printf("      scope:       %s\n", scope_to_str(counters[i].scope));
                std::printf("      storage:     %s\n", storage_to_str(counters[i].storage));
                std::printf("      uuid:        ");
                for (int b = 0; b < VK_UUID_SIZE; b++) {
                    std::printf("%02x", counters[i].uuid[b]);
                }
                std::printf("\n");
            }
        }
    }

    vkDestroyInstance(instance, nullptr);
    std::printf("\n[vk_perf_enum_probe] Done\n");
    return 0;
}
