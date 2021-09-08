#ifndef ENTITY_SYSTEM_H
#define ENTITY_SYSTEM_H

#include <entt/entt.hpp>

class EntitySystem {
    virtual void Update(entt::registry& registry, float deltaTime) = 0;
};

#endif
