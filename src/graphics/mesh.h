#pragma once

#include "types.h"
#include <vector>
#include <glm/vec3.hpp>
#include "vulkan/vulkan.hpp"
#include "vk_mem_alloc.h"

namespace Graphics {

    struct VertexInputDescription {
        std::vector<vk::VertexInputBindingDescription> bindings;
        std::vector<vk::VertexInputAttributeDescription> attributes;
        vk::PipelineVertexInputStateCreateFlags flags;
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        static VertexInputDescription GetInputDescription();
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        AllocatedBuffer vertexBuffer;
        VmaAllocator allocator;

        Mesh() {}
        Mesh(const std::vector<Vertex> &&v): vertices{std::move(v)} { }

        void Destroy(VmaAllocator allocator) {
            vmaDestroyBuffer(
                allocator,
                vertexBuffer.buffer,
                vertexBuffer.allocation
            );
            vertices.clear();
        }

        // Copy the vertex buffer's data into GPU readable data.
        vk::Result Map(VmaAllocator allocator) {
            void * data;
            vk::Result result = (vk::Result)vmaMapMemory(allocator, vertexBuffer.allocation, &data);
            if (result != vk::Result::eSuccess) return (vk::Result)result;
            auto p = (float*)vertices.data();
            memcpy(data, vertices.data(), GetVertexBufferSize());
            vmaUnmapMemory(allocator, vertexBuffer.allocation);
            return result;
        }

        uint32_t GetVertexBufferSize() {
            return vertices.size() * sizeof(Vertex);
        }
    };
};