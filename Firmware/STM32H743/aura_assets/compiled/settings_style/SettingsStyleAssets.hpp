#pragma once
#include <stdint.h>
struct SettingsStyleSprite { uint16_t bitmapId; int16_t width, height; uint16_t advance16; };
extern const SettingsStyleSprite settingsStyleRows[4][2];
extern const SettingsStyleSprite settingsStyleGlow;
extern const SettingsStyleSprite settingsStyleFocusFrame;
extern const SettingsStyleSprite settingsStyleGlyphs[94];
constexpr uint16_t SETTINGS_STYLE_SPACE16 = 45;
