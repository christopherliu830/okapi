#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec3.hpp>

namespace Graphics {

    struct AllocatedBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

};