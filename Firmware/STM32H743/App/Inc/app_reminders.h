#ifndef AURA_APP_REMINDERS_H
#define AURA_APP_REMINDERS_H
#include "tasks_data.h"
#ifdef __cplusplus
extern "C" {
#endif
void APP_RemindersInit(void); /* Before scheduler start; seeds offline demo tasks. */
/* Network owner publishes validated API records and increasing revisions.
   These functions do no network I/O; call in task context only. */
bool APP_RemindersPublish(const TaskRecord *records,size_t count,uint32_t revision);
void APP_RemindersReadPage(unsigned page,TaskPage *out);
bool APP_RemindersComplete(uint32_t id);
bool APP_RemindersToggle(uint32_t id,bool *completed);
bool APP_RemindersNextChange(uint32_t *id,bool *completed);
bool APP_RemindersAcknowledgeChange(uint32_t id,bool completed,bool accepted);
/* Worker peeks, sends an idempotent completion to the API, then acknowledges.
   On transport failure leave pending for retry; reject only on server refusal. */
bool APP_RemindersNextCompletion(uint32_t *id);
bool APP_RemindersAcknowledge(uint32_t id,bool accepted);
#ifdef __cplusplus
}
#endif
#endif
