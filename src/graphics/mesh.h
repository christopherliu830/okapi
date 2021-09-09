#pragma once

#include "types.h"
#include "vulkan.h"
#include <vector>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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
        glm::vec3 GetColor();
        static VertexInputDescription GetInputDescription();
    };

    class Mesh {

    public:
        std::unique_ptr<AllocatedBuffer> vertexBuffer;
        std::vector<Vertex> vertices;
        Mesh() {}

        void Destroy();
        static Mesh FromObj(const char* path, vma::Allocator allocator);

        size_t GetVertexBufferSize() {
            return vertices.size() * sizeof(Vertex);
        }

        // Mesh & operator =(Mesh && other) {
        //     vertexBuffer = std::move(other.vertexBuffer);
        //     vertices = std::move(other.vertices);
        //     return *this; 
        // }

    private:
        vk::Result Allocate();
        vma::Allocator _allocator;
    };

    struct MeshPushConstants {
        glm::vec4 data;
        glm::mat4 renderMatrix;
    };
}