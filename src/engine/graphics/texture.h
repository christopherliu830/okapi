#pragma once

#include "types.h"

namespace Graphics {
    class Engine;

    namespace Util {
        /**
         * Load a texture. Remember to delete!
         */
        bool LoadImageFromFile(Engine& engine, const char * file, AllocatedImage& outImage);
    }

    struct Texture {
        AllocatedImage image;
        vk::ImageView imageView;
        vk::Sampler sampler;
    };
};