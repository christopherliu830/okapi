#include "graphics.h"
#include "util.h"
#include "logging.h"
#include <vector>
#include <iostream>
#include <set>
#include <SDL2/SDL_vulkan.h>
#include "vk_mem_alloc.h"

#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace Graphics {
    // https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/samples/utils/utils.cpp
    VkBool32 DebugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
    void * /*pUserData*/ ) {
    #ifndef NDEBUG
        if ( pCallbackData->messageIdNumber == 648835635 )
        {
            // UNASSIGNED-khronos-Validation-debug-build-warning-message
            return VK_FALSE;
        }
        if ( pCallbackData->messageIdNumber == 767975156 )
        {
            // UNASSIGNED-BestPractices-vkCreateInstance-specialuse-extension
            return VK_FALSE;
        }
    #endif

        std::cerr << vk::to_string( static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( messageSeverity ) ) << ": "
                    << vk::to_string( static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( messageTypes ) ) << ":\n";
        std::cerr << "\t"
                    << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
        std::cerr << "\t"
                    << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
        std::cerr << "\t"
                    << "message         = <" << pCallbackData->pMessage << ">\n";
        if ( 0 < pCallbackData->queueLabelCount )
        {
            std::cerr << "\t"
                    << "Queue Labels:\n";
            for ( uint8_t i = 0; i < pCallbackData->queueLabelCount; i++ )
            {
            std::cerr << "\t\t"
                        << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
            }
        }
        if ( 0 < pCallbackData->cmdBufLabelCount )
        {
            std::cerr << "\t"
                    << "CommandBuffer Labels:\n";
            for ( uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++ )
            {
            std::cerr << "\t\t"
                        << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
            }
        }
        if ( 0 < pCallbackData->objectCount )
        {
            std::cerr << "\t"
                    << "Objects:\n";
            for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
            {
            std::cerr << "\t\t"
                        << "Object " << i << "\n";
            std::cerr << "\t\t\t"
                        << "objectType   = "
                        << vk::to_string( static_cast<vk::ObjectType>( pCallbackData->pObjects[i].objectType ) ) << "\n";
            std::cerr << "\t\t\t"
                        << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if ( pCallbackData->pObjects[i].pObjectName )
            {
                std::cerr << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
            }
        }
        return VK_TRUE;
    }

    Engine::Engine() {
        Init();
    }

    Engine::~Engine() {
        CloseVulkan();
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    void Engine::CloseVulkan() {
        VK_CHECK(_device.waitIdle());

        TeardownFramebuffers();
        for(auto &perframe: _perframes) {
            TeardownPerframe(perframe);
        }

        _perframes.clear();

        for(auto semaphore: _recycledSemaphores) {
            _device.destroySemaphore(semaphore);
        }

        if (_pipeline) {
            _device.destroyPipeline(_pipeline);
        }

        if (_pipelineLayout) {
            _device.destroyPipelineLayout(_pipelineLayout);
        }

        if (_renderPass) {
            _device.destroyRenderPass(_renderPass);
        }

        for(auto imageView : _swapchainImageViews) {
            _device.destroyImageView(imageView);
        }

        if (_swapchain) {
            _device.destroySwapchainKHR(_swapchain);
            _swapchain = nullptr;
        }

        if (_surface) {
            _instance.destroySurfaceKHR(_surface);
            _surface = nullptr;
        }

        if (_device) {
            _device.destroy();
            _device = nullptr;
        }

#ifndef NDEBUG
        if (_debugMessenger) {
            _instance.destroyDebugUtilsMessengerEXT(_debugMessenger);
            _debugMessenger = nullptr;
        }
#endif

        _instance.destroy();
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
        InitSwapchain();
        InitRenderPass();
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

        std::vector<const char *> activeInstanceExtensions{requiredInstanceExtensions};

    #ifdef _DEBUG
        activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        activeInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #else
    #       pragma error Platform not supported
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

    /**
     * @brief Validates a list of required layers, comparing it with the available ones.
     *
     * @param required A vector containing required layer names.
     * @param available A VkLayerProperties object containing available layers.
     * @return true if all required extensions are available
     * @return false otherwise
     */
    bool Engine::AreRequiredExtensionsPresent(
        std::vector<const char *> required,
        std::vector<vk::ExtensionProperties> &available
    ) {

        for(auto extension : required) {
            bool extensionFound = false;
            for (const auto& availableExtension : available) {
                if (strcmp(extension, availableExtension.extensionName) == 0) {
                    extensionFound = true;
                    break;
                }
            }
            if (!extensionFound) return false;
        }
        return true;
    }

    bool Engine::AreRequiredValidationLayersPresent(
        std::vector<const char *> required,
        std::vector<vk::LayerProperties> &available
    ) {

        for(auto layer : required) {
            bool layerFound = false;
            for (const auto& availableLayer : available) {
                std::cout << availableLayer.layerName << std::endl;
                if (strcmp(layer, availableLayer.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) return false;
        }
        return true;
    }

    void Engine::InitPhysicalDeviceAndSurface() {
        vk::Result result;
        std::vector<vk::PhysicalDevice> gpus;
        std::tie(result, gpus) = _instance.enumeratePhysicalDevices();
        VK_CHECK(result);

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
            break;
        }
    }

    void Engine::InitLogicalDevice(const std::vector<const char *> &requiredDeviceExtensions) {
        vk::Result result;

        std::vector<vk::ExtensionProperties> deviceExtensions;
        std::tie(result, deviceExtensions) = _physicalDevice.enumerateDeviceExtensionProperties();
        assert(AreRequiredExtensionsPresent(requiredDeviceExtensions, deviceExtensions));

        float queuePriority = 1.0f;

        vk::DeviceQueueCreateInfo queueCreateInfo {
            {}, // Flags
            _graphicsQueueIndex,
            1, // Queue Count
            &queuePriority
        };

        vk::DeviceCreateInfo deviceCreateInfo {
            {}, // Flags
            queueCreateInfo,
            {},
            requiredDeviceExtensions
        };

        vk::Result rCreateDevice;
        std::tie(rCreateDevice, _device) = _physicalDevice.createDevice(deviceCreateInfo);
        VK_CHECK(rCreateDevice);

        _queue = _device.getQueue(_graphicsQueueIndex, 0);
    }

    void Engine::CreateSurface() {
        assert(SDL_Vulkan_CreateSurface(_window, _instance, (VkSurfaceKHR *)(&_surface)) == SDL_TRUE);
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
        vk::SwapchainCreateInfoKHR swapchainCreateInfo{
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

        for(size_t i = 0; i < imageCount; i++) {
            InitPerframe(_perframes[i]);
        }

        vk::ImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.viewType = vk::ImageViewType::e2D;
        viewCreateInfo.format = _swapchainFormat;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.layerCount = 1;
        viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewCreateInfo.components = {
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eG,
            vk::ComponentSwizzle::eB,
            vk::ComponentSwizzle::eA,
        };

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

    void Engine::InitPerframe(Perframe &perframe) {
        vk::Result result;
        std::tie(result, perframe.queueSubmitFence) = _device.createFence({vk::FenceCreateFlagBits::eSignaled});
        assert(result == vk::Result::eSuccess);

        vk::CommandPoolCreateInfo cmdPoolInfo{
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

        perframe.device = _device;
        perframe.queueIndex = _graphicsQueueIndex;
    }

    void Engine::TeardownPerframe(Perframe &perframe) {
        if (perframe.queueSubmitFence) {
            _device.destroyFence(perframe.queueSubmitFence);
            perframe.queueSubmitFence = nullptr;
        }
        if (perframe.primaryCommandBuffer) {
            _device.freeCommandBuffers(perframe.primaryCommandPool, perframe.primaryCommandBuffer);
            perframe.primaryCommandBuffer = nullptr;
        }
        if (perframe.primaryCommandPool) {
            _device.destroyCommandPool(perframe.primaryCommandPool);
            perframe.primaryCommandPool = nullptr;
        }
        if (perframe.swapchainAcquireSemaphore) {
            _device.destroySemaphore(perframe.swapchainAcquireSemaphore);
            perframe.swapchainAcquireSemaphore = nullptr;
        }
        if (perframe.swapchainReleaseSemaphore) {
            _device.destroySemaphore(perframe.swapchainReleaseSemaphore);
            perframe.swapchainReleaseSemaphore = nullptr;
        }

        perframe.device = nullptr;
        perframe.queueIndex = -1;
    }


    void Engine::InitPipeline() {
        vk::Result result;

        // Create a blank pipeline layout.
        std::tie(result, _pipelineLayout) = _device.createPipelineLayout({});
        VK_CHECK(result);

        vk::PipelineVertexInputStateCreateInfo vertexInput{};

        // Specify use triangle lists
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{{}, vk::PrimitiveTopology::eTriangleList};

        vk::PipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.cullMode = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace = vk::FrontFace::eClockwise;
        rasterizer.lineWidth = 1.0f;

        // enable color blending.
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
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

        vk::PipelineColorBlendStateCreateInfo blend{{}, {}, {}, colorBlendAttachment};

        // We have one viewport.
        vk::PipelineViewportStateCreateInfo viewport;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        // Disable depth testing.
        vk::PipelineDepthStencilStateCreateInfo depthStencil;

        // No multisampling.
        vk::PipelineMultisampleStateCreateInfo multisample({}, vk::SampleCountFlagBits::e1);

        // Specify that these states will be dynamic, i.e. not part of pipeline state object.
        std::array<vk::DynamicState, 2> dynamics{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamic({}, dynamics);


        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{
            vk::PipelineShaderStageCreateInfo{
                {}, vk::ShaderStageFlagBits::eVertex, LoadShaderModule("assets/shaders/vert.spv"), "main"
            },
            vk::PipelineShaderStageCreateInfo{
                {}, vk::ShaderStageFlagBits::eFragment, LoadShaderModule("assets/shaders/frag.spv"), "main"
            },
        };

        vk::GraphicsPipelineCreateInfo pipe{{}, shaderStages};
        pipe.pVertexInputState   = &vertexInput;
        pipe.pInputAssemblyState = &inputAssembly;
        pipe.pRasterizationState = &rasterizer;
        pipe.pColorBlendState    = &blend;
        pipe.pMultisampleState   = &multisample;
        pipe.pViewportState      = &viewport;
        pipe.pDepthStencilState  = &depthStencil;
        pipe.pDynamicState       = &dynamic;

        // We need to specify the pipeline layout and the render pass description up front as well.
        pipe.renderPass = _renderPass;
        pipe.layout     = _pipelineLayout;

        std::tie(result, _pipeline) = _device.createGraphicsPipeline(nullptr, pipe);
        VK_CHECK(result);

        // Modules loaded, ok to destroy.
        _device.destroyShaderModule(shaderStages[0].module);
        _device.destroyShaderModule(shaderStages[1].module);
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
        vk::AttachmentReference colorRef{0, vk::ImageLayout::eColorAttachmentOptimal};

        // We will end up with two transitions.
        // The first one happens right before we start subpass #0, where
        // eUndefined is transitioned into eColorAttachmentOptimal.
        // The final layout in the render pass attachment states ePresentSrcKHR, so we
        // will get a final transition from eColorAttachmentOptimal to ePresetSrcKHR.
        vk::SubpassDescription subpass{{}, vk::PipelineBindPoint::eGraphics, {}, colorRef};

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

        vk::RenderPassCreateInfo renderPassCreateInfo{{}, colorAttachment, subpass, dependency};

        vk::Result result;
        std::tie(result, _renderPass) = _device.createRenderPass(renderPassCreateInfo);
        VK_CHECK(result);
    }

    void Engine::InitFramebuffers() {

        for (auto &imageView : _swapchainImageViews) {
            vk::FramebufferCreateInfo fbInfo{
                {},
                _renderPass,
                imageView,
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
        VmaAllocatorCreateInfo vmaInfo{};
        vmaInfo.physicalDevice = _physicalDevice;
        vmaInfo.device = _device;
        vmaInfo.instance = _instance;
        VK_CHECK(vmaCreateAllocator(&vmaInfo, &_allocator));
    }

    void Engine::Update() {
        uint32_t index;

        vk::Result res = AcquireNextImage(&index);

        switch(res) {
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
                Resize(_swapchainDimensions.width, _swapchainDimensions.height);
                VK_CHECK(_queue.waitIdle());
                return;
            case vk::Result::eSuccess: {
                auto fb = _swapchainFramebuffers[index];
                auto cmd = _perframes[index].primaryCommandBuffer;
                vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
                VK_CHECK(cmd.begin(beginInfo));
                vk::ClearValue clearValue;
                clearValue.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.2f, 1.0f}}));
                vk::RenderPassBeginInfo rpBeginInfo{
                    _renderPass, fb, {{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}},
                    clearValue
                };

                cmd.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
                vk::Viewport vp{0.0f, 0.0f, static_cast<float>(_swapchainDimensions.width), static_cast<float>(_swapchainDimensions.height)};
                cmd.setViewport(0, vp);
                vk::Rect2D scissor{{0, 0}, {_swapchainDimensions.width, _swapchainDimensions.height}};
                cmd.setScissor(0, scissor);
                cmd.draw(3, 1, 0, 0);
                cmd.endRenderPass();
                VK_CHECK(cmd.end());

                if (!_perframes[index].swapchainReleaseSemaphore) {
                    vk::Result result;
                    std::tie(result, _perframes[index].swapchainReleaseSemaphore) = _device.createSemaphore({});
                    VK_CHECK(result);
                }

                vk::PipelineStageFlags waitStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

                vk::SubmitInfo info{
                    _perframes[index].swapchainAcquireSemaphore, waitStage, cmd,
                    _perframes[index].swapchainReleaseSemaphore
                };

                VK_CHECK(_queue.submit(info, _perframes[index].queueSubmitFence));
                break;
            } default:
                VK_CHECK(_queue.waitIdle());
                return;
        }

        res = Present(index);

        // Handle Outdated error in present.
        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR)
        {
            Resize(_swapchainDimensions.width, _swapchainDimensions.height);
        }
        else if (res != vk::Result::eSuccess)
        {
            LOGE("Failed to present swapchain image.");
        }
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

        if (result != vk::Result::eSuccess) {
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

    vk::Result Engine::Present(uint32_t index) {
        vk::PresentInfoKHR present{
            _perframes[index].swapchainReleaseSemaphore, _swapchain, index
        };
        // Avoid assertion failure on result because we will manually parse the result here
        return (vk::Result)vkQueuePresentKHR(_queue, reinterpret_cast<VkPresentInfoKHR *>(&present));
    }

    void Engine::Resize(uint32_t width, uint32_t height) {
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
        TeardownFramebuffers();
        InitSwapchain();
        InitFramebuffers();
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
}