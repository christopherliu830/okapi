#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/sdl.h>
#include <vulkan/vulkan.hpp>

class Screen {

public:
    Screen();
    ~Screen();

    void update();


private:
    void vulkanInit();
    bool checkValidationLayerSupport();

    SDL_Window* window;
    SDL_Surface* windowSurface;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
};

#endif