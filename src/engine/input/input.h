#pragma once

#include "SDL2/SDL.h"

class Input {
public:

	bool KeyDown;

	void Parse(SDL_Event &e);

	void Reset();
};