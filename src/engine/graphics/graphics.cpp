#define VMA_IMPLEMENTATION

#include "graphics.h"
#include "util.h"
#include "logging.h"
#include "types.h"
#include "mesh.h"
#include "pipeline.h"
#include "texture.h"
#include "renderable.h"
#include <vector>
#include <iostream>
#include <set>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace Graphics {
    Engine::Engine() { Init(); }

    Engine::~Engine() {
        CloseVulkan();
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void Engine::CloseVulkan() {
        VK_CHECK(_device.waitIdle());

        // Destroy GUI
        _device.destroyDescriptorPool(_imguiPool);
        ImGui_ImplVulkan_Shutdown();

        for(auto &texture : _textures) {
            _allocator.destroyImage(texture.second.image.image, texture.second.image.allocation);
            _device.destroyImageView(texture.second.imageView);
            _device.destroySampler(texture.second.sampler);
        }
        _textures.clear();

        for(auto &mesh : _meshes) {
            _allocator.destroyBuffer(mesh.second.vertexBuffer.buffer, mesh.second.vertexBuffer.allocation);
            mesh.second.Destroy();
        }
        _meshes.clear();

        _allocator.destroyBuffer(sceneParamsBuffer.buffer, sceneParamsBuffer.allocation);

        _allocator.destroyImage(_depthImage.image, _depthImage.allocation);

        _device.destroyImageView(_depthImageView);

        TeardownFramebuffers();
        for(auto &perframe: _perframes) {
            TeardownPerframe(perframe);
        }

        _perframes.clear();

        _allocator.destroy();
        _allocator = nullptr;

        for(auto semaphore: _recycledSemaphores) {
            _device.destroySemaphore(semaphore);
        }

        _device.destroyPipeline(_pipeline);

        _device.destroyPipelineLayout(_pipelineLayout);

        _device.destroyRenderPass(_renderPass);

        _uploadContext.Destroy();

        _device.destroyDescriptorPool(_descriptorPool);
        _device.destroyDescriptorSetLayout(_singleTextureSetLayout);
        _device.destroyDescriptorSetLayout(_objectSetLayout);
        _device.destroyDescriptorSetLayout(_globalSetLayout);

        for(auto imageView : _swapchainImageViews) {
            _device.destroyImageView(imageView);
        }

        _device.destroySwapchainKHR(_swapchain);
        _swapchain = nullptr;

        _instance.destroySurfaceKHR(_surface);
        _surface = nullptr;

        _device.destroy();
        _device = nullptr;

#ifndef NDEBUG
        _instance.destroyDebugUtilsMessengerEXT(_debugMessenger);
        _debugMessenger = nullptr;
#endif

        _instance.destroy();
    }

    void Engine::Init() {
        // Init SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOGE("Could not intialize sdl2: {}", SDL_GetError());
        }

        window = SDL_CreateWindow(
            "Okapi",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (window == NULL) {
            fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
            assert(0);
        }

        InitVulkan();
        InitGui();
    }

    void Engine::InitVulkan() {
        CreateDispatcher();

        InitVkInstance(
            { "VK_LAYER_KHRONOS_validation" },
            { VK_KHR_SURFACE_EXTENSION_NAME }
        );

    #ifndef NDEBUG
        CreateDebugUtilsMessenger();
    #endif

        InitPhysicalDeviceAndSurface();
        InitLogicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
        InitAllocator();
        InitSwapchain();
        InitRenderPass();
        InitSceneBuffer();
        InitDescriptorSetLayouts();
        InitDescriptors();
        InitUploadContext();
        InitPipeline();
        InitFramebuffers();
    }

    void Engine::InitVkInstance(
        const std::vector<const char *> &requiredValidationLayers,
        const std::vector<const char *> &requiredInstanceExtensions
    ) {
        vk::ApplicationInfo applicationInfo("Space Crawler", 1, "No Engine", 1, VK_API_VERSION_1_2);
        vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);

        auto [rEnumerateInstance, instanceExtensions] = vk::enumerateInstanceExtensionProperties(); 
        VK_CHECK(rEnumerateInstance);

        std::vector<const char *> activeInstanceExtensions {requiredInstanceExtensions};

    #ifndef NDEBUG
        activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        activeInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_MACOS_KHR)
        activeInstanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
    #else
        assert(0);
    #endif

        assert(AreRequiredExtensionsPresent(activeInstanceExtensions, instanceExtensions));

        // Check for validation layer support
        auto [rEnumerateLayers, supportedValidationLayers] = vk::enumerateInstanceLayerProperties();
        VK_CHECK(rEnumerateLayers);
        std::vector<const char *> requestedValidationLayers(requiredValidationLayers);
        assert(AreRequiredValidationLayersPresent(requestedValidationLayers, supportedValidationLayers));

        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = activeInstanceExtensions.data();
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requestedValidationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = requestedValidationLayers.data();

    #ifndef NDEBUG
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = GetDebugUtilsMessengerCreateInfo();
        instanceCreateInfo.pNext = &debugUtilsCreateInfo;
    #endif

        vk::Result createInstanceResult;
        std::tie(createInstanceResult, _instance) = vk::createInstance(instanceCreateInfo);
        VK_CHECK(createInstanceResult);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
    }

    void Engine::CreateDispatcher() {
        // https://github.com/KhronosGroup/Vulkan-Hpp#extensions--per-device-function-pointers
        vk::DynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    }

    void Engine::CreateDebugUtilsMessenger() {
#ifndef NDEBUG
        vk::Result result;
        std::tie(result, _debugMessenger) = _instance.createDebugUtilsMessengerEXT(GetDebugUtilsMessengerCreateInfo());
        VK_CHECK(result);
#endif
    }

    vk::DebugUtilsMessengerCreateInfoEXT Engine::GetDebugUtilsMessengerCreateInfo() {
        return { 
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            DebugUtilsMessengerCallback,
        };
    }

    void Engine::InitPhysicalDeviceAndSurface() {
        vk::Result result;
        std::vector<vk::PhysicalDevice> gpus;
        std::tie(result, gpus) = _instance.enumeratePhysicalDevices();
        VK_CHECK(result);

        // find a suitable gpu to use (one with the right queue family)
        for (const auto& gpu : gpus) {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = gpu.getQueueFamilyProperties();
            assert(!queueFamilyProperties.empty());

            if (_surface) {
                _instance.destroySurfaceKHR(_surface);
            }

            // Call SDL to reassign surface
            CreateSurface();

            uint32_t count = static_cast<uint32_t>(queueFamilyProperties.size());
            for(uint32_t i = 0; i < count; i++) {
                vk::Bool32 supportsPresent;
                std::tie(result, supportsPresent) = gpu.getSurfaceSupportKHR(i, _surface);
                VK_CHECK(result);

                // Get queue family with graphics and present capabilities
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && supportsPresent) {
                    _graphicsQueueIndex = i;
                }
            }

            _physicalDevice = gpu;
            _physicalDeviceProperties = gpu.getProperties();
            LOGI("Enabled GPU: {}", _physicalDeviceProperties.deviceName);
            LOGI("Atom Size: {}", _physicalDeviceProperties.limits.nonCoherentAtomSize);
            break;
        }

    }

    void Engine::InitLogicalDevice(const std::vector<const char *> &requiredDeviceExtensions) {
        vk::Result result;
        std::vector<const char *> extensions(requiredDeviceExtensions);
        std::vector<vk::ExtensionProperties> supportedExtensions;
        std::tie(result, supportedExtensions) = _physicalDevice.enumerateDeviceExtensionProperties();

        assert(AreRequiredExtensionsPresent(extensions, supportedExtensions));

        // Portability support. If portability subset is found in device extensions,
        // it must be enabled in required extensions.
        if (std::find_if(
            supportedExtensions.begin(), 
            supportedExtensions.end(),
            [](auto &extension) { 
                return strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0; 
            }
        ) != supportedExtensions.end()) {
            extensions.push_back("VK_KHR_portability_subset");
        };

        float queuePriority = 1.0f;

        vk::DeviceQueueCreateInfo queueCreateInfo {
            {}, // Flags
            _graphicsQueueIndex,
            1, // Queue Count
            &queuePriority
        };

        vk::PhysicalDeviceShaderDrawParametersFeatures shaderFeatures { VK_TRUE };

        vk::DeviceCreateInfo deviceCreateInfo {
            {}, // Flags
            queueCreateInfo,
            {},
            extensions,
            {},
            &shaderFeatures
        };

        std::tie(result, _device) = _physicalDevice.createDevice(deviceCreateInfo);
        VK_CHECK(result);

        _queue = _device.getQueue(_graphicsQueueIndex, 0);
    }

    void Engine::CreateSurface() {
#if defined(VK_USE_PLATFORM_METAL_EXT)
        LOGI("Using Metal");
#endif
        if(SDL_Vulkan_CreateSurface(window, _instance, (VkSurfaceKHR *)(&_surface)) != SDL_TRUE) {
            LOGE(SDL_GetError());
            assert(0);
        }
    }

    void Engine::InitGui() {
        vk::DescriptorPoolSize poolSizes[] {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000},
            { vk::DescriptorType::eSampledImage, 1000},
            { vk::DescriptorType::eStorageImage, 1000},
            { vk::DescriptorType::eUniformTexelBuffer, 1000},
            { vk::DescriptorType::eStorageTexelBuffer, 1000},
            { vk::DescriptorType::eUniformBuffer, 1000},
            { vk::DescriptorType::eStorageBuffer, 1000},
            { vk::DescriptorType::eUniformBufferDynamic, 1000},
            { vk::DescriptorType::eStorageBufferDynamic, 1000},
            { vk::DescriptorType::eInputAttachment, 1000},
        };

        vk::Result result;
        std::tie(result, _imguiPool) = _device.createDescriptorPool({{}, 1000, poolSizes});
        VK_CHECK(result);

        ImGui::CreateContext();

        ImGui_ImplSDL2_InitForVulkan(window);

        ImGui_ImplVulkan_InitInfo initInfo {
            _instance, 
            _physicalDevice,
            _device,
            {},
            _queue,
            {},
            _imguiPool,
            {},
            3,
            3,
            VK_SAMPLE_COUNT_1_BIT
        };

        ImGui_ImplVulkan_Init(&initInfo, _renderPass);

        _uploadContext.Begin();

        ImGui_ImplVulkan_CreateFontsTexture(_uploadContext.cmd);

        _uploadContext.SubmitSync(_queue);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void Engine::InitSwapchain() {
        vk::Result result;

        vk::SurfaceCapabilitiesKHR surfaceProperties;
        std::tie(result, surfaceProperties) = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
        VK_CHECK(result);

        std::vector<vk::SurfaceFormatKHR> formats;
        std::tie(result, formats) = _physicalDevice.getSurfaceFormatsKHR(_surface);
        VK_CHECK(result);

        vk::SurfaceFormatKHR format;
        if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined) {
            // There is no preferred format, just pick one
            format = formats[0];
            format.format = vk::Format::eB8G8R8A8Unorm;
        }
        assert(!formats.empty());

        format.format = vk::Format::eUndefined;
        for(auto &candidate : formats) {
            switch (candidate.format) {
                // Prefer these formats
                case vk::Format::eR8G8B8A8Unorm:
                case vk::Format::eB8G8R8A8Unorm:
                case vk::Format::eA8B8G8R8UnormPack32:
                    format = candidate;
                    break;

                default:
                    break;
            }
            if (format.format != vk::Format::eUndefined) {
                break;
            }
        }

        if (format.format == vk::Format::eUndefined) {
            format = formats[0];
        }

        vk::Extent2D swapchainSize = ChooseSwapExtent(surfaceProperties);

        std::vector<vk::PresentModeKHR> presentModes;
        std::tie(result, presentModes) = _physicalDevice.getSurfacePresentModesKHR(_surface);
        VK_CHECK(result);

        vk::PresentModeKHR swapchainPresentMode = ChooseSwapPresentMode(presentModes);

        // Determine the number of vk::Image's to use in the swapchain.
        // Ideally, we desire to own 1 image at a time, the rest of the images can
        // either be rendered to and/or being queued up for display.
        // Simply sticking to this minimum means that we may sometimes have to wait on the driver to
        // complete internal operations before we can acquire another image to render to.
        // Therefore it is recommended to request at least one more image than the minimum.
        uint32_t swapchainImageCount = surfaceProperties.minImageCount + 1;

        if (surfaceProperties.maxImageCount > 0 && swapchainImageCount > surfaceProperties.maxImageCount) {
            // Settle for less :/
            swapchainImageCount = surfaceProperties.maxImageCount;
        }

        vk::SwapchainKHR oldSwapchain = _swapchain;
        vk::SwapchainCreateInfoKHR swapchainCreateInfo {
            {}, // Flags
            _surface,
            swapchainImageCount,
            format.format,
            format.colorSpace,
            swapchainSize,
            1, // Image Layers
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::SharingMode::eExclusive,
            {},
            surfaceProperties.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            swapchainPresentMode, 
            true, // Clipped
            oldSwapchain
        };

        std::tie(result, _swapchain) = _device.createSwapchainKHR(swapchainCreateInfo);
        VK_CHECK(result);

        // Tear down old swapchain
        if (oldSwapchain) {
            for (vk::ImageView imageView : _swapchainImageViews)
            {
                _device.destroyImageView(imageView);
            }

            std::vector<vk::Image> swapchainImages;
            std::tie(result, swapchainImages) = _device.getSwapchainImagesKHR(oldSwapchain);
            VK_CHECK(result);
            size_t imageCount = swapchainImages.size();
            for (size_t i = 0; i < imageCount; i++)
            {
                TeardownPerframe(_perframes[i]);
            }

            _swapchainImageViews.clear();
            _device.destroySwapchainKHR(oldSwapchain);
        }

        _swapchainDimensions = swapchainSize;
        _swapchainFormat = format.format;

        std::vector<vk::Image> swapchainImages;
        std::tie(result, swapchainImages) = _device.getSwapchainImagesKHR(_swapchain);
        VK_CHECK(result);
        size_t imageCount = swapchainImages.size();

        _perframes.clear();
        _perframes.resize(imageCount);

        for(uint32_t i = 0; i < static_cast<uint32_t>(imageCount); i++) {
            InitPerframe(_perframes[i], i);
        }

        vk::ImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.viewType = vk::ImageViewType::e2D;
        viewCreateInfo.format = _swapchainFormat;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.layerCount = 1;
        viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;

        // Allocate the depth image
        _depthFormat = vk::Format::eD32Sfloat;
        vk::ImageCreateInfo depthBuffer {};
        vk::Extent3D extent = { _swapchainDimensions.width, _swapchainDimensions.height, 1 };
        depthBuffer.imageType = vk::ImageType::e2D;
        depthBuffer.format = _depthFormat;
        depthBuffer.extent = extent;
        depthBuffer.mipLevels = 1;
        depthBuffer.arrayLayers = 1;
        depthBuffer.samples = vk::SampleCountFlagBits::e1,
        depthBuffer.tiling = vk::ImageTiling::eOptimal;
        depthBuffer.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

        vma::AllocationCreateInfo depthAllocInfo {};
        depthAllocInfo.flags = {};
        depthAllocInfo.usage = vma::MemoryUsage::eGpuOnly;
        depthAllocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        VK_CHECK(_allocator.createImage(&depthBuffer,
            &depthAllocInfo,
            &_depthImage.image,
            &_depthImage.allocation,
            &_depthImage.allocInfo));

        vk::ImageViewCreateInfo depthViewInfo {};
        depthViewInfo.image = _depthImage.image;
        depthViewInfo.viewType = vk::ImageViewType::e2D;
        depthViewInfo.format = _depthFormat;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.layerCount = 0;
        depthViewInfo.subresourceRange.layerCount = 1;
        depthViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        std::tie(result, _depthImageView) = _device.createImageView(depthViewInfo);
        VK_CHECK(result);

        for(size_t i = 0; i < imageCount; i++) {
            viewCreateInfo.image = swapchainImages[i];
            auto [result, imageView] = _device.createImageView(viewCreateInfo);
            VK_CHECK(result);
            _swapchainImageViews.push_back(imageView);
        }
    }

    vk::PresentModeKHR Engine::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eImmediate;
    }

    vk::Extent2D Engine::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(
                actualExtent.width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );
            actualExtent.height = std::clamp(
                actualExtent.height,
                capabilities.maxImageExtent.height,
                capabilities.maxImageExtent.height
            );

            return actualExtent;
        }
    }

    void Engine::InitPerframe(Perframe &perframe, uint32_t index) {
        vk::Result result;
        std::tie(result, perframe.queueSubmitFence) = _device.createFence({vk::FenceCreateFlagBits::eSignaled});
        assert(result == vk::Result::eSuccess);

        vk::CommandPoolCreateInfo cmdPoolInfo {
            vk::CommandPoolCreateFlagBits::eTransient |
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            _graphicsQueueIndex
        };

        std::tie(result, perframe.primaryCommandPool) = _device.createCommandPool(cmdPoolInfo);
        assert(result == vk::Result::eSuccess);

        vk::CommandBufferAllocateInfo cmdBufInfo{
            perframe.primaryCommandPool,
            vk::CommandBufferLevel::ePrimary,
            1,
        };

        std::vector<vk::CommandBuffer> cmdBuf;
        std::tie(result, cmdBuf) = _device.allocateCommandBuffers(cmdBufInfo);
        assert(result == vk::Result::eSuccess);
        perframe.primaryCommandBuffer = cmdBuf.front();

        perframe.cameraBuffer = CreateBuffer(
            sizeof(GPUCameraData),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead,
            {},
            vma::MemoryUsage::eAuto
        );

        perframe.objectBuffer = CreateBuffer(
            sizeof(GPUObjectData) * MAX_OBJECTS,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
            {},
            vma::MemoryUsage::eAuto
        );

        perframe.device = _device;
        perframe.queueIndex = _graphicsQueueIndex;
        perframe.perframeIndex = index;
    }

    void Engine::TeardownPerframe(Perframe &perframe) {

        _allocator.destroyBuffer(perframe.objectBuffer.buffer, perframe.objectBuffer.allocation);
        _allocator.destroyBuffer(perframe.cameraBuffer.buffer, perframe.cameraBuffer.allocation);

        _device.destroyFence(perframe.queueSubmitFence);
        perframe.queueSubmitFence = nullptr;

        _device.freeCommandBuffers(perframe.primaryCommandPool, perframe.primaryCommandBuffer);
        perframe.primaryCommandBuffer = nullptr;

        _device.destroyCommandPool(perframe.primaryCommandPool);
        perframe.primaryCommandPool = nullptr;

        _device.destroySemaphore(perframe.swapchainAcquireSemaphore);
        perframe.swapchainAcquireSemaphore = nullptr;

        _device.destroySemaphore(perframe.swapchainReleaseSemaphore);
        perframe.swapchainReleaseSemaphore = nullptr;

        perframe.device = nullptr;
        perframe.queueIndex = -1;
        perframe.perframeIndex = -1;
    }

    void Engine::InitDescriptorSetLayouts() {
        vk::Result result;

        // Global information descriptor set layout
        vk::DescriptorSetLayoutBinding cameraBinding {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex};
        vk::DescriptorSetLayoutBinding sceneBinding {1, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment};
        vk::DescriptorSetLayoutBinding bindings[] = {cameraBinding, sceneBinding};
        std::tie(result, _globalSetLayout) = _device.createDescriptorSetLayout({{}, bindings});
        VK_CHECK(result);

        // Object descriptor set layout
        vk::DescriptorSetLayoutBinding objectBinding = {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex};
        std::tie(result, _objectSetLayout) = _device.createDescriptorSetLayout({{}, objectBinding});
        VK_CHECK(result);

        vk::DescriptorSetLayoutBinding textureBinding {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment};
        std::tie(result, _singleTextureSetLayout) = _device.createDescriptorSetLayout({{}, textureBinding});
        VK_CHECK(result);
    }

    void Engine::InitDescriptors() {
        vk::Result result;
        const int MAX_OBJECTS = 10000;

        // Create descriptor pool
        std::vector<vk::DescriptorPoolSize> sizes = {
            { vk::DescriptorType::eUniformBuffer, 10 },
            { vk::DescriptorType::eUniformBufferDynamic, 10 },
            { vk::DescriptorType::eStorageBuffer, 10 },
            { vk::DescriptorType::eCombinedImageSampler, 10 }
        };

        vk::DescriptorPoolCreateInfo poolInfo {{}, 10, sizes};

        std::tie(result, _descriptorPool) = _device.createDescriptorPool(poolInfo);
        VK_CHECK(result);

        for(int i  = 0; i < _perframes.size(); i++) {
            // Does not need to be explicitly freed
            std::vector<vk::DescriptorSet> descriptors;
            std::tie(result, descriptors) = _device.allocateDescriptorSets({_descriptorPool, _globalSetLayout});
            VK_CHECK(result);
            _perframes[i].globalDescriptor = descriptors[0];

            std::vector<vk::DescriptorSet> objectDescriptors;
            std::tie(result, objectDescriptors) = _device.allocateDescriptorSets({_descriptorPool, _objectSetLayout});
            VK_CHECK(result);
            _perframes[i].objectDescriptor = objectDescriptors[0];

            // point the descriptor set to the buffers
            vk::DescriptorBufferInfo cameraBufferInfo {_perframes[i].cameraBuffer.buffer, 0, sizeof(GPUCameraData)};
            vk::DescriptorBufferInfo sceneBufferInfo {sceneParamsBuffer.buffer, 0, sizeof(GPUSceneData)};
            vk::DescriptorBufferInfo objectBufferInfo {_perframes[i].objectBuffer.buffer, 0, sizeof(GPUObjectData) * MAX_OBJECTS};

            vk::WriteDescriptorSet setWrites[] = {
                {_perframes[i].globalDescriptor, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, cameraBufferInfo},
                {_perframes[i].globalDescriptor, 1, 0, vk::DescriptorType::eUniformBufferDynamic, nullptr, sceneBufferInfo},
                {_perframes[i].objectDescriptor, 0, 0, vk::DescriptorType::eStorageBuffer, nullptr, objectBufferInfo}
            };

            _device.updateDescriptorSets(setWrites, {});
        }
    }

    void Engine::InitUploadContext() {
        _uploadContext.Init(_device, _graphicsQueueIndex);
    }

    void Engine::InitPipeline() {
        vk::Result result;
        PipelineBuilder builder;

        // Create push constant accesible only to vertex shader
        vk::PushConstantRange pushConstant {vk::ShaderStageFlagBits::eVertex, 0, sizeof(MeshPushConstants)};


        // Create a pipeline layout with 1 push constant.
        vk::DescriptorSetLayout setLayouts[] = {_globalSetLayout, _objectSetLayout, _singleTextureSetLayout};
        std::tie(result, _pipelineLayout) = _device.createPipelineLayout({{}, setLayouts, pushConstant});
        VK_CHECK(result);

        builder.SetPipelineLayout(_pipelineLayout);

        VertexInputDescription vertexInputDescription = Vertex::GetInputDescription();

        builder.SetVertexInput({{},
            static_cast<uint32_t>(vertexInputDescription.bindings.size()),
            vertexInputDescription.bindings.data(),
            static_cast<uint32_t>(vertexInputDescription.attributes.size()),
            vertexInputDescription.attributes.data()
        });

        // Specify use triangle lists
        builder.SetInputAssembly({{}, vk::PrimitiveTopology::eTriangleList});

        vk::PipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.cullMode = vk::CullModeFlagBits::eNone;
        rasterizer.frontFace = vk::FrontFace::eClockwise;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        builder.SetRasterizer(rasterizer);

        // enable color blending.
        vk::PipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | 
                                            vk::ColorComponentFlagBits::eG | 
                                            vk::ColorComponentFlagBits::eB | 
                                            vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
        builder.SetColorBlendState({{}, {}, {}, colorBlendAttachment});

        vk::PipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        builder.SetDepthStencil(depthStencil);

        // We have one viewport and one scissor.
        builder.SetViewport({{}, 1, {}, 1, {}});
        builder.SetMultisample({{}, vk::SampleCountFlagBits::e1});

        // Specify that these states will be dynamic, i.e. not part of pipeline state object.
        std::array<vk::DynamicState, 2> dynamics {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        builder.SetDynamicState({ {}, dynamics });

        vk::ShaderModule vertShader = LoadShaderModule("assets/shaders/shader.vert.spv");
        vk::ShaderModule fragShader = LoadShaderModule("assets/shaders/shader.frag.spv");
        builder.AddShaderModule({{}, vk::ShaderStageFlagBits::eVertex, vertShader, "main"});
        builder.AddShaderModule({{}, vk::ShaderStageFlagBits::eFragment, fragShader, "main"});

        std::tie(result, _pipeline) = builder.Build(_device, _renderPass);
        VK_CHECK(result);

        _device.destroyShaderModule(vertShader);
        _device.destroyShaderModule(fragShader);
        builder.FlushShaderModules();

        CreateMaterial(_pipeline, _pipelineLayout, "default");
    }

    void Engine::InitRenderPass() {

        // Describe the color attachment that this render pass will use
        vk::AttachmentDescription colorAttachment {};
        // Backbuffer format
        colorAttachment.format = _swapchainFormat;
        // Not multisampled
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        // When starting frame, clear tiles
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        // When ending frame, write out tiles
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        // Not using stencils
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        // On render pass begin, image layout undefined
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        // After render pass is complete, transition to PresentSrcKHR layout.
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        // One subpass, this subpass has one color attachment.
        // While executing this subpass, the attachment will be in attachment optimal layout.
        vk::AttachmentReference colorRef {0, vk::ImageLayout::eColorAttachmentOptimal};

        // Create the depth attachment
        vk::AttachmentDescription depthAttachment {{}, _depthFormat};
        depthAttachment.format = _depthFormat;
        depthAttachment.samples = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare; 
        depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        vk::AttachmentReference depthRef = {1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

        // We will end up with two transitions.
        // The first one happens right before we start subpass #0, where
        // eUndefined is transitioned into eColorAttachmentOptimal.
        // The final layout in the render pass attachment states ePresentSrcKHR, so we
        // will get a final transition from eColorAttachmentOptimal to ePresetSrcKHR.
        vk::SubpassDescription subpass {{}, 
            vk::PipelineBindPoint::eGraphics, 
            {},
            colorRef,
            {},
            &depthRef
        };

        // Create a dependency to external events.
        // We need to wait for the WSI semaphore to signal.
        // Only pipeline stages which depend on eColorAttachmentOutput will
        // actually wait for the semaphore, so we must also wait for that pipeline stage.
        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        // Since we changed the image layout, we need to make the memory visible to
        // color attachment to modify.
        dependency.srcAccessMask = {};
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        vk::RenderPassCreateInfo renderPassCreateInfo {{}, attachments, subpass, dependency};

        vk::Result result;
        std::tie(result, _renderPass) = _device.createRenderPass(renderPassCreateInfo);
        VK_CHECK(result);
    }

    void Engine::InitSceneBuffer() {
        // Create global scene parameters buffer
        const size_t sceneParametersBufferSize = _perframes.size() * PadUniformBufferSize(sizeof(GPUSceneData));
        sceneParamsBuffer = CreateBuffer(
            sceneParametersBufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            {},
            {},
            vma::MemoryUsage::eCpuToGpu
        );
    }

    void Engine::InitFramebuffers() {

        for (auto &imageView : _swapchainImageViews) {
            std::array<vk::ImageView, 2> attachments = {imageView, _depthImageView};
            vk::FramebufferCreateInfo fbInfo {
                {},
                _renderPass,
                attachments,
                _swapchainDimensions.width,
                _swapchainDimensions.height,
                1
            };
            auto [result, framebuffer] = _device.createFramebuffer(fbInfo);
            VK_CHECK(result);
            _swapchainFramebuffers.push_back(framebuffer);
        }
    }

    void Engine::InitAllocator() {

        vma::VulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

        vma::AllocatorCreateInfo allocatorInfo {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorInfo.physicalDevice = _physicalDevice;
        allocatorInfo.device = _device;
        allocatorInfo.instance = _instance;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions; 

        vk::Result result;
        std::tie(result, _allocator) = vma::createAllocator(allocatorInfo);
        VK_CHECK(result);

    }

    void Engine::BeginRenderPass() {
        auto cmd = currentPerframe->primaryCommandBuffer;

        vk::ClearValue clearValue;
        clearValue.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.2f, 1.0f}}));

        vk::ClearValue depthClear;
        depthClear.depthStencil.depth = 1.f;

        std::array<vk::ClearValue, 2> clearValues = {clearValue, depthClear};

        vk::RenderPassBeginInfo rpBeginInfo {
            _renderPass, _swapchainFramebuffers[currentPerframe->perframeIndex],
            {{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}},
            clearValues
        };

        cmd.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

        vk::Viewport vp {
            0.0f, 0.0f, 
            static_cast<float>(_swapchainDimensions.width), static_cast<float>(_swapchainDimensions.height),
            0.0f, 0.1f
        };
        cmd.setViewport(0, vp);

        vk::Rect2D scissor {{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}};
        cmd.setScissor(0, scissor);
    }

    void Engine::EndRenderPass() {
        currentPerframe->primaryCommandBuffer.endRenderPass();
    }

    // Returns nullptr if the frame isn't ready yet
    Perframe* Engine::BeginFrame() {
        currentPerframe = nullptr;

        uint32_t index;
        vk::Result res = AcquireNextImage(&index);

        switch(res) {
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
                Resize();
                return nullptr;
            case vk::Result::eSuccess:
                break;
            default:
                return nullptr;
        }

        // Begin render pass
        auto cmd = _perframes[index].primaryCommandBuffer;

        vk::CommandBufferBeginInfo beginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        VK_CHECK(cmd.begin(beginInfo));

        currentPerframe = &_perframes[index];

        BeginRenderPass();

        return currentPerframe;
    }

    void Engine::Render() {
        auto perframe = currentPerframe;
        auto cmd = perframe->primaryCommandBuffer;

        EndRenderPass();

        VK_CHECK(cmd.end());

        // If the perframe release semaphore wasn't created yet, initialize it now.
        if (!perframe->swapchainReleaseSemaphore) {
            vk::Result result;
            std::tie(result, perframe->swapchainReleaseSemaphore) = _device.createSemaphore({});
            VK_CHECK(result);
        }

        vk::PipelineStageFlags waitStage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        vk::SubmitInfo info {
            perframe->swapchainAcquireSemaphore, // Wait Semaphores
            waitStage, // Wait Stage Mask
            perframe->primaryCommandBuffer, // Command Buffer 
            perframe->swapchainReleaseSemaphore // Signal Semaphores
        };

        VK_CHECK(_queue.submit(info, perframe->queueSubmitFence));

        vk::Result res = Present(perframe);

        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR)
        {
            Resize();
        }
        else if (res != vk::Result::eSuccess)
        {
            LOGE("Failed to present swapchain image.");
            VK_CHECK(res);
        }

        _currentFrame++;
    }

    void Engine::EndFrame(Perframe* perframe) {
        auto cmd = perframe->primaryCommandBuffer;

        cmd.endRenderPass();

        VK_CHECK(cmd.end());

        if (!perframe->swapchainReleaseSemaphore) {
            vk::Result result;
            std::tie(result, perframe->swapchainReleaseSemaphore) = _device.createSemaphore({});
            VK_CHECK(result);
        }

        vk::PipelineStageFlags waitStage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        vk::SubmitInfo info {
            perframe->swapchainAcquireSemaphore,
            waitStage, perframe->primaryCommandBuffer,
            perframe->swapchainReleaseSemaphore
        };

        VK_CHECK(_queue.submit(info, perframe->queueSubmitFence));

        vk::Result res = Present(perframe);

        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR)
        {
            Resize();
        }
        else if (res != vk::Result::eSuccess)
        {
            LOGE("Failed to present swapchain image.");
            VK_CHECK(res);
        }

        _currentFrame++;
    }

    vk::Result Engine::AcquireNextImage(uint32_t *image) {
        vk::Result result;
        vk::Semaphore acquireSemaphore;

        if (_recycledSemaphores.empty()) {
            std::tie(result, acquireSemaphore) = _device.createSemaphore({});
            VK_CHECK(result);
        } else {
            acquireSemaphore = _recycledSemaphores.back();
            _recycledSemaphores.pop_back();
        }

        std::tie(result, *image) = _device.acquireNextImageKHR(_swapchain, UINT64_MAX, acquireSemaphore);

        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            _recycledSemaphores.push_back(acquireSemaphore);
            return result;
        }

        // If we have outstanding fences for this swapchain image, wait for them to complete first.
        // After begin frame returns, it is safe to reuse or delete resources which
        // were used previously.
        //
        // We wait for fences which completes N frames earlier, so we do not stall,
        // waiting for all GPU work to complete before this returns.
        // Normally, this doesn't really block at all,
        // since we're waiting for old frames to have been completed, but just in case.
        if (_perframes[*image].queueSubmitFence) {
            VK_CHECK(_device.waitForFences(_perframes[*image].queueSubmitFence, true, UINT64_MAX));
            VK_CHECK(_device.resetFences(_perframes[*image].queueSubmitFence));
        }

        if (_perframes[*image].primaryCommandPool)
        {
            VK_CHECK(_device.resetCommandPool(_perframes[*image].primaryCommandPool));
        }

        // Release semaphore back into the manager
        vk::Semaphore oldSemaphore = _perframes[*image].swapchainAcquireSemaphore;
        if (oldSemaphore) {
            _recycledSemaphores.push_back(oldSemaphore);
        }

        _perframes[*image].swapchainAcquireSemaphore = acquireSemaphore;
        return vk::Result::eSuccess;
    }

    vk::Result Engine::Present(Perframe *perframe) {
        vk::PresentInfoKHR present {
            perframe->swapchainReleaseSemaphore, _swapchain, perframe->perframeIndex
        };
        // Avoid assertion failure on result because we want to
        // bypass assert check on vk::Result::eOutdated and handle manually
        return (vk::Result)vkQueuePresentKHR(_queue, reinterpret_cast<VkPresentInfoKHR *>(&present));
    }

    void Engine::Resize() {
        if (!_device) {
            return;
        }

        auto [result, surfaceProperties] = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);

        // Only rebuild the swapchain if the dimensions have changed
        if (surfaceProperties.currentExtent.width == _swapchainDimensions.width &&
            surfaceProperties.currentExtent.height == _swapchainDimensions.height)
        {
            return;
        }

        VK_CHECK(_device.waitIdle());

        _device.destroyImageView(_depthImageView);
        _allocator.destroyImage(_depthImage.image, _depthImage.allocation);

        TeardownFramebuffers();
        InitSwapchain();
        InitFramebuffers();
        InitDescriptors();
    }

    void Engine::WaitIdle() {
        vkDeviceWaitIdle(_device);
    }

    void Engine::TeardownFramebuffers() {
        VK_CHECK(_queue.waitIdle());
        for(auto &framebuffer : _swapchainFramebuffers) {
            _device.destroyFramebuffer(framebuffer);
        }
        _swapchainFramebuffers.clear();
    }

    AllocatedBuffer Engine::CreateBuffer(
        size_t size,
        vk::BufferUsageFlags bufferUsage,
        vma::AllocationCreateFlags preferredFlags,
        vk::MemoryPropertyFlags requiredFlags,
        vma::MemoryUsage memoryUsage
    ) {
        AllocatedBuffer buffer;

        vk::BufferCreateInfo bufferCreateInfo {};
        bufferCreateInfo.flags = {};
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = bufferUsage;
        bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

        vma::AllocationCreateInfo allocationCreateInfo {};
        allocationCreateInfo.flags = preferredFlags;
        allocationCreateInfo.usage = memoryUsage;
        allocationCreateInfo.requiredFlags = requiredFlags;

        VK_CHECK(_allocator.createBuffer(
            &bufferCreateInfo,
            &allocationCreateInfo,
            &buffer.buffer,
            &buffer.allocation,
            &buffer.allocInfo
        ));

        return buffer;
    }

    AllocatedImage Engine::CreateImage(vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage) {
        vma::AllocationCreateInfo allocInfo {};
        AllocatedImage image;
        allocInfo.usage = vma::MemoryUsage::eAuto;

        vk::ImageCreateInfo imageInfo {
            {},
            vk::ImageType::e2D,
            format,
            extent,
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            usage
        };

        auto [result, pair] = _allocator.createImage(imageInfo, allocInfo, &image.allocInfo);
        VK_CHECK(result);
        image.image = pair.first;
        image.allocation = pair.second;
        image.format = format;
        image.extent = extent;

        return image;
    }

    vk::ShaderModule Engine::LoadShaderModule(const char *path) {
        auto spirv = ReadFile(path);
        vk::ShaderModuleCreateInfo moduleInfo(
            {}, 
            static_cast<size_t>(spirv.size()), 
            reinterpret_cast<const uint32_t *>(spirv.data())
        );
        auto [result, mod] = _device.createShaderModule(moduleInfo);
        VK_CHECK(result);
        return mod;
    }

    size_t Engine::PadUniformBufferSize(size_t originalSize) {
        // Calculated required alignment based on minimum device offset alignment
        size_t minUboAlignment = _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
        size_t alignedSize = originalSize;
        if (minUboAlignment > 0) {
            alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }
        return alignedSize;
    }

    Material* Engine::CreateMaterial(vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name) {
        Material mat;
        mat.pipeline = pipeline;
        mat.pipelineLayout = layout;
        _materials[name] = mat;
        return &_materials[name];
    }

    Material* Engine::GetMaterial(const std::string& name) {
        auto it = _materials.find(name);
        if (it == _materials.end()) {
            return nullptr;
        } else {
            return &(*it).second;
        }
    }

    vk::Result Engine::MapMemory(vma::Allocation allocation, void **pData) {
        vk::Result result = _allocator.mapMemory(allocation, pData);
        VK_CHECK(result);
        return result;
    }

    void Engine::UnmapMemory(vma::Allocation allocation) {
        _allocator.flushAllocation(allocation, 0, VK_WHOLE_SIZE);
        _allocator.unmapMemory(allocation);
    }

    void Engine::UploadMemory(AllocatedBuffer buffer, const void * data, size_t offset, size_t size) {
        vk::MemoryPropertyFlags memPropFlags = _allocator.getAllocationMemoryProperties(buffer.allocation);
        
        if(memPropFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            // Allocation ended up in a mappable memory and is already mapped - write to it directly.
            if (buffer.allocInfo.pMappedData) {
                memcpy(buffer.allocInfo.pMappedData, data, size);
            }
            else {
                void * dest;
                MapMemory(buffer.allocation, &dest);
                dest = reinterpret_cast<char *>(dest) + offset;
                memcpy(dest, data, size);
                UnmapMemory(buffer.allocation);
            }
        }
        else {
            // Allocation ended up in non-mappable memory - need to transfer.

            _uploadContext.Begin();

            AllocatedBuffer stagingBuf = CreateBuffer(
                size,
                vk::BufferUsageFlagBits::eTransferSrc,
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                {},
                vma::MemoryUsage::eAuto
            );

            char * dest = reinterpret_cast<char *>(stagingBuf.allocInfo.pMappedData) + offset;
            memcpy(dest, data, size);

            vk::BufferCopy bufCopy = { 0, 0, size };
            _uploadContext.cmd.copyBuffer(stagingBuf.buffer, buffer.buffer, 1, &bufCopy);

            _uploadContext.SubmitSync(_queue);

            _allocator.destroyBuffer(stagingBuf.buffer, stagingBuf.allocation);
        }
    }

    void Engine::UploadImage(AllocatedImage image, void * pixels) {

        vk::ImageSubresourceRange range {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        // Specify the image transformation to occur between the sides of the pipeline barrier.
        vk::ImageMemoryBarrier imageBarrierToTransfer {
            {},
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            {},
            {},
            image.image,
            range
        };

        _uploadContext.Begin();

        // Ensure that writes from TopOfPipe are available for read from the Transfer stage
        _uploadContext.cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            {},
            {},
            imageBarrierToTransfer
        );
        size_t imageSize = image.extent.width * image.extent.height * 4;

        AllocatedBuffer stagingBuffer = CreateBuffer(imageSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
            {},
            vma::MemoryUsage::eAuto
        );

        memcpy(stagingBuffer.allocInfo.pMappedData, pixels, static_cast<size_t>(imageSize));

        vk::BufferImageCopy copyRegion {0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {}, image.extent};

        _uploadContext.cmd.copyBufferToImage(stagingBuffer.buffer, image.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);

        // Specify the image transformation to occur between the sides of the pipeline barrier.
        vk::ImageMemoryBarrier imageBarrierToReadable {
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            {},
            {},
            image.image,
            range
        };

        _uploadContext.cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            {},
            {},
            imageBarrierToReadable
        );

        _uploadContext.SubmitSync(_queue);

        DestroyBuffer(stagingBuffer);
    }

    void Engine::DestroyBuffer(AllocatedBuffer buffer) {
        _allocator.destroyBuffer(buffer.buffer, buffer.allocation);
    }   

    Mesh* Engine::GetMesh(const std::string &name) {
        auto it = _meshes.find(name);
        if (it == _meshes.end()) {
            return nullptr;
        } else {
            return &(*it).second;
        }
    }

    void Engine::DrawObjects(vk::CommandBuffer cmd, const Renderable* first, size_t count) {
        glm::vec3 cameraPos {0.f, -6.f, -10.f};
        glm::mat4 view = glm::translate(glm::mat4{1.f}, cameraPos);
        glm::mat4 projection = glm::perspective(
            glm::radians(70.f), 
            (float)(_swapchainDimensions.width / (float)_swapchainDimensions.height),
            0.1f,
            200.f
        );
        projection[1][1] *= -1;

        Mesh* lastMesh = nullptr;
        Material* lastMaterial = nullptr;

        for(size_t i = 0; i < count; i++) {
            const Renderable& obj = first[i];

            if (obj.material != lastMaterial) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline);
                lastMaterial = obj.material;
            }

            if (obj.mesh != lastMesh) {
                vk::DeviceSize offset = 0;
                cmd.bindVertexBuffers(0, 1, &obj.mesh->vertexBuffer.buffer, &offset);
                lastMesh = obj.mesh;
            }

            cmd.draw(static_cast<uint32_t>(obj.mesh->vertices.size()), 1, 0, 0);
        }
    }

    Mesh* Engine::CreateMesh(const std::string &path) {
        Mesh* pMesh = GetMesh(path);
        if (pMesh != nullptr) return pMesh;

        auto [result, mesh] = Mesh::FromObj(*this, path);
        if (!result) return nullptr;

        return CreateMesh(path, mesh);
    }

    Mesh* Engine::CreateMesh(const std::string &name, Mesh mesh) {
        Mesh* pMesh = GetMesh(name);
        if (pMesh != nullptr) return nullptr;

        mesh.vertexBuffer = CreateBuffer(
            mesh.vertices.size() * sizeof(Vertex), 
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vma::AllocationCreateFlagBits::eDedicatedMemory,
            {},
            vma::MemoryUsage::eAuto
        );

        UploadMemory(mesh.vertexBuffer, mesh.vertices.data(), 0, mesh.GetVertexBufferSize());

        _meshes[name] = mesh;

        return &_meshes[name];
    }

    Texture* Engine::CreateTexture(const std::string &name, const std::string &path) {
        vk::Result result;

        Texture texture;

        Util::LoadImageFromFile(*this, path.c_str(), texture.image);

        vk::ImageViewCreateInfo imageInfo {
            {},
            texture.image.image,
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Srgb
        };

        imageInfo.subresourceRange.levelCount = 1;
        imageInfo.subresourceRange.layerCount = 1;
        imageInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;

        std::tie(result, texture.imageView) = _device.createImageView(imageInfo);
        VK_CHECK(result);

        std::tie(result, texture.sampler) = _device.createSampler({});
        VK_CHECK(result);

        _textures[name] = texture;

        return &_textures[name];
    }

    void Engine::BindTexture(Material* material, const std::string &name) {
        vk::Result result;
        Texture* texture = &_textures[name];

        std::vector<vk::DescriptorSet> descriptors;
        std::tie(result, descriptors) = _device.allocateDescriptorSets({_descriptorPool, _singleTextureSetLayout});
        VK_CHECK(result);
        material->textureDescriptor = descriptors[0];

        vk::DescriptorImageInfo imageBufferInfo { texture->sampler, texture->imageView, vk::ImageLayout::eShaderReadOnlyOptimal };
        vk::WriteDescriptorSet writeTextureDescriptor { material->textureDescriptor, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageBufferInfo };
        _device.updateDescriptorSets(writeTextureDescriptor, {});
    }

    std::pair<uint32_t, uint32_t> Engine::GetWindowSize() {
        return std::pair(_swapchainDimensions.width, _swapchainDimensions.height);
    }

    uint64_t Engine::GetCurrentFrame() {
        return _currentFrame;
    }
};
