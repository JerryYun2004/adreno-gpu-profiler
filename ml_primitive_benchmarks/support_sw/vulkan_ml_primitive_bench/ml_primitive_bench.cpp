#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#define VK_CHECK(x) do { VkResult err__ = (x); if (err__ != VK_SUCCESS) { throw std::runtime_error(std::string("Vulkan error ") + std::to_string(err__) + " at " + __FILE__ + ":" + std::to_string(__LINE__)); } } while (0)

struct Args {
    std::string op = "softmax";        // softmax or rmsnorm
    std::string variant = "three_pass"; // three_pass, fused_lmem, online, rmsnorm_basic
    std::string spv_path;
    uint32_t width = 256;
    uint32_t rows = 65536;
    uint32_t repeats = 128;
    bool verify = false;
    bool csv = false;
    float epsilon = 1.0e-5f;
};

struct PushConstants {
    uint32_t width;
    uint32_t rows;
    uint32_t elements;
    uint32_t flags;
    float epsilon;
};

static void usage(const char* argv0) {
    std::cerr <<
        "Usage:\n"
        "  " << argv0 << " --op softmax --variant three_pass --spv softmax_three_pass.spv --width 256 --rows 65536 --repeats 128 --verify\n"
        "  " << argv0 << " --op rmsnorm --variant basic --spv rmsnorm_basic.spv --width 256 --rows 65536 --repeats 128 --verify\n\n"
        "Options:\n"
        "  --op softmax|rmsnorm\n"
        "  --variant three_pass|fused_lmem|online|basic\n"
        "  --spv <path>\n"
        "  --width <row/reduction width>\n"
        "  --rows <number of rows>\n"
        "  --elements <total elements>; overrides --rows with rows=elements/width\n"
        "  --repeats <dispatch repetitions inside one timed region>\n"
        "  --epsilon <RMSNorm epsilon>\n"
        "  --verify\n"
        "  --csv  Print one CSV-style result row\n";
}

static Args parse_args(int argc, char** argv) {
    Args a;
    uint64_t total_elements_override = 0;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value for ") + name);
            return argv[++i];
        };
        if (s == "--op") a.op = need("--op");
        else if (s == "--variant") a.variant = need("--variant");
        else if (s == "--spv") a.spv_path = need("--spv");
        else if (s == "--width") a.width = static_cast<uint32_t>(std::stoul(need("--width")));
        else if (s == "--rows") a.rows = static_cast<uint32_t>(std::stoul(need("--rows")));
        else if (s == "--elements") total_elements_override = std::stoull(need("--elements"));
        else if (s == "--repeats") a.repeats = static_cast<uint32_t>(std::stoul(need("--repeats")));
        else if (s == "--epsilon") a.epsilon = std::stof(need("--epsilon"));
        else if (s == "--verify") a.verify = true;
        else if (s == "--csv") a.csv = true;
        else if (s == "--help" || s == "-h") { usage(argv[0]); std::exit(0); }
        else throw std::runtime_error("Unknown argument: " + s);
    }
    if (a.spv_path.empty()) throw std::runtime_error("Missing --spv <shader.spv>");
    if (a.width == 0 || a.rows == 0 || a.repeats == 0) throw std::runtime_error("width, rows, and repeats must be non-zero");
    if (total_elements_override) {
        a.rows = static_cast<uint32_t>(total_elements_override / a.width);
        if (a.rows == 0) throw std::runtime_error("--elements must be >= --width");
    }
    uint64_t elems = uint64_t(a.width) * uint64_t(a.rows);
    if (elems > 0x7fffffffull) throw std::runtime_error("This simple benchmark limits element count to < 2^31 floats");
    return a;
}

static std::vector<uint32_t> read_spv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open SPIR-V file: " + path);
    std::streamsize size = f.tellg();
    if (size <= 0 || (size % 4) != 0) throw std::runtime_error("Invalid SPIR-V file size: " + path);
    f.seekg(0, std::ios::beg);
    std::vector<uint32_t> data(size / 4);
    if (!f.read(reinterpret_cast<char*>(data.data()), size)) throw std::runtime_error("Cannot read SPIR-V file: " + path);
    return data;
}

static uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_bits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) && ((mem_props.memoryTypes[i].propertyFlags & props) == props)) return i;
    }
    throw std::runtime_error("No matching Vulkan memory type");
}

struct Buffer {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;

    void destroy() {
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

static Buffer make_buffer(VkPhysicalDevice phys, VkDevice dev, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
    Buffer b;
    b.device = dev;
    b.size = size;

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &bi, nullptr, &b.buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, b.buffer, &req);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(phys, req.memoryTypeBits, props);
    VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &b.memory));
    VK_CHECK(vkBindBufferMemory(dev, b.buffer, b.memory, 0));
    return b;
}

static void upload(VkDevice dev, const Buffer& b, const void* data, size_t bytes) {
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(dev, b.memory, 0, bytes, 0, &mapped));
    std::memcpy(mapped, data, bytes);
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = b.memory;
    range.offset = 0;
    range.size = bytes;
    vkFlushMappedMemoryRanges(dev, 1, &range);
    vkUnmapMemory(dev, b.memory);
}

static void download(VkDevice dev, const Buffer& b, void* data, size_t bytes) {
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(dev, b.memory, 0, bytes, 0, &mapped));
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = b.memory;
    range.offset = 0;
    range.size = bytes;
    vkInvalidateMappedMemoryRanges(dev, 1, &range);
    std::memcpy(data, mapped, bytes);
    vkUnmapMemory(dev, b.memory);
}

static void cpu_softmax(const std::vector<float>& x, std::vector<float>& y, uint32_t rows, uint32_t width) {
    for (uint32_t r = 0; r < rows; ++r) {
        const uint32_t base = r * width;
        float m = -std::numeric_limits<float>::infinity();
        for (uint32_t i = 0; i < width; ++i) m = std::max(m, x[base + i]);
        double sum = 0.0;
        for (uint32_t i = 0; i < width; ++i) sum += std::exp(double(x[base + i] - m));
        for (uint32_t i = 0; i < width; ++i) y[base + i] = float(std::exp(double(x[base + i] - m)) / sum);
    }
}

static void cpu_rmsnorm(const std::vector<float>& x, const std::vector<float>& w, std::vector<float>& y, uint32_t rows, uint32_t width, float eps) {
    for (uint32_t r = 0; r < rows; ++r) {
        const uint32_t base = r * width;
        double ss = 0.0;
        for (uint32_t i = 0; i < width; ++i) ss += double(x[base + i]) * double(x[base + i]);
        float inv = float(1.0 / std::sqrt(ss / double(width) + eps));
        for (uint32_t i = 0; i < width; ++i) y[base + i] = x[base + i] * w[i] * inv;
    }
}

static bool verify_result(const Args& a, const std::vector<float>& x, const std::vector<float>& w, const std::vector<float>& got) {
    // Full CPU reference for very large sweeps can dominate runtime. Verify up to 256 rows.
    uint32_t check_rows = std::min<uint32_t>(a.rows, 256u);
    std::vector<float> ref(size_t(check_rows) * a.width);
    std::vector<float> x_small(x.begin(), x.begin() + size_t(check_rows) * a.width);

    if (a.op == "softmax") {
        cpu_softmax(x_small, ref, check_rows, a.width);
    } else {
        cpu_rmsnorm(x_small, w, ref, check_rows, a.width, a.epsilon);
    }

    double max_abs = 0.0;
    double max_rel = 0.0;
    for (size_t i = 0; i < ref.size(); ++i) {
        double abs_err = std::abs(double(got[i]) - double(ref[i]));
        double rel_err = abs_err / std::max(1.0e-6, std::abs(double(ref[i])));
        max_abs = std::max(max_abs, abs_err);
        max_rel = std::max(max_rel, rel_err);
        if (abs_err > 2.0e-3 && rel_err > 2.0e-3) {
            std::cerr << "verify mismatch at element " << i << ": got=" << got[i] << " ref=" << ref[i]
                      << " abs=" << abs_err << " rel=" << rel_err << "\n";
            return false;
        }
    }

    if (a.op == "softmax") {
        for (uint32_t r = 0; r < check_rows; ++r) {
            double s = 0.0;
            for (uint32_t i = 0; i < a.width; ++i) s += got[size_t(r) * a.width + i];
            if (std::abs(s - 1.0) > 2.0e-3) {
                std::cerr << "verify softmax row sum mismatch row=" << r << " sum=" << s << "\n";
                return false;
            }
        }
    }
    std::cerr << "verify max_abs=" << max_abs << " max_rel=" << max_rel << "\n";
    return true;
}

int main(int argc, char** argv) {
    try {
        Args args = parse_args(argc, argv);
        const uint32_t elements = args.width * args.rows;
        const size_t bytes = size_t(elements) * sizeof(float);
        const size_t weight_bytes = size_t(args.width) * sizeof(float);

        std::vector<float> input(elements);
        std::vector<float> weight(args.width);
        std::vector<float> output(elements, 0.0f);

        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
        std::uniform_real_distribution<float> wdist(0.75f, 1.25f);
        for (auto& v : input) v = dist(rng);
        for (auto& v : weight) v = wdist(rng);

        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "ml_primitive_bench";
        app.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        VkInstance instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&ici, nullptr, &instance));

        uint32_t phys_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &phys_count, nullptr));
        if (phys_count == 0) throw std::runtime_error("No Vulkan physical device found");
        std::vector<VkPhysicalDevice> phys_devs(phys_count);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &phys_count, phys_devs.data()));
        VkPhysicalDevice phys = phys_devs[0];

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(phys, &props);

        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &q_count, qprops.data());
        uint32_t qfam = UINT32_MAX;
        for (uint32_t i = 0; i < q_count; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qfam = i; break; }
        }
        if (qfam == UINT32_MAX) throw std::runtime_error("No compute queue family found");

        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = qfam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;

        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        VkDevice dev = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDevice(phys, &dci, nullptr, &dev));

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(dev, qfam, 0, &queue);

        Buffer in_buf = make_buffer(phys, dev, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Buffer w_buf = make_buffer(phys, dev, weight_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Buffer out_buf = make_buffer(phys, dev, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        upload(dev, in_buf, input.data(), bytes);
        upload(dev, w_buf, weight.data(), weight_bytes);
        upload(dev, out_buf, output.data(), bytes);

        VkDescriptorSetLayoutBinding bindings[3]{};
        for (uint32_t i = 0; i < 3; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dlci.bindingCount = 3;
        dlci.pBindings = bindings;
        VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &dlci, nullptr, &dsl));

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &dsl;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &pipeline_layout));

        auto spv = read_spv(args.spv_path);
        VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        smci.codeSize = spv.size() * sizeof(uint32_t);
        smci.pCode = spv.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(dev, &smci, nullptr, &shader));

        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shader;
        stage.pName = "main";

        VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        cpci.stage = stage;
        cpci.layout = pipeline_layout;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline));

        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 3;
        VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dpci.maxSets = 1;
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = &pool_size;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(dev, &dpci, nullptr, &pool));

        VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsai.descriptorPool = pool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &dsl;
        VkDescriptorSet ds = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(dev, &dsai, &ds));

        VkDescriptorBufferInfo bis[3]{};
        bis[0] = {in_buf.buffer, 0, bytes};
        bis[1] = {w_buf.buffer, 0, weight_bytes};
        bis[2] = {out_buf.buffer, 0, bytes};
        VkWriteDescriptorSet writes[3]{};
        for (uint32_t i = 0; i < 3; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = ds;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &bis[i];
        }
        vkUpdateDescriptorSets(dev, 3, writes, 0, nullptr);

        VkCommandPoolCreateInfo cmd_pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmd_pool_info.queueFamilyIndex = qfam;
        cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkCommandPool cmd_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(dev, &cmd_pool_info, nullptr, &cmd_pool));

        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.commandPool = cmd_pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));

        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &ds, 0, nullptr);
        PushConstants pc{args.width, args.rows, elements, 0u, args.epsilon};
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        for (uint32_t r = 0; r < args.repeats; ++r) {
            vkCmdDispatch(cmd, args.rows, 1, 1);
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0, 1, &mb, 0, nullptr, 0, nullptr);
        }
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        auto t0 = std::chrono::high_resolution_clock::now();
        VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        bool pass = true;
        if (args.verify) {
            download(dev, out_buf, output.data(), bytes);
            pass = verify_result(args, input, weight, output);
        }

        double processed_elements = double(elements) * double(args.repeats);
        double elems_per_s = processed_elements / (elapsed_ms / 1000.0);
        double estimated_bytes_per_elem = (args.op == "softmax") ?
            ((args.variant == "fused_lmem") ? 8.0 : (args.variant == "online" ? 12.0 : 16.0)) : 16.0;
        double estimated_gbps = processed_elements * estimated_bytes_per_elem / (elapsed_ms / 1000.0) / 1.0e9;

        if (args.csv) {
            std::cout << "op,variant,width,rows,elements,repeats,elapsed_ms,elements_per_s,estimated_bytes_per_element,estimated_bandwidth_GBps,verify,device\n";
            std::cout << args.op << "," << args.variant << "," << args.width << "," << args.rows << "," << elements << "," << args.repeats << ","
                      << std::fixed << std::setprecision(4) << elapsed_ms << ","
                      << std::setprecision(3) << elems_per_s << ","
                      << estimated_bytes_per_elem << "," << estimated_gbps << ","
                      << (pass ? "PASS" : "FAIL") << ",\"" << props.deviceName << "\"\n";
        } else {
            std::cout << "[ml_primitive_bench]\n";
            std::cout << "device=" << props.deviceName << "\n";
            std::cout << "op=" << args.op << "\n";
            std::cout << "variant=" << args.variant << "\n";
            std::cout << "width=" << args.width << "\n";
            std::cout << "rows=" << args.rows << "\n";
            std::cout << "elements=" << elements << "\n";
            std::cout << "repeats=" << args.repeats << "\n";
            std::cout << "elapsed_ms=" << std::fixed << std::setprecision(4) << elapsed_ms << "\n";
            std::cout << "elements_per_second=" << std::setprecision(3) << elems_per_s << "\n";
            std::cout << "estimated_bytes_per_element=" << estimated_bytes_per_elem << "\n";
            std::cout << "estimated_bandwidth_GBps=" << estimated_gbps << "\n";
            std::cout << "verify=" << (pass ? "PASS" : "FAIL") << "\n";
        }

        vkDeviceWaitIdle(dev);
        vkDestroyCommandPool(dev, cmd_pool, nullptr);
        vkDestroyDescriptorPool(dev, pool, nullptr);
        vkDestroyPipeline(dev, pipeline, nullptr);
        vkDestroyShaderModule(dev, shader, nullptr);
        vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
        in_buf.destroy();
        w_buf.destroy();
        out_buf.destroy();
        vkDestroyDevice(dev, nullptr);
        vkDestroyInstance(instance, nullptr);
        return pass ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage(argv[0]);
        return 1;
    }
}
