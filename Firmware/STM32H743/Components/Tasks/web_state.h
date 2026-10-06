#ifndef AURA_WEB_STATE_H
#define AURA_WEB_STATE_H
#include "tasks_data.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    TaskRecord tasks[TASKS_CAPACITY];
    TaskRecord reminders[TASKS_CAPACITY];
    CalDate now;
    CalHolidays holidays;
    uint32_t revision;
    uint8_t count;
    uint8_t reminder_count;
    bool reminders_present;
} WebState;
/* Contract v1, bounded ASCII titles and validated calendar dates. */
bool Web_ParseState(const char *json,unsigned requested_year,WebState *out);
bool Web_ParseCompletion(const char *json,uint32_t expected_id,bool *accepted);
#ifdef __cplusplus
}
#endif
#endif
