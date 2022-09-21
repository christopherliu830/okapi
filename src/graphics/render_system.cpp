#include "graphics.h"
#include "render_system.h"
#include "renderable.h"
#include "transform.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>
#include <assert.h>

float currentTime = 0;

namespace Graphics {
    void RenderSystem::Update(entt::registry &registry, float deltaTime) {
        auto view = registry.view<Transform, Renderable>();
        Perframe* perframe = _engine->BeginFrame();

        auto [width, height] = _engine->GetWindowSize();
        glm::vec3 cameraPos {0.f, 0.f, -10.f};
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

        GPUSceneData sceneData;
        float x = (1 + sin(currentTime)) / 2;
        float y = (1 + sin(currentTime + 3)) / 2;
        float z = (1 + sin(currentTime + 7)) / 2;
        sceneData.ambientColor = glm::vec4 {x, y, z, 1};

        if (perframe) {
            vk::CommandBuffer cmd = perframe->primaryCommandBuffer;

            // Map buffers
            _engine->UploadMemory(perframe->cameraBuffer, &camData, 0, sizeof(GPUCameraData));
            _engine->UploadMemory(
                _engine->sceneParamsBuffer,
                &sceneData,
                _engine->PadUniformBufferSize(sizeof(GPUSceneData)) * perframe->imageIndex,
                sizeof(GPUSceneData)
            );

            Mesh* lastMesh = nullptr;
            Material* lastMaterial = nullptr;

            assert(perframe->imageIndex < UINT_MAX); // Just in case, should never have so many frames
            uint32_t uniformOffset = static_cast<unsigned int>(_engine->PadUniformBufferSize(sizeof(GPUSceneData)) * perframe->imageIndex);

            int index = 0;
            for(auto [entity, transform, obj]: view.each()) {
                // Bind the per-object data
                _engine->UploadMemory(perframe->objectBuffer, &transform.matrix, index * sizeof(GPUObjectData), sizeof(GPUObjectData));

                if (obj.material != lastMaterial) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline);
                    lastMaterial = obj.material;
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        obj.material->pipelineLayout,
                        0, 
                        { perframe->globalDescriptor },
                        uniformOffset
                    );
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        obj.material->pipelineLayout,
                        1,
                        perframe->objectDescriptor,
                        {}
                    );
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        obj.material->pipelineLayout,
                        2,
                        obj.material->textureSet,
                        {}
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
                    cmd.bindVertexBuffers(0, { obj.mesh->vertexBuffer.buffer }, { offset });
                    lastMesh = obj.mesh;
                }

                cmd.draw(static_cast<uint32_t>(obj.mesh->vertices.size()), 1, 0, index);
                index += 1;
            }
            _engine->EndFrame(perframe);
            currentTime += 0.01f;
        }
    }
}