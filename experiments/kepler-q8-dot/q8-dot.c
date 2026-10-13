#include <vulkan/vulkan.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static double seconds_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static uint16_t scale_bits(uint64_t block) { return (uint16_t) (0x2800 + (block % 4) * 0x400); }
static float scale_value(uint64_t block) { return ldexpf(1.0f, -5 + (int) (block % 4)); }
static int8_t quant_value(uint64_t block, uint32_t q) { return (int8_t) ((block * 37 + q * 53 + (block >> 3)) % 256 - 128); }
static float activation_value(uint32_t q) { return ((int) ((q * 19 + 7) % 127) - 63) / 64.0f; }

#define VK_OK(call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { \
    fprintf(stderr, "%s failed: %d\n", #call, r_); exit(1); } } while (0)

static uint32_t find_memory_type(const VkPhysicalDeviceMemoryProperties *props,
                                 uint32_t bits, VkMemoryPropertyFlags required) {
    for (uint32_t i = 0; i < props->memoryTypeCount; i++) {
        if ((bits & (1u << i)) &&
            (props->memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    fprintf(stderr, "no suitable Vulkan memory type\n");
    exit(1);
}

static void make_buffer(VkDevice dev, const VkPhysicalDeviceMemoryProperties *mem_props,
                        VkDeviceSize size, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags memory_flags,
                        VkBuffer *buffer, VkDeviceMemory *memory) {
    const VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VK_OK(vkCreateBuffer(dev, &bci, NULL, buffer));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, *buffer, &req);
    const VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = find_memory_type(mem_props, req.memoryTypeBits, memory_flags),
    };
    VK_OK(vkAllocateMemory(dev, &mai, NULL, memory));
    VK_OK(vkBindBufferMemory(dev, *buffer, *memory, 0));
}

static uint32_t *read_spirv(const char *path, size_t *size_out) {
    FILE *file = fopen(path, "rb");
    if (!file) { perror(path); exit(1); }
    if (fseek(file, 0, SEEK_END) || (*size_out = (size_t) ftell(file)) == 0 ||
        fseek(file, 0, SEEK_SET)) {
        fprintf(stderr, "failed to size %s\n", path); exit(1);
    }
    uint32_t *data = malloc(*size_out);
    if (!data || fread(data, 1, *size_out, file) != *size_out) {
        fprintf(stderr, "failed to read %s\n", path); exit(1);
    }
    fclose(file);
    return data;
}

int main(int argc, char **argv) {
    if (argc != 7) {
        fprintf(stderr, "usage: %s shader.spv mode(0..4) K M iterations repeats\n", argv[0]);
        return 2;
    }
    const uint32_t mode = (uint32_t) strtoul(argv[2], NULL, 0);
    const uint32_t stage = 2;
    const uint32_t k = (uint32_t) strtoul(argv[3], NULL, 0);
    const uint32_t m = (uint32_t) strtoul(argv[4], NULL, 0);
    const uint32_t iterations = (uint32_t) strtoul(argv[5], NULL, 0);
    const uint32_t repeats = (uint32_t) strtoul(argv[6], NULL, 0);
    if (mode > 4 || (k != 1024 && k != 2048 && k != 8192) || !m || m > 8192 ||
        !iterations || iterations > 100 || !repeats || repeats > 10) return 2;
    const char *rows_env = getenv("KEPLER_TEST_ROWS");
    const uint32_t rows = rows_env ? (uint32_t) strtoul(rows_env, NULL, 0) : 1;
    if (rows != 1 && rows != 2) return 2;
    const VkDeviceSize bytes = (uint64_t) k * m / 32 * 34;
    const VkDeviceSize dst_bytes = m * sizeof(float);
    const VkDeviceSize activation_bytes = k * sizeof(float);
    const uint64_t groups64 = ((uint64_t) m + rows - 1) / rows;

    const VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "q8-dot",
        .apiVersion = VK_API_VERSION_1_2,
    };
    const VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkInstance instance;
    VK_OK(vkCreateInstance(&ici, NULL, &instance));
    uint32_t physical_count = 0;
    VK_OK(vkEnumeratePhysicalDevices(instance, &physical_count, NULL));
    if (!physical_count) { fprintf(stderr, "no Vulkan physical device\n"); return 1; }
    VkPhysicalDevice *physical_devices = calloc(physical_count, sizeof(*physical_devices));
    VK_OK(vkEnumeratePhysicalDevices(instance, &physical_count, physical_devices));
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t matches = 0;
    for (uint32_t i = 0; i < physical_count; ++i) {
        VkPhysicalDeviceProperties candidate;
        vkGetPhysicalDeviceProperties(physical_devices[i], &candidate);
        if (candidate.vendorID == 0x10de && candidate.deviceID == 0x1022) {
            physical = physical_devices[i];
            matches++;
        }
    }
    free(physical_devices);
    if (matches != 1) { fprintf(stderr, "K20c is not unique\n"); return 1; }

    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceProperties(physical, &props);
    vkGetPhysicalDeviceMemoryProperties(physical, &mem_props);
    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, NULL);
    VkQueueFamilyProperties *families = calloc(family_count, sizeof(*families));
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, families);
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < family_count; i++) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { family = i; break; }
    }
    const uint32_t timestamp_bits = family == UINT32_MAX ? 0 : families[family].timestampValidBits;
    free(families);
    if (family == UINT32_MAX) { fprintf(stderr, "no compute queue\n"); return 1; }
    if (groups64 > props.limits.maxComputeWorkGroupCount[0]) {
        fprintf(stderr, "dispatch needs %" PRIu64 " groups, device limit is %u\n",
                groups64, props.limits.maxComputeWorkGroupCount[0]);
        return 1;
    }
    if (!timestamp_bits || !props.limits.timestampComputeAndGraphics) {
        fprintf(stderr, "compute timestamps unsupported\n"); return 1;
    }

    const float priority = 1.0f;
    const VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority,
    };
    VkPhysicalDeviceVulkan12Features f12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan11Features f11 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &f12 };
    VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f11 };
    vkGetPhysicalDeviceFeatures2(physical, &features);
    features.features.robustBufferAccess = VK_FALSE;
    if (!features.features.shaderInt16 || !f11.storageBuffer16BitAccess || !f12.shaderInt8) {
        fprintf(stderr, "required integer/storage features unavailable\n"); return 1;
    }
    VkPhysicalDeviceSubgroupProperties subgroup = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroup };
    vkGetPhysicalDeviceProperties2(physical, &properties2);
    if (subgroup.subgroupSize != 32 || !(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)) return 1;
    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
        .pNext = &f11, .pEnabledFeatures = &features.features,
    };
    VkDevice dev;
    VK_OK(vkCreateDevice(physical, &dci, NULL, &dev));
    VkQueue queue;
    vkGetDeviceQueue(dev, family, 0, &queue);

    VkBuffer src, dst, verify, activation, upload;
    VkDeviceMemory src_mem, dst_mem, verify_mem, activation_mem, upload_mem;
    const VkBufferUsageFlags gpu_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    make_buffer(dev, &mem_props, bytes, gpu_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &src, &src_mem);
    make_buffer(dev, &mem_props, dst_bytes, gpu_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &dst, &dst_mem);
    make_buffer(dev, &mem_props, dst_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &verify, &verify_mem);

    make_buffer(dev, &mem_props, activation_bytes, gpu_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &activation, &activation_mem);
    make_buffer(dev, &mem_props, bytes + activation_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &upload, &upload_mem);
    unsigned char *input;
    VK_OK(vkMapMemory(dev, upload_mem, 0, bytes + activation_bytes, 0, (void **) &input));
    for (uint64_t block = 0; block < bytes / 34; ++block) {
        const uint16_t bits = scale_bits(block);
        memcpy(input + block * 34, &bits, 2);
        for (uint32_t q = 0; q < 32; ++q) input[block * 34 + 2 + q] = (unsigned char) quant_value(block, q);
    }
    float *activation_input = (float *) (input + bytes);
    for (uint32_t q = 0; q < k; ++q) activation_input[q] = activation_value(q);
    vkUnmapMemory(dev, upload_mem);

    const VkDescriptorSetLayoutBinding bindings[5] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
    };
    const VkDescriptorSetLayoutCreateInfo dlci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 5, .pBindings = bindings,
    };
    VkDescriptorSetLayout descriptor_layout;
    VK_OK(vkCreateDescriptorSetLayout(dev, &dlci, NULL, &descriptor_layout));
    const VkPushConstantRange push_range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 13 * sizeof(uint32_t) };
    const VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &descriptor_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range,
    };
    VkPipelineLayout pipeline_layout;
    VK_OK(vkCreatePipelineLayout(dev, &plci, NULL, &pipeline_layout));

    size_t spirv_size;
    uint32_t *spirv = read_spirv(argv[1], &spirv_size);
    const VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_size, .pCode = spirv,
    };
    VkShaderModule module;
    VK_OK(vkCreateShaderModule(dev, &smci, NULL, &module));
    free(spirv);
    const uint32_t spec_data[4] = {128, rows, 1, mode};
    const VkSpecializationMapEntry spec_entries[4] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}, {3, 12, 4}};
    const VkSpecializationInfo spec = { .mapEntryCount = 4, .pMapEntries = spec_entries, .dataSize = sizeof(spec_data), .pData = spec_data };
    const VkComputePipelineCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main", .pSpecializationInfo = &spec},
        .layout = pipeline_layout,
    };
    VkPipeline pipeline;
    VK_OK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipeline));

    const VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 5,
    };
    const VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_size,
    };
    VkDescriptorPool descriptor_pool;
    VK_OK(vkCreateDescriptorPool(dev, &dpci, NULL, &descriptor_pool));
    const VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool, .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_layout,
    };
    VkDescriptorSet descriptor_set;
    VK_OK(vkAllocateDescriptorSets(dev, &dsai, &descriptor_set));
    const VkDescriptorBufferInfo buffer_infos[5] = {
        {src, 0, bytes}, {activation, 0, activation_bytes}, {dst, 0, dst_bytes},
        {activation, 0, activation_bytes}, {activation, 0, activation_bytes},
    };
    VkWriteDescriptorSet writes[5] = {0};
    for (uint32_t i = 0; i < 5; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffer_infos[i];
    }
    vkUpdateDescriptorSets(dev, 5, writes, 0, NULL);

    const VkCommandPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = family,
    };
    VkCommandPool command_pool;
    VK_OK(vkCreateCommandPool(dev, &pool_ci, NULL, &command_pool));
    const VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command;
    VK_OK(vkAllocateCommandBuffers(dev, &cbai, &command));
    const VkQueryPoolCreateInfo qpci = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2,
    };
    VkQueryPool queries;
    VK_OK(vkCreateQueryPool(dev, &qpci, NULL, &queries));

    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_OK(vkBeginCommandBuffer(command, &begin));
    const VkBufferCopy upload_weights = {0, 0, bytes};
    const VkBufferCopy upload_activation = {bytes, 0, activation_bytes};
    vkCmdCopyBuffer(command, upload, src, 1, &upload_weights);
    vkCmdCopyBuffer(command, upload, activation, 1, &upload_activation);
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &descriptor_set, 0, NULL);
    const uint32_t push[13] = {k, k, k, m, k*m, k, m, 0, 0, 1, 1, 1, 1};
    vkCmdPushConstants(command, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(command, (uint32_t) groups64, 1, 1);
    VK_OK(vkEndCommandBuffer(command));
    const VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command };
    VK_OK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_OK(vkQueueWaitIdle(queue));
    VK_OK(vkResetCommandPool(dev, command_pool, 0));
    const VkCommandBufferBeginInfo timed_begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VK_OK(vkBeginCommandBuffer(command, &timed_begin));
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    vkCmdPushConstants(command, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    /*
     * Every dispatch writes the same destination range.  Vulkan does not
     * implicitly resolve this WAW hazard, so make each shader write available
     * and visible before the next shader write.  The first barrier also orders the prior submit and readback copy.
     */
    VkBufferMemoryBarrier dispatch_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = dst,
        .offset = 0,
        .size = dst_bytes,
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, NULL, 1, &dispatch_barrier, 0, NULL);
    vkCmdResetQueryPool(command, queries, 0, 2);
    vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queries, 0);
    for (uint32_t i = 0; i < iterations; i++) {
        vkCmdDispatch(command, (uint32_t) groups64, 1, 1);
        if (i + 1 < iterations) {
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                                 0, NULL, 1, &dispatch_barrier, 0, NULL);
        }
    }
    vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queries, 1);
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
    const VkBufferCopy copy = {.srcOffset = 0, .dstOffset = 0, .size = dst_bytes};
    vkCmdCopyBuffer(command, dst, verify, 1, &copy);
    VK_OK(vkEndCommandBuffer(command));
    // Submit once before measuring to warm the complete command sequence.
    VK_OK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_OK(vkQueueWaitIdle(queue));
    printf("rows_per_group=%u\ngroups=%" PRIu64 "\n", rows, groups64);
    printf("device=%s\nmode=%u\nK=%u\nM=%u\nWG=128\nsubgroup=32\nrobustness=disabled\n", props.deviceName, mode, k, m);
    printf("weight_bytes=%" PRIu64 "\niterations=%u\nrepeats=%u\ncommand_recording=outside_timing\n", (uint64_t) bytes, iterations, repeats);
    const uint64_t timestamp_mask = timestamp_bits == 64 ? UINT64_MAX : ((UINT64_C(1) << timestamp_bits) - 1);
    for (uint32_t repeat = 0; repeat < repeats; ++repeat) {
        const double start = seconds_now();
        VK_OK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        VK_OK(vkQueueWaitIdle(queue));
        const double wall = seconds_now() - start;
        uint64_t timestamps[2];
        VK_OK(vkGetQueryPoolResults(dev, queries, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        const double seconds = (double) ((timestamps[1] - timestamps[0]) & timestamp_mask) * props.limits.timestampPeriod * 1e-9;
        printf("sample=%u gpu_us_per_op=%.6f submit_wait_us_per_op=%.6f logical_weight_GBs=%.6f\n", repeat, seconds * 1e6 / iterations, wall * 1e6 / iterations, bytes * iterations / seconds / 1e9);
    }
    float *verified;
    VK_OK(vkMapMemory(dev, verify_mem, 0, dst_bytes, 0, (void **) &verified));
    double max_error = 0;
    for (uint32_t row = 0; row < m; ++row) {
        double expected = 0;
        for (uint32_t col = 0; col < k; col += 8) {
            const uint64_t block = (uint64_t) row * (k / 32) + col / 32;
            const uint32_t q0 = col % 32;
            if (stage == 0) {
                int packed_sum = 0;
                for (uint32_t q = 0; q < 8; q += 2) {
                    uint16_t word = (uint8_t) quant_value(block, q0 + q) | ((uint16_t) (uint8_t) quant_value(block, q0 + q + 1) << 8);
                    packed_sum += (int16_t) word;
                }
                expected += packed_sum + scale_bits(block);
            } else {
                for (uint32_t q = 0; q < 8; ++q) {
                    double value = quant_value(block, q0 + q) * scale_value(block);
                    if (stage == 2) value *= activation_value(col + q);
                    expected += value;
                }
            }
        }
        const double error = fabs(verified[row] - expected);
        if (error > max_error) max_error = error;
        const double tolerance = stage == 0 ? 8 + fabs(expected) * 1e-6 : 1e-4 + fabs(expected) * 1e-5;
        if (!isfinite(verified[row]) || error > tolerance) {
            fprintf(stderr, "CPU verification failed row=%u got=%.9g expected=%.9g error=%.9g\n", row, verified[row], expected, error);
            return 1;
        }
    }
    vkUnmapMemory(dev, verify_mem);
    printf("verification=pass rows=%u max_absolute_error=%.9g\n", m, max_error);
    printf("timestamp_scope=dispatches_and_WAW_barriers\nwall_scope=submit_and_wait_includes_small_readback_copy_excludes_recording_upload\n");

    vkDestroyQueryPool(dev, queries, NULL);
    vkDestroyCommandPool(dev, command_pool, NULL);
    vkDestroyDescriptorPool(dev, descriptor_pool, NULL);
    vkDestroyPipeline(dev, pipeline, NULL);
    vkDestroyShaderModule(dev, module, NULL);
    vkDestroyPipelineLayout(dev, pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(dev, descriptor_layout, NULL);
    vkDestroyBuffer(dev, verify, NULL); vkFreeMemory(dev, verify_mem, NULL);
    vkDestroyBuffer(dev, dst, NULL); vkFreeMemory(dev, dst_mem, NULL);
    vkDestroyBuffer(dev, upload, NULL); vkFreeMemory(dev, upload_mem, NULL);
    vkDestroyBuffer(dev, activation, NULL); vkFreeMemory(dev, activation_mem, NULL);
    vkDestroyBuffer(dev, src, NULL); vkFreeMemory(dev, src_mem, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(instance, NULL);
    return 0;
}
