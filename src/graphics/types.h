#pragma once

#include <vk_mem_alloc.h>

namespace Graphics {
    struct AllocatedBuffer {
        VkBuffer _buffer;
        VmaAllocation _allocation;
    }
}