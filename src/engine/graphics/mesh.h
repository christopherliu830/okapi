#pragma once

#include "types.h"
#include "vulkan.h"
#include <vector>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace Graphics {

    class Engine;

    struct VertexInputDescription {
        std::vector<vk::VertexInputBindingDescription> bindings;
        std::vector<vk::VertexInputAttributeDescription> attributes;
        vk::PipelineVertexInputStateCreateFlags flags;
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;
        static VertexInputDescription GetInputDescription();
    };

    class Mesh {

    public:
        AllocatedBuffer vertexBuffer;
        std::vector<Vertex> vertices;

        vk::Result Allocate();
        void Destroy();
        static std::pair<bool, Mesh> FromObj(Engine& engine, const std::string &path);

        size_t GetVertexBufferSize() {
            return vertices.size() * sizeof(Vertex);
        }
    };

    struct MeshPushConstants {
        glm::vec4 data;
        glm::mat4 renderMatrix;
    };
}