#pragma once

#include "graphics.h"
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

namespace Gui {

    /**
     * Wrapper for DearImgui.
     */
    class Gui {

    public:
        Gui(Graphics::Engine* engine) : _engine{engine} {};

        void Init();
        void Render();
        void BeginFrame();
        void PollEvents(const SDL_Event &event);
    
    private:
        Graphics::Engine* _engine;
    };
};