#pragma once

#include "vulkan.h"

namespace Graphics {
    class UploadContext {

    public:
        vk::CommandPool commandPool;
        vk::CommandBuffer cmd;
        vk::Fence fence;
        void Init(vk::Device device, uint32_t queueIndex);
        void Begin();
        void SubmitSync(vk::Queue queue);
        void Destroy();

        operator bool() { return commandPool; }

    private:
        vk::Device _device;
    };
};
