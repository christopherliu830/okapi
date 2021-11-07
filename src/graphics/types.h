#pragma once

#include "vulkan.h"

namespace Graphics {

    struct AllocatedBuffer {
        vk::Buffer buffer;
        VmaAllocation allocation;
    };

    struct AllocatedImage {
        vk::Image image;
        VmaAllocation allocation;
    };


};