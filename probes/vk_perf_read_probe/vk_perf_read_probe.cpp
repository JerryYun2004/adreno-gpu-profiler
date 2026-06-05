#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <limits>

#define CHECK_VK(expr, msg) do {                                      \
    VkResult _r = (expr);                                             \
    if (_r != VK_SUCCESS) {                                           \
        std::fprintf(stderr, "[vk_perf_read_probe] %s failed: VkResult=%d\n", msg, _r); \
        return 1;                                                     \
    }                                                                 \
} while (0)

static uint32_t find_memory_type(VkPhysicalDevice phys,
                                 uint32_t type_bits,
                                 VkMemoryPropertyFlags wanted)
{
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) &&
            ((mem_props.memoryTypes[i].propertyFlags & wanted) == wanted)) {
            return i;
        }
    }

    // Fallback: any compatible memory type.
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if (type_bits & (1u << i)) {
            return i;
        }
    }

    return UINT32_MAX;
}

int main(int argc, char **argv)
{
    uint32_t counter_index = 3;     // Default: PERF_CP_BUSY_CYCLES
    uint32_t fill_repeats = 512;    // More repeats = larger command-buffer workload
    uint32_t buffer_mb = 64;

    if (argc >= 2) counter_index = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));
    if (argc >= 3) fill_repeats = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
    if (argc >= 4) buffer_mb = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));

    std::printf("[vk_perf_read_probe] Starting\n");
    std::printf("[vk_perf_read_probe] counter_index=%u fill_repeats=%u buffer_mb=%u\n",
                counter_index, fill_repeats, buffer_mb);

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "vk_perf_read_probe";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;

    VkInstance instance = VK_NULL_HANDLE;
    CHECK_VK(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");

    uint32_t gpu_count = 0;
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr),
             "vkEnumeratePhysicalDevices count");

    if (gpu_count == 0) {
        std::fprintf(stderr, "[vk_perf_read_probe] No Vulkan physical devices\n");
        return 1;
    }

    std::vector<VkPhysicalDevice> gpus(gpu_count);
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data()),
             "vkEnumeratePhysicalDevices list");

    VkPhysicalDevice phys = gpus[0];

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);

    std::printf("[vk_perf_read_probe] Device: %s\n", props.deviceName);
    std::printf("[vk_perf_read_probe] vendorID=0x%04x deviceID=0x%08x api=%u.%u.%u\n",
                props.vendorID, props.deviceID,
                VK_VERSION_MAJOR(props.apiVersion),
                VK_VERSION_MINOR(props.apiVersion),
                VK_VERSION_PATCH(props.apiVersion));

    uint32_t q_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &q_count, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(q_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &q_count, qprops.data());

    uint32_t qfam = UINT32_MAX;
    for (uint32_t i = 0; i < q_count; i++) {
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ||
            (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            qfam = i;
            break;
        }
    }

    if (qfam == UINT32_MAX) {
        std::fprintf(stderr, "[vk_perf_read_probe] No usable queue family\n");
        return 1;
    }

    std::printf("[vk_perf_read_probe] Using queue family %u flags=0x%x\n",
                qfam, qprops[qfam].queueFlags);

    VkPhysicalDevicePerformanceQueryFeaturesKHR perf_features{};
    perf_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &perf_features;

    vkGetPhysicalDeviceFeatures2(phys, &features2);

    std::printf("[vk_perf_read_probe] performanceCounterQueryPools supported: %u\n",
                perf_features.performanceCounterQueryPools);
    std::printf("[vk_perf_read_probe] performanceCounterMultipleQueryPools supported: %u\n",
                perf_features.performanceCounterMultipleQueryPools);

    if (!perf_features.performanceCounterQueryPools) {
        std::fprintf(stderr, "[vk_perf_read_probe] performanceCounterQueryPools not supported\n");
        return 1;
    }

    perf_features.performanceCounterQueryPools = VK_TRUE;
    // Keep this false unless the driver requires/advertises it.
    perf_features.performanceCounterMultipleQueryPools = VK_FALSE;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = qfam;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    const char *dev_exts[] = {
        VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,
    };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &perf_features;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dev_exts;

    VkDevice device = VK_NULL_HANDLE;
    CHECK_VK(vkCreateDevice(phys, &dci, nullptr, &device), "vkCreateDevice");

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, qfam, 0, &queue);

    auto pfnAcquireProfilingLock =
        (PFN_vkAcquireProfilingLockKHR)vkGetDeviceProcAddr(device, "vkAcquireProfilingLockKHR");
    auto pfnReleaseProfilingLock =
        (PFN_vkReleaseProfilingLockKHR)vkGetDeviceProcAddr(device, "vkReleaseProfilingLockKHR");

    if (!pfnAcquireProfilingLock || !pfnReleaseProfilingLock) {
        std::fprintf(stderr, "[vk_perf_read_probe] Profiling lock functions not found\n");
        return 1;
    }

    VkAcquireProfilingLockInfoKHR lock_info{};
    lock_info.sType = VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR;
    lock_info.timeout = std::numeric_limits<uint64_t>::max();

    CHECK_VK(pfnAcquireProfilingLock(device, &lock_info), "vkAcquireProfilingLockKHR");
    std::printf("[vk_perf_read_probe] Acquired profiling lock\n");

    VkQueryPoolPerformanceCreateInfoKHR perf_qp_info{};
    perf_qp_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR;
    perf_qp_info.queueFamilyIndex = qfam;
    perf_qp_info.counterIndexCount = 1;
    perf_qp_info.pCounterIndices = &counter_index;

    VkQueryPoolCreateInfo qpci{};
    qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpci.pNext = &perf_qp_info;
    qpci.queryType = VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR;
    qpci.queryCount = 1;

    VkQueryPool query_pool = VK_NULL_HANDLE;
    CHECK_VK(vkCreateQueryPool(device, &qpci, nullptr, &query_pool), "vkCreateQueryPool");
    std::printf("[vk_perf_read_probe] Created performance query pool\n");

    VkDeviceSize buffer_size = VkDeviceSize(buffer_mb) * 1024ull * 1024ull;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = buffer_size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    CHECK_VK(vkCreateBuffer(device, &bci, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements mem_req{};
    vkGetBufferMemoryRequirements(device, buffer, &mem_req);

    uint32_t mem_type = find_memory_type(
        phys,
        mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (mem_type == UINT32_MAX) {
        std::fprintf(stderr, "[vk_perf_read_probe] No compatible memory type\n");
        return 1;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mem_req.size;
    mai.memoryTypeIndex = mem_type;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    CHECK_VK(vkAllocateMemory(device, &mai, nullptr, &memory), "vkAllocateMemory");
    CHECK_VK(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = qfam;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    CHECK_VK(vkCreateCommandPool(device, &cpci, nullptr, &cmd_pool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = cmd_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    CHECK_VK(vkAllocateCommandBuffers(device, &cbai, &cmd), "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo cbi{};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    CHECK_VK(vkBeginCommandBuffer(cmd, &cbi), "vkBeginCommandBuffer");

    vkCmdResetQueryPool(cmd, query_pool, 0, 1);
    vkCmdBeginQuery(cmd, query_pool, 0, 0);

    for (uint32_t i = 0; i < fill_repeats; i++) {
        vkCmdFillBuffer(cmd, buffer, 0, buffer_size, 0x12340000u + i);
    }

    vkCmdEndQuery(cmd, query_pool, 0);

    CHECK_VK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    VkPerformanceQuerySubmitInfoKHR perf_submit{};
    perf_submit.sType = VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;
    perf_submit.pNext = nullptr;
    perf_submit.counterPassIndex = 0;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &perf_submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    CHECK_VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    CHECK_VK(vkQueueWaitIdle(queue), "vkQueueWaitIdle");

    std::printf("[vk_perf_read_probe] Work submitted and completed\n");

    struct ResultWithAvailability {
        VkPerformanceCounterResultKHR value;
        uint64_t availability;
    };

    VkPerformanceCounterResultKHR result_wait{};
    VkResult qr_wait = vkGetQueryPoolResults(
        device,
        query_pool,
        0,
        1,
        sizeof(result_wait),
        &result_wait,
        sizeof(result_wait),
        VK_QUERY_RESULT_WAIT_BIT);

    std::printf("[vk_perf_read_probe] Read mode WAIT: VkResult=%d value=%llu\n",
                qr_wait, (unsigned long long)result_wait.uint64);

    VkPerformanceCounterResultKHR result_partial{};
    VkResult qr_partial = vkGetQueryPoolResults(
        device,
        query_pool,
        0,
        1,
        sizeof(result_partial),
        &result_partial,
        sizeof(result_partial),
        VK_QUERY_RESULT_PARTIAL_BIT);

    std::printf("[vk_perf_read_probe] Read mode PARTIAL: VkResult=%d value=%llu\n",
                qr_partial, (unsigned long long)result_partial.uint64);

    ResultWithAvailability result_avail{};
    VkResult qr_avail = vkGetQueryPoolResults(
        device,
        query_pool,
        0,
        1,
        sizeof(result_avail),
        &result_avail,
        sizeof(result_avail),
        VK_QUERY_RESULT_PARTIAL_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

    std::printf("[vk_perf_read_probe] Read mode PARTIAL+AVAIL: VkResult=%d value=%llu availability=%llu\n",
                qr_avail,
                (unsigned long long)result_avail.value.uint64,
                (unsigned long long)result_avail.availability);

    VkResult qr = qr_wait;

    vkDeviceWaitIdle(device);

    vkDestroyCommandPool(device, cmd_pool, nullptr);
    vkFreeMemory(device, memory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
    vkDestroyQueryPool(device, query_pool, nullptr);

    pfnReleaseProfilingLock(device);
    std::printf("[vk_perf_read_probe] Released profiling lock\n");

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    std::printf("[vk_perf_read_probe] Done\n");

    return qr == VK_SUCCESS ? 0 : 1;
}
