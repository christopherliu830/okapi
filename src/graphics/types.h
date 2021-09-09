#pragma once

#include "vulkan.h"
#include "logging.h"

namespace Graphics {

    struct AllocatedBuffer {
        vk::Buffer buffer;
        vma::Allocation allocation;
        vma::Allocator _allocator;

        AllocatedBuffer() {}

        AllocatedBuffer(
            vma::Allocator allocator,
            size_t size,
            vk::BufferUsageFlags usage,
            vma::MemoryUsage memoryUsage)
            : _allocator(allocator)
        {
            auto [result, bufAlloc] = _allocator.createBuffer(
                {{}, size, usage, vk::SharingMode::eExclusive},
                {{}, memoryUsage}
            );
            VK_CHECK(result);
            buffer = bufAlloc.first;
            allocation = bufAlloc.second;
        }

        static AllocatedBuffer && Create(
            vma::Allocator allocator,
            size_t size,
            vk::BufferUsageFlags usage,
            vma::MemoryUsage memoryUsage
        ) {
            return std::move(AllocatedBuffer {allocator, size, usage, memoryUsage});
        }

        void Destroy() {
            if (_allocator) {
                _allocator.destroyBuffer(buffer, allocation);
                _allocator = nullptr;
                buffer = nullptr;
                allocation = nullptr;
            }
        }

        ~AllocatedBuffer() {
            this->Destroy();
        }

        AllocatedBuffer & operator =(AllocatedBuffer buffer) = delete;
    };

    struct AllocatedImage {
        vk::Image image;
        vma::Allocation allocation;
    };


};