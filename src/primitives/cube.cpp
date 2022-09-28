#include "cube.h"

namespace Primitives {
    Cube::Cube(Graphics::Engine& engine) {
        if (engine.GetMesh("cube") != nullptr) {
            return;
        }

        Graphics::Mesh mesh = Graphics::Mesh();

        int vertexLength = 36;

        glm::vec3 positions[] = {
            // Front face
            { -1,  1, -1 }, {  1,  1, -1 }, {  1, -1, -1 },
            { -1, -1, -1 }, { -1,  1, -1 }, {  1, -1, -1 },

            // Top face
            { -1,  1,  1 }, {  1,  1,  1 }, {  1,  1, -1 },
            { -1,  1, -1 }, { -1,  1,  1 }, {  1,  1, -1 },

            // Left face
            { -1,  1,  1 }, { -1,  1, -1 }, { -1, -1, -1 },
            { -1, -1,  1 }, { -1,  1,  1 }, { -1, -1, -1 },

            // Back face
            {  1,  1,  1 }, { -1,  1,  1 }, { -1, -1,  1 },
            {  1, -1,  1 }, {  1,  1,  1 }, { -1, -1,  1 },

            // Bottom face
            {  1, -1,  1 }, { -1, -1,  1 }, { -1, -1, -1 },
            {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1, -1 },

            // Right face
            {  1,  1, -1 }, {  1,  1,  1 }, {  1, -1,  1 },
            {  1, -1, -1 }, {  1,  1, -1 }, {  1, -1,  1 },
        };

        glm::vec3 normals[] = {
            // Front face
            {  0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 },
            {  0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 },

            // Top face
            {  0,  1,  0 }, {  0,  1,  0 }, {  0,  1,  0 },
            {  0,  1,  0 }, {  0,  1,  0 }, {  0,  1,  0 },

            // Left face
            { -1,  0,  0 }, { -1,  0,  0 }, { -1,  0,  0 },
            { -1,  0,  0 }, { -1,  0,  0 }, { -1,  0,  0 },

            // Back face
            {  0,  0,  1 }, {  0,  0,  1 }, {  0,  0,  1 },
            {  0,  0,  1 }, {  0,  0,  1 }, {  0,  0,  1 },

            // Bottom face
            {  0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 },
            {  0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 },

            // Right face
            {  1,  0,  0 }, {  1,  0,  0 }, {  1,  0,  0 },
            {  1,  0,  0 }, {  1,  0,  0 }, {  1,  0,  0 },
        };

        glm::vec2 uvs[] = {
            {  0,  1 }, {  1,  1 }, {  1,  0 },
            {  0,  0 }, {  0,  1 }, {  1,  0 },
        };

        mesh.vertices.clear();

        for(int i = 0; i < vertexLength; i++) {
            Graphics::Vertex v;
            v.position = positions[i];
            v.normal = normals[i];
            v.uv = uvs[i % 6];
            v.color = normals[i];

            mesh.vertices.push_back(v);
        }

        mesh.Allocate();
        Graphics::Mesh* meshPtr = engine.CreateMesh("cube", mesh);
        renderable.mesh = meshPtr;
        renderable.material = engine.GetMaterial("default");
    }
}