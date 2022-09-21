#include "actor.h"
#include "graphics/graphics.h"
#include "graphics/render_system.h"
#include "graphics/renderable.h"
#include "logging.h"
#include <entt/entt.hpp>
#include <string>
#include <iostream>
#include <glm/vec3.hpp> // glm::vec3
#include <glm/ext/matrix_transform.hpp>

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

float globalvar = 0;

// class GravitySystem : EntitySystem {
class GravitySystem {
public:
    void Update(entt::registry &registry, float deltaTime) {
        auto view = registry.view<Position, Velocity>();
        view.each([](auto &pos, auto &vel) {
            pos.y = sin(globalvar);
            pos.x = cos(globalvar);
        });

        globalvar += 0.01f;
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
    Graphics::RenderSystem rs {&graphics};
    entt::registry registry;
    GravitySystem gs;
    SDL_Event e;

    Graphics::Mesh* monkeyMesh = graphics.CreateMesh("assets/Monkey/Monkey.obj");
    Graphics::Renderable monkey;
    monkey.mesh = monkeyMesh;
    monkey.material = graphics.GetMaterial("default");

    Graphics::Mesh* lostEmpireMesh = graphics.CreateMesh("assets/lost-empire/lost-empire.obj");
    Graphics::Renderable lostEmpire;
    lostEmpire.mesh = lostEmpireMesh;
    lostEmpire.material = graphics.GetMaterial("default");

    graphics.CreateTexture("assets/lost-empire/lost-empire-RGBA.png");
    graphics.BindTexture(graphics.GetMaterial("default"), "assets/lost-empire/lost-empire-RGBA.png");

    for(auto i = 0u; i < 10u; i++) {
        const auto entity = registry.create();
        registry.emplace<Transform>(entity, glm::mat4 {1.0f});
        registry.emplace<Graphics::Renderable>(entity, monkey);
    }

    const auto entity = registry.create();
    registry.emplace<Transform>(entity, glm::translate(glm::mat4 {1.0f}, glm::vec3 {5, -15, 0}));
    registry.emplace<Graphics::Renderable>(entity, lostEmpire);

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }
        gs.Update(registry, 0);
        rs.Update(registry, 0);
        SDL_Delay((int)(1.f/60.f*1000.f));
    }

    graphics.WaitIdle();
    return 0;
}
