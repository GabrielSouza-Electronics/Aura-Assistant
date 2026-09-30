#include <stdint.h>
typedef void *osThreadId_t;
typedef struct {
    const char *name;
    void *stack_mem, *cb_mem;
    uint32_t stack_size, cb_size;
    int priority;
} osThreadAttr_t;
#define osPriorityBelowNormal 16
osThreadId_t osThreadNew(void (*entry)(void *),void *arg,const osThreadAttr_t *attr);
void osDelay(uint32_t ticks);
