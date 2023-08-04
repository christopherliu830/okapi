#include "input.h"
#include "SDL2/SDL.h"
#include "logging.h"


void Input::Parse(SDL_Event &e) {
	switch (e.type) {
		case SDL_KEYDOWN:
			LOGI("I am key down.");
			this->KeyDown = true;
			break;
	}
}

void Input::Reset() {
	//this->KeyDown = false;
}