#ifndef AURA_TASKS_DATA_H
#define AURA_TASKS_DATA_H
#include "calendar_data.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TASKS_CAPACITY 40U
#define TASKS_PAGE_SIZE 4U
#define TASKS_TITLE_SIZE 64U
typedef enum { TASK_PERSONAL, TASK_WORK, TASK_PRIORITY, TASK_PROJECT } TaskCategory;
typedef struct {
    uint32_t id;
    CalDate deadline; /* Same local timezone as the published calendar clock. */
    char title[TASKS_TITLE_SIZE];
    TaskCategory category;
    bool completed;
} TaskRecord;
typedef struct {
    TaskRecord records[TASKS_CAPACITY];
    uint8_t order[TASKS_CAPACITY];
    uint64_t pending, desired;
    uint32_t revision, version;
    uint8_t count;
    bool loaded;
} TaskStore;
typedef struct {
    TaskRecord rows[TASKS_PAGE_SIZE];
    uint32_t version;
    uint8_t count, total, page;
    uint8_t completed_count;
    bool loaded;
} TaskPage;
bool Tasks_Publish(TaskStore *store, const TaskRecord *records, size_t count, uint32_t revision);
void Tasks_ReadPage(const TaskStore *store, unsigned page, TaskPage *out);
bool Tasks_Complete(TaskStore *store, uint32_t id);
bool Tasks_Toggle(TaskStore *store, uint32_t id, bool *completed);
bool Tasks_NextChange(const TaskStore *store, uint32_t *id, bool *completed);
bool Tasks_AcknowledgeChange(TaskStore *store, uint32_t id, bool completed, bool accepted);
bool Tasks_NextPending(const TaskStore *store, uint32_t *id);
bool Tasks_Acknowledge(TaskStore *store, uint32_t id, bool accepted);
bool Tasks_DeadlineText(const CalDate *deadline, const CalDate *now, bool clock_valid,
                        char *out, size_t capacity);
/* Forty arc steps, rounded to the nearest 2.5% increment. */
uint8_t Tasks_ProgressSteps(uint8_t completed,uint8_t total);
#ifdef __cplusplus
}
#endif
#endif
