#pragma once

#include "vulkan.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Graphics {

    struct AllocatedBuffer {
        vk::Buffer buffer;
        vma::Allocation allocation;
        vma::AllocationInfo allocInfo;
    };

    struct AllocatedImage {
        vk::Image image;
        vma::Allocation allocation;
        vma::AllocationInfo allocInfo;
    };

    struct GPUCameraData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
    };

    struct GPUSceneData {
        glm::vec4 fogColor; // w = exponent
        glm::vec4 fogDistances; // x = min, y = max, zw = unused.
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection; // w = sun power
        glm::vec4 sunlightColor;
    };

};