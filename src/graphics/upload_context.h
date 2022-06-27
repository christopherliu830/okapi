#pragma once

#include "vulkan.h"

namespace Graphics {
    class UploadContext {

public:
        vk::CommandPool commandPool;
        vk::CommandBuffer cmd;
        vk::Fence fence;
        void Init(vk::Device device, uint32_t queueIndex);
        void Destroy(vk::Device device);

        operator bool() { return commandPool; }
    };
};
