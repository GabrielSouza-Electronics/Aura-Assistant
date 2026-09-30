#pragma once
#include <stdint.h>
struct StartupSprite { const uint32_t* pixels; int16_t width,height; };
extern const StartupSprite startupSprites[6];
