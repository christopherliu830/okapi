#pragma once

#include "graphics.h"

namespace Graphics {

    class Renderer {
        Engine* _engine;
        Renderer(Engine* engine): _engine{engine} {}
        bool BeginFrame(Swapchain swapchain);
        void Render();
        void EndFrame();
    }

}