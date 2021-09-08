#pragma once

#include "vulkan.h"

namespace Graphics {

    struct AllocatedBuffer {
        vk::Buffer buffer;
        vma::Allocation allocation;
    };

    struct AllocatedImage {
        vk::Image image;
        vma::Allocation allocation;
    };


};