#define VMA_IMPLEMENTATION

#include "graphics.h"
#include "util.h"
#include "logging.h"
#include "types.h"
#include "mesh.h"
#include "vma.h"
#include "pipeline.h"
#include "renderable.h"
#include <vector>
#include <iostream>
#include <set>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace Graphics {
    Engine::Engine() { Init(); }

    Engine::~Engine() {
        CloseVulkan();
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    void Engine::CloseVulkan() {
        VK_CHECK(_device->waitIdle());

        for(auto &mesh : _meshes) {
            mesh.second.Destroy();
        }

        _meshes.clear();

        _allocator.destroyImage(_depthImage.image, _depthImage.allocation);

        TeardownFramebuffers();
        for(auto &perframe: _perframes) {
            TeardownPerframe(perframe);
        }

        _perframes.clear();

        if (_allocator) {
            _allocator.destroy();
        }


        // for(auto &semaphore: _recycledSemaphores) {
        //     _device->destroySemaphore(semaphore);
        // }

        // if (_pipeline) {
        //     _device->destroyPipeline(_pipeline);
        // }

        // if (_pipelineLayout) {
        //     _device->destroyPipelineLayout(_pipelineLayout);
        // }
    }

    void Engine::Init() {
        // Init SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOGE("Could not intialize sdl2: {}", SDL_GetError());
        }

        _window = SDL_CreateWindow(
            "Space Crawler 0.1.0",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (_window == NULL) {
            fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
            assert(0);
        }

        InitVulkan();
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
        InitDescriptors();
        InitPipeline();
        InitFramebuffers();
    }

    void Engine::InitVkInstance(
        const std::vector<const char *> &requiredValidationLayers,
        const std::vector<const char *> &requiredInstanceExtensions
    ) {
        vk::ApplicationInfo applicationInfo("Space Crawler", 1, "No Engine", 1, VK_API_VERSION_1_1);
        vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);

        auto [rEnumerateInstance, instanceExtensions] = vk::enumerateInstanceExtensionProperties(); 
        VK_CHECK(rEnumerateInstance);

        std::vector<const char *> activeInstanceExtensions {requiredInstanceExtensions};

    #ifndef NDEBUG
        activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        activeInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_MACOS_MVK)
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

        vk::Result result;
        std::tie(result, _instance) = vk::createInstanceUnique(instanceCreateInfo).asTuple();
        VK_CHECK(result);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance);
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
        std::tie(result, _debugMessenger) = _instance->createDebugUtilsMessengerEXTUnique(GetDebugUtilsMessengerCreateInfo()).asTuple();
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
        std::tie(result, gpus) = _instance->enumeratePhysicalDevices();
        VK_CHECK(result);

        for (const auto& gpu : gpus) {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = gpu.getQueueFamilyProperties();
            assert(!queueFamilyProperties.empty());

            if (_surface) {
                _instance->destroySurfaceKHR(*_surface);
            }

            // Call SDL to reassign surface
            CreateSurface();

            uint32_t count = static_cast<uint32_t>(queueFamilyProperties.size());
            for(uint32_t i = 0; i < count; i++) {
                vk::Bool32 supportsPresent;
                std::tie(result, supportsPresent) = gpu.getSurfaceSupportKHR(i, *_surface);
                VK_CHECK(result);

                // Get queue family with graphics and present capabilities
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && supportsPresent) {
                    _graphicsQueueIndex = i;
                }
            }

            _physicalDevice = gpu;
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

        std::tie(result, _device) = _physicalDevice.createDeviceUnique({{}, queueCreateInfo, {}, extensions}).asTuple();
        VK_CHECK(result);

        _queue = _device->getQueue(_graphicsQueueIndex, 0);
    }

    void Engine::CreateSurface() {
        vk::SurfaceKHR surface;
        assert(SDL_Vulkan_CreateSurface(_window, *_instance, (VkSurfaceKHR *)(&surface)) == SDL_TRUE);
        _surface = vk::UniqueSurfaceKHR {surface, *_instance};
    }

    void Engine::InitSwapchain() {
        vk::Result result;

        vk::SurfaceCapabilitiesKHR surfaceProperties;
        std::tie(result, surfaceProperties) = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
        VK_CHECK(result);

        std::vector<vk::SurfaceFormatKHR> formats;
        std::tie(result, formats) = _physicalDevice.getSurfaceFormatsKHR(*_surface);
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
        std::tie(result, presentModes) = _physicalDevice.getSurfacePresentModesKHR(*_surface);
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

        vk::SwapchainKHR oldSwapchain = *_swapchain;

        // Tear down old swapchain
        if (oldSwapchain) {
            std::vector<vk::Image> swapchainImages;
            std::tie(result, swapchainImages) = _device->getSwapchainImagesKHR(oldSwapchain);
            VK_CHECK(result);
            size_t imageCount = swapchainImages.size();
            for (size_t i = 0; i < imageCount; i++)
            {
                TeardownPerframe(_perframes[i]);
            }

            _swapchainImageViews.clear();
        }

        std::tie(result, _swapchain) = _device->createSwapchainKHRUnique({
            {}, // Flags
            *_surface,
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
            *_swapchain 
        }).asTuple();
        VK_CHECK(result);

        _swapchainDimensions = swapchainSize;
        _swapchainFormat = format.format;

        std::vector<vk::Image> swapchainImages;
        std::tie(result, swapchainImages) = _device->getSwapchainImagesKHR(*_swapchain);
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
        vma::AllocationCreateInfo depthAllocInfo {{}, vma::MemoryUsage::eGpuOnly, vk::MemoryPropertyFlagBits::eDeviceLocal};
        std::pair<vk::Image, vma::Allocation> imageAlloc;
        std::tie(result, imageAlloc) = _allocator.createImage(depthBuffer, depthAllocInfo);
        VK_CHECK(result);
        _depthImage.image = imageAlloc.first;
        _depthImage.allocation = imageAlloc.second;

        vk::ImageViewCreateInfo depthViewInfo {};
        depthViewInfo.image = _depthImage.image;
        depthViewInfo.viewType = vk::ImageViewType::e2D;
        depthViewInfo.format = _depthFormat;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.layerCount = 0;
        depthViewInfo.subresourceRange.layerCount = 1;
        depthViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        std::tie(result, _depthImageView) = _device->createImageViewUnique(depthViewInfo).asTuple();
        VK_CHECK(result);

        for(size_t i = 0; i < imageCount; i++) {
            viewCreateInfo.image = swapchainImages[i];
            auto [result, imageView] = _device->createImageViewUnique(viewCreateInfo);
            VK_CHECK(result);
            _swapchainImageViews.push_back(std::move(imageView));
        }
    }

    vk::PresentModeKHR Engine::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Engine::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            SDL_Vulkan_GetDrawableSize(_window, &width, &height);
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
        std::tie(result, perframe.queueSubmitFence) = _device->createFenceUnique(
            {vk::FenceCreateFlagBits::eSignaled}
        ).asTuple();
        assert(result == vk::Result::eSuccess);

        vk::CommandPoolCreateInfo cmdPoolInfo {
            vk::CommandPoolCreateFlagBits::eTransient |
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            _graphicsQueueIndex
        };

        std::tie(result, perframe.primaryCommandPool) = _device->createCommandPoolUnique(cmdPoolInfo).asTuple();
        assert(result == vk::Result::eSuccess);

        vk::CommandBufferAllocateInfo cmdBufInfo {
            *perframe.primaryCommandPool,
            vk::CommandBufferLevel::ePrimary,
            1,
        };

        std::vector<vk::UniqueCommandBuffer> cmdBuf;
        std::tie(result, cmdBuf) = _device->allocateCommandBuffersUnique(cmdBufInfo).asTuple();
        VK_CHECK(result);
        perframe.primaryCommandBuffer = std::move(cmdBuf.front());

        perframe.cameraBuffer = std::make_unique<AllocatedBuffer>(
            _allocator,
            sizeof(GPUCameraData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vma::MemoryUsage::eCpuToGpu
        );

        perframe.queueIndex = _graphicsQueueIndex;
        perframe.imageIndex = index;
    }

    void Engine::TeardownPerframe(Perframe &perframe) {
        // if (perframe.cameraBuffer->buffer) {
        //     _allocator.destroyBuffer(perframe.cameraBuffer->buffer, perframe.cameraBuffer->allocation);
        // }

        // if (perframe.queueSubmitFence) {
        //     _device->destroyFence(perframe.queueSubmitFence);
        //     perframe.queueSubmitFence = nullptr;
        // }
        // if (perframe.primaryCommandBuffer) {
        //     _device->freeCommandBuffers(perframe.primaryCommandPool, perframe.primaryCommandBuffer);
        //     perframe.primaryCommandBuffer = nullptr;
        // }
        // if (perframe.primaryCommandPool) {
        //     _device->destroyCommandPool(perframe.primaryCommandPool);
        //     perframe.primaryCommandPool = nullptr;
        // }
        // if (perframe.swapchainAcquireSemaphore) {
        //     _device->destroySemaphore(perframe.swapchainAcquireSemaphore);
        //     perframe.swapchainAcquireSemaphore = nullptr;
        // }
        // if (perframe.swapchainReleaseSemaphore) {
        //     _device->destroySemaphore(perframe.swapchainReleaseSemaphore);
        //     perframe.swapchainReleaseSemaphore = nullptr;
        // }

        perframe.queueIndex = -1;
        perframe.imageIndex = -1;
    }

    void Engine::InitDescriptors() {
        vk::DescriptorSetLayoutBinding camBufferBinding = {
            0, 
            vk::DescriptorType::eUniformBuffer,
            1, // Descriptor count
            vk::ShaderStageFlagBits::eVertex,
            nullptr
        };

        std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {camBufferBinding};
        vk::Result result;
        std::tie(result, _globalSetLayout) = _device->createDescriptorSetLayoutUnique({{}, bindings}).asTuple();
        VK_CHECK(result);

        std::vector<vk::DescriptorPoolSize> sizes = {
            { vk::DescriptorType::eUniformBuffer, 10 }
        };

        vk::DescriptorPoolCreateInfo poolInfo {{}, 10, sizes};

        std::tie(result, _descriptorPool) = _device->createDescriptorPoolUnique(poolInfo).asTuple();
        VK_CHECK(result);

        for(int i  = 0; i < _perframes.size(); i++) {
            // Does not need to be explicitly freed
            std::tie(result, _perframes[i].globalDescriptor) = _device->allocateDescriptorSets({*_descriptorPool, *_globalSetLayout});
            VK_CHECK(result);

            vk::DescriptorBufferInfo bufferInfo {_perframes[i].cameraBuffer->buffer, 0, sizeof(GPUCameraData)};

            _device->updateDescriptorSets({{
                _perframes[i].globalDescriptor.front(),
                0,
                0,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                bufferInfo 
            }}, {});
        }
    }

    void Engine::InitPipeline() {
        vk::Result result;
        PipelineBuilder builder;

        // Create push constant accesible only to vertex shader
        vk::PushConstantRange pushConstant {vk::ShaderStageFlagBits::eVertex, 0, sizeof(MeshPushConstants)};

        // Create a pipeline layout with 1 push constant.
        std::tie(result, _pipelineLayout) = _device->createPipelineLayoutUnique({{}, *_globalSetLayout, pushConstant}).asTuple();
        VK_CHECK(result);

        builder.SetPipelineLayout(*_pipelineLayout);

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

        vk::ShaderModule vertShader = LoadShaderModule("assets/shaders/vert.spv");
        vk::ShaderModule fragShader = LoadShaderModule("assets/shaders/frag.spv");
        builder.AddShaderModule({{}, vk::ShaderStageFlagBits::eVertex, vertShader, "main"});
        builder.AddShaderModule({{}, vk::ShaderStageFlagBits::eFragment, fragShader, "main"});


        std::tie(result, _pipeline) = builder.BuildUnique(*_device, *_renderPass).asTuple();
        VK_CHECK(result);

        _device->destroyShaderModule(vertShader);
        _device->destroyShaderModule(fragShader);
        builder.FlushShaderModules();

        CreateMaterial(*_pipeline, *_pipelineLayout, "defaultMesh");
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

        vk::Result result;
        std::tie(result, _renderPass) = _device->createRenderPassUnique({{}, attachments, subpass, dependency}).asTuple();
        VK_CHECK(result);
    }

    void Engine::InitFramebuffers() {

        for (auto &imageView : _swapchainImageViews) {
            std::array<vk::ImageView, 2> attachments = {*imageView, *_depthImageView};
            vk::FramebufferCreateInfo fbInfo {
                {},
                *_renderPass,
                attachments,
                _swapchainDimensions.width,
                _swapchainDimensions.height,
                1
            };
            fbInfo.attachmentCount = 2;
            auto [result, framebuffer] = _device->createFramebuffer(fbInfo);
            VK_CHECK(result);
            _swapchainFramebuffers.push_back(framebuffer);
        }
    }

    void Engine::InitAllocator() {
        vma::AllocatorCreateInfo allocatorInfo {
            {},
            _physicalDevice,
            *_device
        };

        allocatorInfo.instance = *_instance;
        vk::Result result;
        std::tie(result, _allocator) = vma::createAllocator(allocatorInfo);
        VK_CHECK(result);
    }

    // Returns nullptr if the frame isn't ready yet
    Perframe* Engine::BeginFrame() {
        uint32_t index;
        vk::Result res = AcquireNextImage(&index);

        switch(res) {
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
                Resize(_swapchainDimensions.width, _swapchainDimensions.height);
                return nullptr;
            case vk::Result::eSuccess:
                break;
            default:
                return nullptr;
        }

        // Begin render pass
        auto cmd = *_perframes[index].primaryCommandBuffer;

        vk::CommandBufferBeginInfo beginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        VK_CHECK(cmd.begin(beginInfo));

        vk::ClearValue clearValue;
        clearValue.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.2f, 1.0f}}));

        vk::ClearValue depthClear;
        depthClear.depthStencil.depth = 1.f;

        std::array<vk::ClearValue, 2> clearValues = {clearValue, depthClear};

        vk::RenderPassBeginInfo rpBeginInfo {
            *_renderPass, _swapchainFramebuffers[index],
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

        return &_perframes[index];
    }

    void Engine::EndFrame(Perframe* perframe) {
        auto cmd = *perframe->primaryCommandBuffer;

        cmd.endRenderPass();

        VK_CHECK(cmd.end());

        if (!perframe->swapchainReleaseSemaphore) {
            vk::Result result;
            std::tie(result, perframe->swapchainReleaseSemaphore) = _device->createSemaphoreUnique({}).asTuple();
            VK_CHECK(result);
        }

        vk::PipelineStageFlags waitStage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        vk::SubmitInfo info {
            *perframe->swapchainAcquireSemaphore,
            waitStage, *perframe->primaryCommandBuffer,
            *perframe->swapchainReleaseSemaphore
        };

        VK_CHECK(_queue.submit(info, *perframe->queueSubmitFence));

        vk::Result res = Present(perframe);

        // Handle Outdated error in present.
        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR)
        {
            Resize(_swapchainDimensions.width, _swapchainDimensions.height);
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
        vk::UniqueSemaphore acquireSemaphore;

        if (_recycledSemaphores.empty()) {
            std::tie(result, acquireSemaphore) = _device->createSemaphoreUnique({}).asTuple();
            VK_CHECK(result);
        } else {
            acquireSemaphore = std::move(_recycledSemaphores.back());
            _recycledSemaphores.pop_back();
        }

        std::tie(result, *image) = _device->acquireNextImageKHR(*_swapchain, UINT64_MAX, *acquireSemaphore);

        if (result != vk::Result::eSuccess) {
            _recycledSemaphores.push_back(std::move(acquireSemaphore));
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
        if (*_perframes[*image].queueSubmitFence) {
            VK_CHECK(_device->waitForFences(*_perframes[*image].queueSubmitFence, true, UINT64_MAX));
            VK_CHECK(_device->resetFences(*_perframes[*image].queueSubmitFence));
        }

        if (*_perframes[*image].primaryCommandPool)
        {
            VK_CHECK(_device->resetCommandPool(*_perframes[*image].primaryCommandPool));
        }

        // Release semaphore back into the manager
        if (_perframes[*image].swapchainAcquireSemaphore) {
            _recycledSemaphores.push_back(std::move(_perframes[*image].swapchainAcquireSemaphore));
        }

        _perframes[*image].swapchainAcquireSemaphore = std::move(acquireSemaphore);
        return vk::Result::eSuccess;
    }

    vk::Result Engine::DrawFrame(uint32_t index, const std::vector<Renderable> &objects) {
        // auto fb = _swapchainFramebuffers[index];
        // auto cmd = _perframes[index].primaryCommandBuffer;

        // vk::CommandBufferBeginInfo beginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        // VK_CHECK(cmd.begin(beginInfo));

        // vk::ClearValue clearValue;
        // clearValue.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.2f, 1.0f}}));

        // vk::ClearValue depthClear;
        // depthClear.depthStencil.depth = 1.f;

        // std::array<vk::ClearValue, 2> clearValues = {clearValue, depthClear};

        // vk::RenderPassBeginInfo rpBeginInfo {
        //     *_renderPass, fb, {{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}},
        //     clearValues
        // };

        // cmd.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

        // vk::Viewport vp {
        //     0.0f, 0.0f, 
        //     static_cast<float>(_swapchainDimensions.width), static_cast<float>(_swapchainDimensions.height),
        //     0.0f, 0.1f
        // };
        // cmd.setViewport(0, vp);

        // vk::Rect2D scissor {{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}};
        // cmd.setScissor(0, scissor);

        // DrawObjects(cmd, objects.data(), objects.size());

        // cmd.endRenderPass();

        // VK_CHECK(cmd.end());

        // if (!_perframes[index].swapchainReleaseSemaphore) {
        //     vk::Result result;
        //     std::tie(result, _perframes[index].swapchainReleaseSemaphore) = _device->createSemaphore({});
        //     VK_CHECK(result);
        // }

        // vk::PipelineStageFlags waitStage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        // vk::SubmitInfo info {
        //     _perframes[index].swapchainAcquireSemaphore,
        //     waitStage, cmd,
        //     _perframes[index].swapchainReleaseSemaphore
        // };

        // return _queue.submit(info, _perframes[index].queueSubmitFence);
        return vk::Result::eSuccess;
    }

    vk::Result Engine::Present(Perframe *perframe) {
        vk::PresentInfoKHR present {
            *perframe->swapchainReleaseSemaphore, *_swapchain, perframe->imageIndex
        };
        // Avoid assertion failure on result because we want to
        // bypass assert check on vk::Result::eOutdated and handle manually
        return (vk::Result)vkQueuePresentKHR(_queue, reinterpret_cast<VkPresentInfoKHR *>(&present));
    }

    void Engine::Resize(uint32_t width, uint32_t height) {
        if (!_device) {
            return;
        }

        auto [result, surfaceProperties] = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);

        // Only rebuild the swapchain if the dimensions have changed
        if (surfaceProperties.currentExtent.width == _swapchainDimensions.width &&
            surfaceProperties.currentExtent.height == _swapchainDimensions.height)
        {
            return;
        }

        VK_CHECK(_device->waitIdle());
        TeardownFramebuffers();
        InitSwapchain();
        InitFramebuffers();
    }

    void Engine::WaitIdle() {
        vkDeviceWaitIdle(*_device);
    }

    void Engine::TeardownFramebuffers() {
        VK_CHECK(_queue.waitIdle());
        for(auto &framebuffer : _swapchainFramebuffers) {
            _device->destroyFramebuffer(framebuffer);
        }
        _swapchainFramebuffers.clear();
    }

    AllocatedBuffer Engine::CreateBuffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage memoryUsage) {
        return AllocatedBuffer {_allocator, size, usage, memoryUsage};
    }

    vk::ShaderModule Engine::LoadShaderModule(const char *path) {
        auto spirv = ReadFile(path);
        vk::ShaderModuleCreateInfo moduleInfo(
            {}, 
            static_cast<size_t>(spirv.size()), 
            reinterpret_cast<const uint32_t *>(spirv.data())
        );
        auto [result, mod] = _device->createShaderModule(moduleInfo);
        VK_CHECK(result);
        return mod;
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

    void* Engine::MapMemory(vma::Allocation allocation) {
        auto [result, data] = _allocator.mapMemory(allocation);
        VK_CHECK(result);
        return data;
    }

    void Engine::UnmapMemory(vma::Allocation allocation) {
        _allocator.unmapMemory(allocation);
    }

    Mesh* Engine::GetMesh(const std::string& name) {
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

            // MeshPushConstants mvpMatrix;
            // mvpMatrix.renderMatrix = projection * view * transform.matrix;

            // cmd.pushConstants(
            //     obj.material->pipelineLayout,
            //     vk::ShaderStageFlagBits::eVertex,
            //     0, sizeof(MeshPushConstants), &mvpMatrix
            // );

            if (obj.mesh != lastMesh) {
                vk::DeviceSize offset = 0;
                cmd.bindVertexBuffers(0, 1, &obj.mesh->vertexBuffer->buffer, &offset);
                lastMesh = obj.mesh;
            }

            cmd.draw(static_cast<uint32_t>(obj.mesh->vertices.size()), 1, 0, 0);
        }
    }

    Mesh* Engine::CreateMesh(const std::string &path) {
        Mesh* pMesh = GetMesh(path);
        if (pMesh != nullptr) return GetMesh(path);

        _meshes[path] = Mesh::FromObj("assets/Monkey/monkey.obj", _allocator);
        return &_meshes[path];
    }

    std::pair<uint32_t, uint32_t> Engine::GetWindowSize() {
        return std::pair(_swapchainDimensions.width, _swapchainDimensions.height);
    }

    uint64_t Engine::GetCurrentFrame() {
        return _currentFrame;
    }
};
