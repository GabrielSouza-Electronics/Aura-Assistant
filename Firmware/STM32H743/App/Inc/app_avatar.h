#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Task-context flags, published by the future microphone/gesture owner. */
void APP_Avatar_SetPreparing(bool active);
void APP_Avatar_SetSpeaking(bool active);
void APP_Avatar_Read(bool *preparing, bool *speaking);
#ifdef __cplusplus
}
#endif
