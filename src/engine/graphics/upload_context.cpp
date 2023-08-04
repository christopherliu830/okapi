#include "upload_context.h"
#include "vulkan.h"
#include "logging.h"

namespace Graphics {

    void UploadContext::Init(vk::Device device, uint32_t queueIndex) {
        _device = device;

        vk::Result result;

        vk::CommandPoolCreateInfo cmdPoolInfo {
            vk::CommandPoolCreateFlagBits::eTransient |
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueIndex
        };

        std::tie(result, commandPool) = _device.createCommandPool(cmdPoolInfo);

        // Allocate the single command buffer for the upload pool.
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        std::vector<vk::CommandBuffer> cmds;
        std::tie(result, cmds) = _device.allocateCommandBuffers(allocInfo);
        VK_CHECK(result);
        cmd = cmds[0];

        std::tie(result, fence) = _device.createFence({});
        VK_CHECK(result);
    }

    void UploadContext::Destroy() {
        _device.destroyFence(fence);
        _device.freeCommandBuffers(commandPool, cmd);
        _device.destroyCommandPool(commandPool);
    }

    void UploadContext::Begin() {
        VK_CHECK(cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit }));
    }

    void UploadContext::SubmitSync(vk::Queue queue) {
        vk::SubmitInfo submitInfo { };
        submitInfo.setCommandBuffers(cmd);

        VK_CHECK(cmd.end());

        VK_CHECK(queue.submit(submitInfo, fence));
        VK_CHECK(_device.waitForFences(fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(_device.resetFences(fence));
        _device.resetCommandPool(commandPool, {});
    }
}