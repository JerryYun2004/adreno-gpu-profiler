#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static const char* vk_result_to_string(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default: return "UNKNOWN_VK_RESULT";
    }
}

static void print_version(uint32_t version) {
    printf("%u.%u.%u",
           VK_VERSION_MAJOR(version),
           VK_VERSION_MINOR(version),
           VK_VERSION_PATCH(version));
}

static bool device_supports_extension(VkPhysicalDevice dev, const char* ext_name) {
    uint32_t ext_count = 0;
    VkResult r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
    if (r != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> exts(ext_count);
    r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());
    if (r != VK_SUCCESS) {
        return false;
    }

    for (const auto& ext : exts) {
        if (std::strcmp(ext.extensionName, ext_name) == 0) {
            return true;
        }
    }

    return false;
}

int main() {
    printf("[vk_probe] Starting Vulkan probe...\n");

    uint32_t loader_version = 0;
    VkResult r = vkEnumerateInstanceVersion(&loader_version);
    if (r == VK_SUCCESS) {
        printf("[vk_probe] Vulkan loader API version: ");
        print_version(loader_version);
        printf("\n");
    } else {
        printf("[vk_probe] vkEnumerateInstanceVersion failed: %s\n", vk_result_to_string(r));
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vk_probe";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "none";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo inst_info{};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo = &app_info;

    VkInstance instance = VK_NULL_HANDLE;
    r = vkCreateInstance(&inst_info, nullptr, &instance);
    if (r != VK_SUCCESS) {
        printf("[vk_probe] vkCreateInstance failed: %s\n", vk_result_to_string(r));
        return 1;
    }

    uint32_t dev_count = 0;
    r = vkEnumeratePhysicalDevices(instance, &dev_count, nullptr);
    if (r != VK_SUCCESS || dev_count == 0) {
        printf("[vk_probe] vkEnumeratePhysicalDevices failed or found no devices: %s, count=%u\n",
               vk_result_to_string(r), dev_count);
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(dev_count);
    r = vkEnumeratePhysicalDevices(instance, &dev_count, devices.data());
    if (r != VK_SUCCESS) {
        printf("[vk_probe] vkEnumeratePhysicalDevices second call failed: %s\n", vk_result_to_string(r));
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    printf("[vk_probe] Physical device count: %u\n", dev_count);

    for (uint32_t i = 0; i < dev_count; i++) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(devices[i], &props);

        bool has_driver_properties =
            device_supports_extension(devices[i], VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME) ||
            VK_VERSION_MAJOR(props.apiVersion) > 1 ||
            (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) >= 2);

        VkPhysicalDeviceDriverProperties driver_props{};
        driver_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = has_driver_properties ? &driver_props : nullptr;

        vkGetPhysicalDeviceProperties2(devices[i], &props2);

        printf("\n[vk_probe] Device %u\n", i);
        printf("  name: %s\n", props.deviceName);
        printf("  vendorID: 0x%04x\n", props.vendorID);
        printf("  deviceID: 0x%08x\n", props.deviceID);
        printf("  apiVersion: ");
        print_version(props.apiVersion);
        printf("\n");
        printf("  driverVersion: 0x%08x\n", props.driverVersion);
        printf("  deviceType: %u\n", props.deviceType);
        printf("  timestampComputeAndGraphics: %u\n", props.limits.timestampComputeAndGraphics);
        printf("  timestampPeriod: %f ns\n", props.limits.timestampPeriod);

        printf("  VK_KHR_driver_properties supported: %s\n",
               has_driver_properties ? "yes" : "no");

        if (has_driver_properties) {
            printf("  driverName: %s\n", driver_props.driverName);
            printf("  driverInfo: %s\n", driver_props.driverInfo);
            printf("  driverID: %u\n", driver_props.driverID);
            printf("  conformanceVersion: %u.%u.%u.%u\n",
                   driver_props.conformanceVersion.major,
                   driver_props.conformanceVersion.minor,
                   driver_props.conformanceVersion.patch,
                   driver_props.conformanceVersion.subminor);
        }

        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &q_count, qprops.data());

        printf("  queue families: %u\n", q_count);
        for (uint32_t q = 0; q < q_count; q++) {
            printf("    queue[%u]: count=%u flags=0x%x",
                   q, qprops[q].queueCount, qprops[q].queueFlags);

            if (qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) printf(" GRAPHICS");
            if (qprops[q].queueFlags & VK_QUEUE_COMPUTE_BIT)  printf(" COMPUTE");
            if (qprops[q].queueFlags & VK_QUEUE_TRANSFER_BIT) printf(" TRANSFER");

            printf("\n");
        }
    }

    vkDestroyInstance(instance, nullptr);
    printf("\n[vk_probe] Done.\n");
    return 0;
}