#include "psx_sdl.h"
extern "C" {
#include "host_keymap.h"
}
#include <cstdio>
int main(){host_keymap_load("pc/config.ini");
    if(!host_keymap_match_event(HOST_KEYMAP_TURBO_TOGGLE,SDLK_SPACE,SDL_SCANCODE_SPACE,0))return 1;
    if(host_keymap_match_event(HOST_KEYMAP_TURBO_TOGGLE,SDLK_x,SDL_SCANCODE_X,0))return 2;
    std::puts("keymap_probe: actual framework config maps Space to latched TurboToggle");return 0;
}
