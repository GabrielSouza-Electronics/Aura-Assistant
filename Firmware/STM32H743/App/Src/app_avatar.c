#include "app_avatar.h"
#include "FreeRTOS.h"
#include "task.h"
static bool preparing;
static bool speaking;
void APP_Avatar_SetPreparing(bool active) { taskENTER_CRITICAL(); preparing=active; taskEXIT_CRITICAL(); }
void APP_Avatar_SetSpeaking(bool active) { taskENTER_CRITICAL(); speaking=active; taskEXIT_CRITICAL(); }
void APP_Avatar_Read(bool *prep, bool *speech)
{
    if (!prep || !speech) return;
    taskENTER_CRITICAL(); *prep=preparing; *speech=speaking; taskEXIT_CRITICAL();
}
