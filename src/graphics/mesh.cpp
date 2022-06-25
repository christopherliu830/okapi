#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "logging.h"
#include "graphics.h"

namespace Graphics {

    glm::vec3 Vertex::GetColor() {
        return color;
    }

    VertexInputDescription Vertex::GetInputDescription() {
        VertexInputDescription description;

        // One vertex buffer binding with a per-vertex rate
        vk::VertexInputBindingDescription mainBinding {
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

    // Allocate buffer's data and map into GPU readable data.
    vk::Result Mesh::Allocate() {
        vk::Result res;

        vertexBuffer = _engine->CreateBuffer(
            vertices.size() * sizeof(Vertex), 
            vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            {},
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        void *data;
        res = _engine->MapMemory(vertexBuffer.allocation, &data);
        if (res != vk::Result::eSuccess) {
            _engine->DestroyBuffer(vertexBuffer);
            return res;
        }

        memcpy(data, vertices.data(), GetVertexBufferSize());

        _engine->UnmapMemory(vertexBuffer.allocation);
        return res;
    }

    void Mesh::Destroy() {
        _engine->DestroyBuffer(vertexBuffer);
        vertices.clear();
    }

    std::pair<bool, Mesh> Mesh::FromObj(Engine *engine, const std::string &path) {

        tinyobj::ObjReaderConfig config;
        tinyobj::ObjReader reader;

        Mesh m {engine};

        if (!reader.ParseFromFile(path.c_str(), config)) {
            if (!reader.Error().empty()) {
                LOGE(reader.Error());
            }
            return std::pair(false, m);
        }

        if (!reader.Warning().empty()) {
            LOGW(reader.Warning());
        }

        // attrib contains the vertex arrays of the file
        tinyobj::attrib_t attrib = reader.GetAttrib();

        // shape info for each shape
        std::vector<tinyobj::shape_t> shapes = reader.GetShapes();

        // Information about materials for each shape
        std::vector<tinyobj::material_t> materials = reader.GetMaterials();

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++) {
            // Loop over faces(polygon)
            size_t indexOffset = 0;
            for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                // Loop over vertices in the face
                for (size_t v = 0; v < fv; v++) {
                    Vertex vertex;

                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[indexOffset + v];
                    tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                    vertex.position = { vx, vy, vz };

                    // check if "normal index" is 0 or positive. negative = no normal data
                    if (idx.normal_index >= 0) {
                        tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                        vertex.normal = { nx, ny, nz };

                        vertex.color = vertex.normal;
                    }

                    // check if "texcoord index" is 0 or positive. negative = no texcoord data
                    if (idx.texcoord_index >= 0) {
                        tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    }

                    tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
                    tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
                    tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
                    m.vertices.push_back(vertex);
                }
                indexOffset += fv;
            }
        }

        if (m.Allocate() != vk::Result::eSuccess) return std::pair(false, m);

        return std::pair(true, m);
    }
}