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
        Perframe* perframe = _engine->BeginFrame();

        auto [width, height] = _engine->GetWindowSize();
        glm::vec3 cameraPos {0.f, -6.f, -10.f};
        glm::mat4 viewMatrix = glm::translate(glm::mat4{1.f}, cameraPos);
        glm::mat4 projection = glm::perspective(
            glm::radians(70.f), 
            (float)width / (float)height,
            0.1f,
            200.f
        );
        projection[1][1] *= -1;

        GPUCameraData camData;
        camData.proj = projection;
        camData.view = viewMatrix;
        camData.viewProj = projection * viewMatrix;

        if (perframe) {
            void* data = _engine->MapMemory(perframe->cameraBuffer->allocation);
            memcpy(data, &camData, sizeof(GPUCameraData));
            _engine->UnmapMemory(perframe->cameraBuffer->allocation);

            vk::CommandBuffer cmd = *perframe->primaryCommandBuffer;

            Mesh* lastMesh = nullptr;
            Material* lastMaterial = nullptr;

            for(auto [entity, transform, obj]: view.each()) {
                glm::mat4 mvp = projection * viewMatrix * transform.matrix;

                if (obj.material != lastMaterial) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline);
                    lastMaterial = obj.material;
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        obj.material->pipelineLayout, 0, 
                        { perframe->globalDescriptor.front() }, {}
                    );
                }

                MeshPushConstants mvpMatrix;
                mvpMatrix.renderMatrix = transform.matrix;

                cmd.pushConstants(
                    obj.material->pipelineLayout,
                    vk::ShaderStageFlagBits::eVertex,
                    0, sizeof(MeshPushConstants), &mvpMatrix
                );

                if (obj.mesh != lastMesh) {
                    vk::DeviceSize offset = 0;
                    cmd.bindVertexBuffers(0, 1, &obj.mesh->vertexBuffer->buffer, &offset);
                    lastMesh = obj.mesh;
                }

                cmd.draw(static_cast<uint32_t>(obj.mesh->vertices.size()), 1, 0, 0);
            }

            _engine->EndFrame(perframe);
        }
    }
}