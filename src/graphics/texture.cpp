#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "texture.h"
#include "logging.h"

namespace Graphics::Util {

    bool LoadImageFromFile(Engine &engine, const char * file, AllocatedImage &outImage) {
        int width, height, channels;

        stbi_uc* pixels = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
        
        if (!pixels) {
            LOGW("Failed to load texture {}", file);
            return false;
        }

        vk::Format imageFormat = vk::Format::eR8G8B8A8Srgb;
        vk::Extent3D imageExtent {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        AllocatedImage image = engine.CreateImage(imageFormat, imageExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        engine.UploadImage(image, pixels);

        stbi_image_free(pixels);
        return true;
    }
}