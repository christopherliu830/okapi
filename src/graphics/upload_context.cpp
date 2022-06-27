#include "upload_context.h"
#include "vulkan.h"
#include "logging.h"

namespace Graphics {

    void UploadContext::Init(vk::Device device, uint32_t queueIndex) {
        vk::Result result;

        vk::CommandPoolCreateInfo cmdPoolInfo {
            vk::CommandPoolCreateFlagBits::eTransient |
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueIndex
        };

        std::tie(result, commandPool) = device.createCommandPool(cmdPoolInfo);

        // Allocate the single command buffer for the upload pool.
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        std::vector<vk::CommandBuffer> cmds;
        std::tie(result, cmds) = device.allocateCommandBuffers(allocInfo);
        VK_CHECK(result);
        cmd = cmds[0];

        std::tie(result, fence) = device.createFence({});
        VK_CHECK(result);
    }

    void UploadContext::Destroy(vk::Device device) {
        device.destroyFence(fence);
        device.freeCommandBuffers(commandPool, cmd);
    }
}