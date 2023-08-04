#pragma once

#include <vector>
#include <unordered_map>
#include <SDL2/SDL.h>
#include "vulkan.h"
#include "mesh.h"
#include "texture.h"
#include "renderable.h"
#include "upload_context.h"

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
    const int MAX_OBJECTS = 10000;

    struct Perframe {
        vk::Device device;
        vk::Fence queueSubmitFence;
        vk::CommandPool primaryCommandPool;
        vk::CommandBuffer primaryCommandBuffer;
        vk::Semaphore swapchainAcquireSemaphore;
        vk::Semaphore swapchainReleaseSemaphore;

        // Buffer that holds a GPUCameraData to use when rendering
        AllocatedBuffer cameraBuffer;
        AllocatedBuffer objectBuffer;
        vk::DescriptorSet objectDescriptor;
        vk::DescriptorSet globalDescriptor;
        uint32_t queueIndex;

        /**
         * Index of the Perframe in the engine's array.
         */
        uint32_t perframeIndex;
    };

    class Engine {

    public:
        AllocatedBuffer sceneParamsBuffer;
        SDL_Window* window;
        Perframe* currentPerframe;

        Engine();
        ~Engine();
        void Init();
        void Update(const std::vector<Renderable> &objects);
        uint64_t GetCurrentFrame();
        void WaitIdle();

        Mesh* GetMesh(const std::string& name);
        AllocatedImage* GetImage(const std::string& name);
        Material* GetMaterial(const std::string& name);
        Mesh* CreateMesh(const std::string& name);
        Mesh* CreateMesh(const std::string& name, Mesh mesh);
        Texture* CreateTexture(const std::string& name, const std::string& path);
        void BindTexture(Material* material, const std::string& name);
        Material* CreateMaterial(vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string &name);
        void InitGui();
        vk::Result MapMemory(vma::Allocation allocation, void **data);
        void UnmapMemory(vma::Allocation allocation);


        /**
         * Write CPU data to device-local memory. If allocation is HOST_VISIBLE, map and write directly,
         * but if not, create a staging buffer. 
         */
        void UploadMemory(AllocatedBuffer buffer, const void * data, size_t offset, size_t size);

        /**
         * Write an image to device-local memory.
         */
        void UploadImage(AllocatedImage image, void * pixels);

        AllocatedBuffer CreateBuffer(size_t size,
            vk::BufferUsageFlags bufferUsage,
            vma::AllocationCreateFlags preferredFlags,
            vk::MemoryPropertyFlags requiredFlags,
            vma::MemoryUsage memoryUsage);
        AllocatedImage CreateImage(vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage);

        void DestroyBuffer(AllocatedBuffer buffer);
        void DestroyDescriptorPool(vk::DescriptorPool descriptorPool);


        Perframe* BeginFrame();
        void BeginRenderPass();
        void EndRenderPass();
        Perframe* CurrentFrame();
        void DrawObjects(vk::CommandBuffer cmd, const Renderable* first, size_t count);
        void EndFrame(Perframe *perframe);
        void Render();
        std::pair<uint32_t, uint32_t> GetWindowSize();
        size_t PadUniformBufferSize(size_t originalSize);

    private:

        uint64_t _currentFrame = 0;

        vk::Instance _instance;
#ifndef NDEBUG
        vk::DebugUtilsMessengerEXT _debugMessenger;
#endif
        uint32_t _graphicsQueueIndex;
        vk::PhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        vk::PhysicalDeviceProperties _physicalDeviceProperties;
        vk::Device _device;
        vk::SurfaceKHR _surface;
        vk::SwapchainKHR _swapchain;
        vk::Queue _queue;
        vk::Format _swapchainFormat;
        vk::Extent2D _swapchainDimensions;
        vk::RenderPass _renderPass;
        vk::PipelineLayout _pipelineLayout;
        vk::Pipeline _pipeline;
        vk::DescriptorSetLayout _globalSetLayout;
        vk::DescriptorSetLayout _objectSetLayout;
        vk::DescriptorSetLayout _singleTextureSetLayout;
        vk::DescriptorPool _descriptorPool;
        vk::DescriptorPool _imguiPool;
        vk::CommandPool _commandPool;
        vma::Allocator _allocator; // AMD Vulkan memory allocator

        // Depth Testing 
        vk::ImageView _depthImageView;
        vk::Format _depthFormat;
        AllocatedImage _depthImage;

        UploadContext _uploadContext;

        std::vector<Perframe> _perframes;
        std::vector<vk::ImageView> _swapchainImageViews;
        std::vector<vk::Framebuffer> _swapchainFramebuffers;
        std::vector<vk::Semaphore> _recycledSemaphores;
        std::vector<Renderable> _renderables;
        std::unordered_map<std::string, Material> _materials;
        std::unordered_map<std::string, Texture> _textures;
        std::unordered_map<std::string, Mesh> _meshes;

        GPUSceneData _sceneParameters;

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
        void InitSceneBuffer();
        void InitDescriptorSetLayouts();

        /**
         * A descriptor points shaders to data from program
         */
        void InitDescriptors();

        void InitUploadContext();
        void InitPipeline();
        void InitRenderPass();
        void InitFramebuffers();
        void InitAllocator();

        void CloseVulkan();
        void TeardownSwapchain();
        void TeardownPerframe(Perframe &perframe);
        void TeardownDescriptors();
        void TeardownFramebuffers();

        vk::Result AcquireNextImage(uint32_t *index);
        vk::Result Present(Perframe *perframe);
        void Resize();

        vk::DebugUtilsMessengerCreateInfoEXT GetDebugUtilsMessengerCreateInfo();
        vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
        vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
        vk::ShaderModule LoadShaderModule(const char *path);
    };
};
