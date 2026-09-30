#ifndef CAL_TEST_FREERTOS_H
#define CAL_TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef struct { uint32_t storage[64]; } StaticTask_t;
#define configTICK_RATE_HZ 1000U
#define pdMS_TO_TICKS(ms) (ms)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
