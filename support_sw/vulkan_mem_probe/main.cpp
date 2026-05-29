#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#define VK_CHECK(x) do { \
    VkResult err = (x); \
    if (err != VK_SUCCESS) { \
        std::fprintf(stderr, "[vk_mem_probe] Vulkan error %d at %s:%d: %s\n", \
                     err, __FILE__, __LINE__, #x); \
        std::exit(1); \
    } \
} while (0)

struct PushConstants {
    uint32_t n;
    uint32_t iters;
};

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

static std::vector<uint32_t> read_spv(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "[vk_mem_probe] Failed to open SPIR-V file: %s\n", path);
        std::exit(1);
    }

    std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        std::fprintf(stderr, "[vk_mem_probe] Invalid SPIR-V size\n");
        std::exit(1);
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint32_t> data(size / 4);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::fprintf(stderr, "[vk_mem_probe] Failed to read SPIR-V file\n");
        std::exit(1);
    }

    return data;
}

static uint32_t cpu_reference(uint32_t input, uint32_t idx, uint32_t iters) {
    uint32_t x = input ^ (idx * 747796405u + 2891336453u);

    for (uint32_t i = 0; i < iters; i++) {
        x = x * 1664525u + 1013904223u;
        x ^= x >> 16;
        x *= 2246822519u;
        x ^= x >> 13;
        x *= 3266489917u;
        x ^= x >> 16;
        x += i ^ idx;
    }

    return x;
}

static uint32_t cpu_mem_reference(const uint32_t* in,
                                  uint32_t idx,
                                  uint32_t elements,
                                  uint32_t iters) {
    uint32_t n = elements;
    uint32_t x = in[idx];

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t j0 = (idx + i * 17u   + 1u)  & (n - 1u);
        uint32_t j1 = (idx + i * 67u   + 13u) & (n - 1u);
        uint32_t j2 = (idx + i * 257u  + 29u) & (n - 1u);
        uint32_t j3 = (idx + i * 1021u + 53u) & (n - 1u);

        uint32_t a = in[j0];
        uint32_t b = in[j1];
        uint32_t c = in[j2];
        uint32_t d = in[j3];

        x ^= a;
        x += b;
        x ^= c;
        x += d;
    }

    return x;
}

static uint32_t find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_bits,
    VkMemoryPropertyFlags required_flags
) {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) &&
            ((mem_props.memoryTypes[i].propertyFlags & required_flags) == required_flags)) {
            return i;
        }
    }

    std::fprintf(stderr, "[vk_mem_probe] Failed to find suitable memory type\n");
    std::exit(1);
}

static Buffer create_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage
) {
    Buffer b{};
    b.size = size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &b.buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, b.buffer, &req);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        physical_device,
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &b.memory));
    VK_CHECK(vkBindBufferMemory(device, b.buffer, b.memory, 0));
    VK_CHECK(vkMapMemory(device, b.memory, 0, size, 0, &b.mapped));

    return b;
}

static void destroy_buffer(VkDevice device, Buffer& b) {
    if (b.mapped) {
        vkUnmapMemory(device, b.memory);
        b.mapped = nullptr;
    }
    if (b.buffer) {
        vkDestroyBuffer(device, b.buffer, nullptr);
        b.buffer = VK_NULL_HANDLE;
    }
    if (b.memory) {
        vkFreeMemory(device, b.memory, nullptr);
        b.memory = VK_NULL_HANDLE;
    }
}

int main(int argc, char** argv) {
    const char* spv_path = "/data/local/tmp/jerry_work/mem.comp.spv";
    uint32_t n = 1u << 18;
    uint32_t alu_iters = 512;
    uint32_t dispatch_repeats = 64;

    if (argc >= 2) spv_path = argv[1];
    if (argc >= 3) n = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
    if (argc >= 4) alu_iters = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
    if (argc >= 5) dispatch_repeats = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));

    std::printf("[vk_mem_probe] Starting Vulkan compute workload\n");
    std::printf("[vk_mem_probe] SPIR-V: %s\n", spv_path);
    std::printf("[vk_mem_probe] elements=%u alu_iters=%u dispatch_repeats=%u\n",
                n, alu_iters, dispatch_repeats);

    std::vector<uint32_t> spv = read_spv(spv_path);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vk_mem_probe";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "none";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));

    uint32_t device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
    if (device_count == 0) {
        std::fprintf(stderr, "[vk_mem_probe] No Vulkan physical devices found\n");
        return 1;
    }

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()));

    VkPhysicalDevice physical_device = physical_devices[0];

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device, &props);
    std::printf("[vk_mem_probe] Using device: %s\n", props.deviceName);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_props(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_props.data());

    uint32_t queue_family = UINT32_MAX;

    for (uint32_t i = 0; i < queue_family_count; i++) {
        bool compute = queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        bool graphics = queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        if (compute && !graphics) {
            queue_family = i;
            break;
        }
    }

    if (queue_family == UINT32_MAX) {
        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queue_family = i;
                break;
            }
        }
    }

    if (queue_family == UINT32_MAX) {
        std::fprintf(stderr, "[vk_mem_probe] No compute queue found\n");
        return 1;
    }

    std::printf("[vk_mem_probe] Using queue family %u, flags=0x%x\n",
                queue_family, queue_props[queue_family].queueFlags);

    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device));

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_family, 0, &queue);

    VkDeviceSize buffer_size = static_cast<VkDeviceSize>(n) * sizeof(uint32_t);

    Buffer input = create_buffer(
        physical_device,
        device,
        buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    Buffer output = create_buffer(
        physical_device,
        device,
        buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    uint32_t* in = reinterpret_cast<uint32_t*>(input.mapped);
    uint32_t* out = reinterpret_cast<uint32_t*>(output.mapped);

    for (uint32_t i = 0; i < n; i++) {
        in[i] = i * 17u + 123u;
        out[i] = 0;
    }

    VkShaderModuleCreateInfo shader_info{};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = spv.size() * sizeof(uint32_t);
    shader_info.pCode = spv.data();

    VkShaderModule shader = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &shader_info, nullptr, &shader));

    VkDescriptorSetLayoutBinding bindings[2]{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsl_info{};
    dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_info.bindingCount = 2;
    dsl_info.pBindings = bindings;

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &dsl_info, nullptr, &descriptor_set_layout));

    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_range;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = shader;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = stage_info;
    pipeline_info.layout = pipeline_layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 2;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool));

    VkDescriptorSetAllocateInfo desc_alloc{};
    desc_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc_alloc.descriptorPool = descriptor_pool;
    desc_alloc.descriptorSetCount = 1;
    desc_alloc.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device, &desc_alloc, &descriptor_set));

    VkDescriptorBufferInfo in_desc{};
    in_desc.buffer = input.buffer;
    in_desc.offset = 0;
    in_desc.range = buffer_size;

    VkDescriptorBufferInfo out_desc{};
    out_desc.buffer = output.buffer;
    out_desc.offset = 0;
    out_desc.range = buffer_size;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &in_desc;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &out_desc;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    VkCommandPoolCreateInfo cmd_pool_info{};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = queue_family;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &cmd_pool_info, nullptr, &cmd_pool));

    VkCommandBufferAllocateInfo cmd_alloc{};
    cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc.commandPool = cmd_pool;
    cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd));

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_layout,
        0,
        1,
        &descriptor_set,
        0,
        nullptr
    );

    PushConstants pc{};
    pc.n = n;
    pc.iters = alu_iters;

    vkCmdPushConstants(
        cmd,
        pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConstants),
        &pc
    );

    uint32_t groups = (n + 255u) / 256u;

    for (uint32_t r = 0; r < dispatch_repeats; r++) {
        vkCmdDispatch(cmd, groups, 1, 1);
    }

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1,
        &barrier,
        0,
        nullptr,
        0,
        nullptr
    );

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    std::printf("[vk_mem_probe] Submitting workload...\n");
    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    std::printf("[vk_mem_probe] Workload complete.\n");

    std::printf("[vk_mem_probe] Verifying output...\n");

    bool ok = true;
    uint32_t check_count = std::min<uint32_t>(n, 1024);

    for (uint32_t i = 0; i < check_count; i++) {
        uint32_t expected = cpu_mem_reference(in, i, n, alu_iters);
        if (out[i] != expected) {
            std::printf("[vk_mem_probe] MISMATCH at %u: got=0x%08x expected=0x%08x\n",
                        i, out[i], expected);
            ok = false;
            break;
        }
    }

    if (ok) {
        std::printf("[vk_mem_probe] Verification PASSED for first %u elements.\n", check_count);
    } else {
        std::printf("[vk_mem_probe] Verification FAILED.\n");
    }

    vkDestroyCommandPool(device, cmd_pool, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);

    destroy_buffer(device, output);
    destroy_buffer(device, input);

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    std::printf("[vk_mem_probe] Done.\n");
    return ok ? 0 : 1;
}
