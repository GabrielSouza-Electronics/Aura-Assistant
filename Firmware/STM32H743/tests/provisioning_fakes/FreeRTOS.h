#ifndef PROV_FAKE_FREERTOS_H
#define PROV_FAKE_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
extern int critical_depth;
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() (--critical_depth)
#define pdMS_TO_TICKS(ms) (ms)
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
#endif
