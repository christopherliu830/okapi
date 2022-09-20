#pragma once

#include "graphics.h"
#include "types.h"

namespace Graphics::Util {

    /**
     * Load a texture. Remember to delete!
     */
    bool LoadImageFromFile(Engine& engine, const char * file, AllocatedImage& outImage);
};
