#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace Vulkan {
    namespace Initializers {
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
            uint32_t commandBufferCount,
            VkCommandPool commandPool
        ) {
            VkCommandBufferAllocateInfo cmdAllocInfo {};
            cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdAllocInfo.pNext = nullptr;
            cmdAllocInfo.commandPool = commandPool;
            cmdAllocInfo.commandBufferCount = commandBufferCount;
            cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            return cmdAllocInfo;
        }

        VkCommandPoolCreateInfo CommandPoolCreateInfo(
            uint32_t queueFamilyIndex,
            VkCommandPoolCreateFlags flags = 0
        ) {
            VkCommandPoolCreateInfo poolInfo {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.pNext = nullptr;
            poolInfo.queueFamilyIndex = queueFamilyIndex;
            poolInfo.flags;

            return poolInfo;
        }
    }
}
