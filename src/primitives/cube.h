#pragma once 

#include "graphics/mesh.h"
#include "graphics.h"

namespace Primitives {
    class Cube {

    public:
        Cube(Graphics::Engine& engine);

        Graphics::Renderable renderable;

    };
};