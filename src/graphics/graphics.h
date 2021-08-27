#pragma once

#if defined(_WIN32)
#  define VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__APPLE__)
#  define VK_USE_PLATFORM_MACOS_MVK
#endif

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_EXCEPTIONS

#include <vector>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

// I don't remember what this layer does
const std::vector<const char*> gValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
    const bool gEnableValidationLayers = false;
#else
    const bool gEnableValidationLayers = true;
#endif


#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

namespace Graphics {
    class Engine {

    public:
        Engine();
        ~Engine();
        void Init();
        void Update();
        void WaitIdle();

    private:
        struct Perframe {
            vk::Device device;
            vk::Fence queueSubmitFence;
            vk::CommandPool primaryCommandPool;
            vk::CommandBuffer primaryCommandBuffer;
            vk::Semaphore swapchainAcquireSemaphore;
            vk::Semaphore swapchainReleaseSemaphore;
            int32_t queueIndex;
        };

        SDL_Window* _window;
        vk::Instance _instance;
#ifndef NDEBUG
        vk::DebugUtilsMessengerEXT _debugMessenger;
#endif
        uint32_t _graphicsQueueIndex;
        vk::PhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        vk::Device _device;
        vk::SurfaceKHR _surface;
        vk::SwapchainKHR _swapchain;
        vk::Queue _queue;
        vk::Format _swapchainFormat;
        vk::Extent2D _swapchainDimensions;
        vk::RenderPass _renderPass;
        vk::PipelineLayout _pipelineLayout;
        vk::Pipeline _pipeline;
        VmaAllocator _allocator;
        std::vector<Perframe> _perframes;
        std::vector<vk::ImageView> _swapchainImageViews;
        std::vector<vk::Framebuffer> _swapchainFramebuffers;
        std::vector<vk::Semaphore> _recycledSemaphores;
        std::vector<VkSemaphore> imageAvailable;
        std::vector<VkSemaphore> renderFinished;
        size_t currentFrame = 0;



        void CreateDispatcher();
        void CreateDebugUtilsMessenger();
        void InitVulkan();
        void InitVkInstance(
            const std::vector<const char *> &requiredValidationLayers,
            const std::vector<const char *> &requiredInstanceExtensions
        );
        void CreateSurface();
        void InitPhysicalDeviceAndSurface();
        void InitLogicalDevice(const std::vector<const char *> &requiredDeviceExtensions);
        void InitSwapchain();
        void InitPerframe(Perframe &perframe);
        void InitPipeline();
        void InitRenderPass();
        void InitFramebuffers();
        void InitAllocator();

        void CloseVulkan();
        void TeardownPerframe(Perframe &perframe);
        void TeardownFramebuffers();

        vk::Result AcquireNextImage(uint32_t *index);
        vk::Result Present(uint32_t index);
        void Resize(uint32_t width, uint32_t height);

        vk::DebugUtilsMessengerCreateInfoEXT GetDebugUtilsMessengerCreateInfo();
        vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
        vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
        vk::ShaderModule LoadShaderModule(const char *path);
        bool AreRequiredExtensionsPresent(
            std::vector<const char *> required,
            std::vector<vk::ExtensionProperties> &available
        );
        bool AreRequiredValidationLayersPresent(
            std::vector<const char *> required,
            std::vector<vk::LayerProperties> &available
        );
    };
};