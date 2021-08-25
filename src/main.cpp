#include <entt/entt.hpp>
#include <string>
#include <iostream>
#include <glm/vec3.hpp> // glm::vec3
#include "free_list.h"
#include "actor.h"
#include "graphics/graphics.h"
#include "renderable.h"
#include "logging.h"


struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

// class GravitySystem : EntitySystem {
class GravitySystem {
public:
    void update(entt::registry &registry, float deltaTime) {
        auto view = registry.view<Position, Velocity>();
        view.each([](auto &pos, auto &vel) {
            vel.dx += 0.01f;
            pos.x += vel.dx;
            pos.y += vel.dy;
        });
    }
};


int main(int argc, char* args[] ) {
#ifdef NDEBUG
    std::cout << "Running in NDEBUG mode" << std::endl;
#else
    std::cout << "Running in DEBUG mode" << std::endl;
#endif

    bool quit = false;
    Graphics::Engine graphics;
    SDL_Event e;

    entt::registry registry;
    GravitySystem gs;
    RenderSystem rs;

    for(auto i = 0u; i < 10u; i++) {
        const auto entity = registry.create();
        registry.emplace<Position>(entity, i * 1.f, i * 1.f);
        if (i % 2 == 0) { registry.emplace<Velocity>(entity, i * .1f, i * .1f); }
    }


    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }
        gs.update(registry, 0);
        rs.update(registry, 0);
        SDL_Delay((int)(1.f/60.f*1000.f));
        graphics.Update();
    }

    graphics.WaitIdle();
    return 0;
}
