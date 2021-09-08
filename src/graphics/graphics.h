#pragma once

#include <vector>
#include <SDL2/SDL.h>
#include "vulkan.h"
#include "vma.hpp"
#include "mesh.h"
#include "renderable.h"

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
        struct Perframe {
            vk::Device device;
            vk::Fence queueSubmitFence;
            vk::CommandPool primaryCommandPool;
            vk::CommandBuffer primaryCommandBuffer;
            vk::Semaphore swapchainAcquireSemaphore;
            vk::Semaphore swapchainReleaseSemaphore;
            uint32_t queueIndex;
            uint32_t imageIndex;
        };

        Engine();
        ~Engine();
        void Init();
        void Update(const std::vector<Renderable> &objects);
        uint64_t GetCurrentFrame();
        void WaitIdle();

        Mesh* CreateMesh(const std::string& name);
        Mesh* GetMesh(const std::string& name);
        Material* CreateMaterial(vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string &name);
        Material* GetMaterial(const std::string& name);

        Perframe* BeginFrame();
        vk::Result DrawFrame(uint32_t index, const std::vector<Renderable> &objects);
        void DrawObjects(vk::CommandBuffer cmd, const Renderable* first, size_t count);
        void EndFrame(Perframe *perframe);
        std::pair<uint32_t, uint32_t> GetWindowSize();

    private:

        uint64_t _currentFrame = 0;

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
        vma::Allocator _allocator; // AMD Vulkan memory allocator

        // Depth Testing 
        vk::ImageView _depthImageView;
        vk::Format _depthFormat;
        AllocatedImage _depthImage;

        std::vector<Perframe> _perframes;
        std::vector<vk::ImageView> _swapchainImageViews;
        std::vector<vk::Framebuffer> _swapchainFramebuffers;
        std::vector<vk::Semaphore> _recycledSemaphores;
        std::vector<Renderable> _renderables;
        std::unordered_map<std::string, Material> _materials;
        std::unordered_map<std::string, Mesh> _meshes;
        Mesh _mesh;



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
        void InitPerframe(Perframe &perframe, uint32_t index);
        void InitPipeline();
        void InitRenderPass();
        void InitFramebuffers();
        void InitAllocator();

        void CloseVulkan();
        void TeardownPerframe(Perframe &perframe);
        void TeardownFramebuffers();

        vk::Result AcquireNextImage(uint32_t *index);
        vk::Result Present(Perframe *perframe);
        void Resize(uint32_t width, uint32_t height);

        vk::DebugUtilsMessengerCreateInfoEXT GetDebugUtilsMessengerCreateInfo();
        vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
        vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
        vk::ShaderModule LoadShaderModule(const char *path);
    };
};
