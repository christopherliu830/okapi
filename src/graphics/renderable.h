#pragma once

#include "mesh.h"
#include "vulkan.h"
#include <glm/mat4x4.hpp>

namespace Graphics {

    // pipeline and layout are 64 bit handles to vulkan driver structures
    struct Material {
        vk::Pipeline pipeline;
        vk::PipelineLayout pipelineLayout;
        vk::DescriptorSet textureSet {VK_NULL_HANDLE};
    };

    struct Renderable {
        Mesh* mesh;
        Material* material;
        Texture* texture;
    };

};
