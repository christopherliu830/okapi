#ifndef ENTITY_SYSTEM_H
#define ENTITY_SYSTEM_H

#include <entt/entt.hpp>

class EntitySystem {
    virtual void update(entt::registry& registry, float deltaTime) = 0;
};

#endif
