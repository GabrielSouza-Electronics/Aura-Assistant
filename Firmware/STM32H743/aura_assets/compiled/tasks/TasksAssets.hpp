#pragma once
#include "SettingsStyleAssets.hpp"
enum TaskSpriteId { TASK_PANEL,TASK_PERSONAL_TAG,TASK_WORK_TAG,TASK_PRIORITY_TAG,TASK_PROJECT_TAG,
TASK_CIRCLE,TASK_FOCUS,TASK_DONE,TASK_FOCUS_DONE,TASK_CLOCK,TASK_LEFT,TASK_RIGHT,TASK_ARROW_GLOW,TASK_PANEL_FOCUS,TASK_FOCUS_HALO };
extern const SettingsStyleSprite taskSprites[15];
extern const SettingsStyleSprite taskTitleGlyphs[94];
extern const int8_t taskTitleInk[94][2];
extern const SettingsStyleSprite taskBullets[2];
constexpr uint16_t TASK_TITLE_SPACE16 = 59;
