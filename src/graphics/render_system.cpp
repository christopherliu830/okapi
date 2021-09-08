#include "graphics.h"
#include "render_system.h"
#include "renderable.h"
#include "transform.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>

namespace Graphics {
    void RenderSystem::Update(entt::registry &registry, float deltaTime) {
        auto view = registry.view<Transform, Renderable>();
        Engine::Perframe* perframe = _engine->BeginFrame();

        float i = 3.f;
        if (perframe) {
            auto [width, height] = _engine->GetWindowSize();
            vk::CommandBuffer cmd = perframe->primaryCommandBuffer;
            glm::vec3 cameraPos {0.f, -6.f, -10.f};
            glm::mat4 viewMatrix = glm::translate(glm::mat4{1.f}, cameraPos);
            glm::mat4 projection = glm::perspective(
                glm::radians(70.f), 
                (float)width / (float)height,
                0.1f,
                200.f
            );
            projection[1][1] *= -1;

            Mesh* lastMesh = nullptr;
            Material* lastMaterial = nullptr;

            for(auto [entity, transform, obj]: view.each()) {
                glm::mat4 mvp = projection * viewMatrix * transform.matrix;

                if (obj.material != lastMaterial) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline);
                    lastMaterial = obj.material;
                }

                MeshPushConstants mvpMatrix;
                mvpMatrix.renderMatrix = projection * viewMatrix * transform.matrix;

                cmd.pushConstants(
                    obj.material->pipelineLayout,
                    vk::ShaderStageFlagBits::eVertex,
                    0, sizeof(MeshPushConstants), &mvpMatrix
                );

                if (obj.mesh != lastMesh) {
                    vk::DeviceSize offset = 0;
                    cmd.bindVertexBuffers(0, 1, &obj.mesh->vertexBuffer.buffer, &offset);
                    lastMesh = obj.mesh;
                }

                cmd.draw(static_cast<uint32_t>(obj.mesh->vertices.size()), 1, 0, 0);

                float factor = _engine->GetCurrentFrame() / 50.f;
                transform.matrix *= glm::translate(glm::mat4 {1.f},
                    glm::vec3 {cos(factor), sin(factor + i), cos(i + factor / 2) / 10.f}
                );
                transform.matrix *= glm::rotate(glm::mat4 {1.f}, 
                    i / 100.f, glm::vec3 {0.f, 0.f, 1.f}
                );

                i++;
            }

            _engine->EndFrame(perframe);
        }
    }
}