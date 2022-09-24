#pragma once

#include "entity_system.h"
#include "renderable.h"
#include "transform.h"
#include "graphics.h"

namespace Graphics {

    class RenderSystem : EntitySystem {

    public:
        RenderSystem(Engine& engine): _engine{engine} {};
        void Update(entt::registry &registry, float deltaTime = 0) override;

        Material* CreateMaterial(vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string &name);
        Material* GetMaterial(const std::string& name);
        Mesh* GetMesh(const std::string& name);

    private:
        Engine& _engine;
        std::vector<Renderable> _renderables;
        std::unordered_map<std::string, Material> _materials;
        std::unordered_map<std::string, Mesh> _meshes;

        vk::Result AcquireNextImage(uint32_t *index);
        vk::Result Present(uint32_t index);
        vk::Result DrawFrame(uint32_t index, const std::vector<Renderable> &objects);
    };
};
