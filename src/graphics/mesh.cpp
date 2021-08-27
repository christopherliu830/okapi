#include "mesh.h"

namespace Graphics {
    VertexInputDescription Vertex::GetInputDescription() {
        VertexInputDescription description;

        // One vertex buffer binding with a per-vertex rate
        vk::VertexInputBindingDescription mainBinding{
            0, sizeof(Vertex), vk::VertexInputRate::eVertex
        };
        mainBinding.binding = 0;
        mainBinding.stride = sizeof(Vertex);
        mainBinding.inputRate = vk::VertexInputRate::eVertex;

        description.bindings.push_back(mainBinding);

        vk::VertexInputAttributeDescription positionAttribute{
            0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)
        };
        positionAttribute.binding = 0;
        positionAttribute.location = 0;
        positionAttribute.format = vk::Format::eR32G32B32Sfloat;
        positionAttribute.offset = offsetof(Vertex, position);

        vk::VertexInputAttributeDescription normalAttribute{
            1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)
        };
        normalAttribute.binding = 0;
        normalAttribute.location = 1;
        normalAttribute.format = vk::Format::eR32G32B32Sfloat;
        normalAttribute.offset = offsetof(Vertex, normal);

        vk::VertexInputAttributeDescription colorAttribute{
            2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)
        };
        colorAttribute.binding = 0;
        colorAttribute.location = 2;
        colorAttribute.format = vk::Format::eR32G32B32Sfloat;
        colorAttribute.offset = offsetof(Vertex, color);

        description.attributes.push_back(positionAttribute);
        description.attributes.push_back(normalAttribute);
        description.attributes.push_back(colorAttribute);

        return description;
    }
}