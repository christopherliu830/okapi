#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "entity_system.h"

class RenderSystem : EntitySystem {
public:
    RenderSystem();
    void update(entt::registry &registry, float deltaTime) override;
private:
};

#endif
