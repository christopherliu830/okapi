#pragma once

#include "graphics.h"
#include <SDL2/SDL.h>

namespace Graphics {

    class Swapchain {
    
    public:

        struct Image {
            vk::Image image;
            vk::ImageView imageView;
        };

        vk::SwapchainKHR swapchain;
        std::vector<Image> images;
        vk::Format format;
        uint32_t width, height;
        uint32_t imageCount;

        Swapchain(
            vk::PhysicalDevice gpu,
            vk::Device device,
            vk::SurfaceKHR surface,
            uint32_t index,
            vk::Format format,
            vk::SwapchainKHR oldSwapchain);
        void Destroy();

        vk::Extent2D GetExtent(const vk::SurfaceCapabilitiesKHR &capabilities);
        vk::PresentModeKHR GetPresentMode(vk::PhysicalDevice gpu, vk::SurfaceKHR surface);
    };

    vk::SurfaceKHR GetSurface();
    vk::SurfaceFormatKHR GetFormat(vk::PhysicalDevice device, vk::SurfaceKHR surface);
};