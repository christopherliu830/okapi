#pragma once

#include "graphics.h"
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

namespace Gui {

    class GuiSystem {
        GuiSystem(Graphics::Engine engine) : _engine{engine} {};

    public:

        void Init();
        void Render();
    
    private:
        Graphics::Engine _engine;

    };
};