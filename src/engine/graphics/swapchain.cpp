#include "swapchain.h"

namespace Graphics {


    Swapchain::Swapchain(
        vk::PhysicalDevice gpu,
        vk::Device device,
        vk::SurfaceKHR surface,
        uint32_t index,
        vk::SwapchainKHR oldSwapchain = 0
    ) {
        vk::Result result;

        vk::SurfaceCapabilitiesKHR surfaceProperties;
        std::tie(result, surfaceProperties) = gpu.getSurfaceCapabilitiesKHR(surface);
        VK_CHECK(result);

        vk::SurfaceFormatKHR format = GetFormat(gpu, surface);
        this->format = format.format;

        vk::Extent2D swapchainSize = GetExtent(surfaceProperties);
        width = swapchainSize.width;
        height = swapchainSize.height;

        std::vector<vk::PresentModeKHR> presentModes;
        std::tie(result, presentModes) = gpu.getSurfacePresentModesKHR(surface);
        VK_CHECK(result);

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        for (const auto& available : presentModes) {
            if (available == vk::PresentModeKHR::eMailbox) {
                presentMode = available;
                break;
            }
        }

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

        vk::SwapchainKHR oldSwapchain = this->swapchain;
        vk::SwapchainCreateInfoKHR swapchainCreateInfo {
            {}, // Flags
            surface,
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

        std::tie(result, swapchain) = device.createSwapchainKHR(swapchainCreateInfo);
        VK_CHECK(result);

        std::vector<vk::Image> swapchainImages;
        std::tie(result, swapchainImages) = device.getSwapchainImagesKHR(swapchain);
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

        std::tie(result, _depthImageView) = _device.createImageView(depthViewInfo);
        VK_CHECK(result);

        for(size_t i = 0; i < imageCount; i++) {
            viewCreateInfo.image = swapchainImages[i];
            auto [result, imageView] = _device.createImageView(viewCreateInfo);
            VK_CHECK(result);
            _swapchainImageViews.push_back(imageView);
        }

    }

    vk::PresentModeKHR Swapchain::GetPresentMode(vk::PhysicalDevice gpu, vk::SurfaceKHR surface) {
        std::vector<vk::PresentModeKHR> presentModes;
        std::tie(result, presentModes) = gpu.getSurfacePresentModesKHR(surface);
        VK_CHECK(result);

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Swapchain::GetExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
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

    vk::SurfaceFormatKHR GetFormat(vk::PhysicalDevice gpu, vk::SurfaceKHR surface) {
        std::vector<vk::SurfaceFormatKHR> formats;
        std::tie(result, formats) = gpu.getSurfaceFormatsKHR(surface);
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

        return format;
    }
};