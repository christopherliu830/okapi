#include "cube.h"
#include "graphics/graphics.h"
#include "graphics/render_system.h"
#include "graphics/renderable.h"
#include "gui/gui.h"
#include "input.h"
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
    Graphics::RenderSystem renderSystem {graphics};

    Input input { false };

    Gui::Gui gui {graphics};

    entt::registry registry;
    GravitySystem gravitySystem;


    Graphics::Mesh* monkeyMesh = graphics.CreateMesh("assets/Monkey/Monkey.obj");
    Graphics::Renderable monkey;
    monkey.mesh = monkeyMesh;
    monkey.material = graphics.GetMaterial("default");

    Graphics::Mesh* lostEmpireMesh = graphics.CreateMesh("assets/lost-empire/lost-empire.obj");
    Graphics::Renderable lostEmpire;
    lostEmpire.mesh = lostEmpireMesh;
    lostEmpire.material = graphics.GetMaterial("default");

    graphics.CreateTexture("lost-empire", "assets/lost-empire/lost-empire-RGBA.png");
    graphics.BindTexture(graphics.GetMaterial("default"), "lost-empire");

    Primitives::Cube cube { graphics };

    std::vector<entt::entity> entities;
    for(auto i = 0u; i < 10u; i++) {
        const auto entity = registry.create();
        registry.emplace<Transform>(entity, glm::translate(glm::mat4 {1.0f}, glm::vec3 {cos(i * .31415*2) * 5, sin(i * .31415*2) * 5, 0}));
        registry.emplace<Graphics::Renderable>(entity, cube.renderable);
        entities.push_back(entity);
    }

    const auto entity = registry.create();
    registry.emplace<Transform>(entity, glm::translate(glm::mat4 {1.0f}, glm::vec3 {5, -15, 0}));
    registry.emplace<Graphics::Renderable>(entity, lostEmpire);

    gui.Init();

    SDL_Event e;
    while (!quit) {

        // Poll events until there are no more events on the event queue.
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }

            gui.PollEvents(e);
            input.Parse(e);
        }

        gravitySystem.Update(registry, 0);

        if (graphics.BeginFrame() == nullptr) {
            // The graphics system isn't ready to begin a frame yet.
            continue;
        }
        gui.BeginFrame();

        renderSystem.Update(registry, 0);

        gui.Render();
        graphics.Render();

        auto view = registry.view<Transform>();

        for(auto entity : entities) {
            Transform& transform = view.get<Transform>(entity);
            transform.matrix = glm::rotate(transform.matrix, 0.1f, glm::vec3(0.5f, 0.5f, 0.5f));
        }

        input.Reset();
        SDL_Delay((int)(input.KeyDown ? 0 : 1.f/60.f*1000.f));
    }

    graphics.WaitIdle();
    return 0;
}
