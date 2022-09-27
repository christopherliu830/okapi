#include "cube.h"

namespace Primitives {
    Cube::Cube(Graphics::Engine& engine) {
        if (engine.GetMesh("cube") != nullptr) {
            return;
        }

        Graphics::Mesh mesh = Graphics::Mesh();

        int vertexLength = 6;

        glm::vec3 positions[] = {
            // Front face
            { -1,  1, -1 },
            {  1,  1, -1 },
            {  1, -1, -1 },
            { -1, -1, -1 },
            { -1,  1, -1 },
            {  1, -1, -1 },
        };

        glm::vec3 normals[] = {
            // Front face
            {  0,  0, -1 },
            {  0,  0, -1 },
            {  0,  0, -1 },
            {  0,  0, -1 },
            {  0,  0, -1 },
            {  0,  0, -1 },
        };

        glm::vec2 uvs[] = {
            // Front face
            {  0,  1 },
            {  1,  1 },
            {  1,  0 },
            {  0,  0 },
            {  0,  1 },
            {  1,  0 },
        };

        mesh.vertices.clear();

        for(int i = 0; i < vertexLength; i++) {
            Graphics::Vertex v;
            v.position = positions[i];
            v.normal = normals[i];
            v.uv = uvs[i];
            v.color = normals[i];

            mesh.vertices.push_back(v);
        }

        mesh.Allocate();
        Graphics::Mesh* meshPtr = engine.CreateMesh("cube", mesh);
        renderable.mesh = meshPtr;
        renderable.material = engine.GetMaterial("default");
    }
}