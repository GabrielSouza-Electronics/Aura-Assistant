#ifndef AURA_APP_TASKS_H
#define AURA_APP_TASKS_H
#include "tasks_data.h"
#ifdef __cplusplus
extern "C" {
#endif
void APP_TasksInit(void); /* Before scheduler start; seeds offline demo tasks. */
/* Network owner publishes validated API records and increasing revisions.
   These functions do no network I/O; call in task context only. */
bool APP_TasksPublish(const TaskRecord *records,size_t count,uint32_t revision);
void APP_TasksReadPage(unsigned page,TaskPage *out);
bool APP_TasksComplete(uint32_t id);
bool APP_TasksToggle(uint32_t id,bool *completed);
bool APP_TasksNextChange(uint32_t *id,bool *completed);
bool APP_TasksAcknowledgeChange(uint32_t id,bool completed,bool accepted);
/* Worker peeks, sends an idempotent completion to the API, then acknowledges.
   On transport failure leave pending for retry; reject only on server refusal. */
bool APP_TasksNextCompletion(uint32_t *id);
bool APP_TasksAcknowledge(uint32_t id,bool accepted);
#ifdef __cplusplus
}
#endif
#endif
