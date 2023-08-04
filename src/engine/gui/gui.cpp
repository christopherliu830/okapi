#include "gui.h"
#include "graphics/vulkan.h"

namespace Gui {

    void Gui::Init() {
    };

    void Gui::Render() {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), _engine.currentPerframe->primaryCommandBuffer);
    }

    void Gui::PollEvents(const SDL_Event &event) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void Gui::BeginFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_engine.window);
        ImGui::NewFrame();
    }
};