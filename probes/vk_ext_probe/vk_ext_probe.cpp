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

int main() {
    std::printf("[vk_ext_probe] Starting\n");

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "vk_ext_probe";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;

    VkInstance instance = VK_NULL_HANDLE;
    CHECK_VK(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");

    uint32_t gpu_count = 0;
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr),
             "vkEnumeratePhysicalDevices count");

    std::printf("[vk_ext_probe] Physical device count: %u\n", gpu_count);

    if (gpu_count == 0) {
        std::fprintf(stderr, "[vk_ext_probe] No Vulkan devices found\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<VkPhysicalDevice> gpus(gpu_count);
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data()),
             "vkEnumeratePhysicalDevices list");

    for (uint32_t i = 0; i < gpu_count; i++) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpus[i], &props);

        std::printf("\n[vk_ext_probe] Device %u\n", i);
        std::printf("  name: %s\n", props.deviceName);
        std::printf("  vendorID: 0x%04x\n", props.vendorID);
        std::printf("  deviceID: 0x%08x\n", props.deviceID);
        std::printf("  apiVersion: %u.%u.%u\n",
                    VK_VERSION_MAJOR(props.apiVersion),
                    VK_VERSION_MINOR(props.apiVersion),
                    VK_VERSION_PATCH(props.apiVersion));

        uint32_t ext_count = 0;
        CHECK_VK(vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &ext_count, nullptr),
                 "vkEnumerateDeviceExtensionProperties count");

        std::vector<VkExtensionProperties> exts(ext_count);
        CHECK_VK(vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &ext_count, exts.data()),
                 "vkEnumerateDeviceExtensionProperties list");

        bool has_perf_query = false;

        std::printf("  device extensions: %u\n", ext_count);
        for (const auto &e : exts) {
            std::printf("    %s specVersion=%u\n", e.extensionName, e.specVersion);
            if (std::strcmp(e.extensionName, "VK_KHR_performance_query") == 0) {
                has_perf_query = true;
            }
        }

        std::printf("\n  VK_KHR_performance_query: %s\n",
                    has_perf_query ? "YES" : "NO");
    }

    vkDestroyInstance(instance, nullptr);
    std::printf("[vk_ext_probe] Done\n");
    return 0;
}
