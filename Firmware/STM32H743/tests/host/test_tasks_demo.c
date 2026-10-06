#include "app_tasks.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    TaskPage page;
    uint32_t id;
    APP_TasksInit();
    APP_TasksReadPage(0,&page);
    assert(page.loaded && page.total==8 && page.count==4);
    assert(APP_TasksComplete(page.rows[0].id));
    assert(!APP_TasksComplete(page.rows[0].id));
    assert(!APP_TasksNextCompletion(&id));
    assert(!APP_TasksPublish(NULL,1,0));
    APP_TasksReadPage(0,&page);
    assert(page.loaded && page.total==8 && page.rows[0].completed);
    bool completed;
    assert(APP_TasksToggle(page.rows[0].id,&completed) && !completed);
    APP_TasksReadPage(0,&page); assert(!page.rows[0].completed);
    assert(!APP_TasksNextChange(&id,&completed));
    assert(APP_TasksToggle(page.rows[0].id,&completed) && completed);
    APP_TasksReadPage(1,&page);
    assert(page.count==4 && page.page==1);
    TaskRecord real={1,{2026,10,11,12,0,0},"Real task",TASK_WORK,false};
    assert(APP_TasksPublish(&real,1,0));
    APP_TasksReadPage(0,&page);
    assert(page.total==1 && !page.rows[0].completed);
    assert(APP_TasksComplete(1));
    assert(APP_TasksNextCompletion(&id) && id==1);
    assert(APP_TasksAcknowledge(id,true));
    assert(!APP_TasksNextCompletion(&id));
    assert(APP_TasksPublish(NULL,0,1));
    APP_TasksReadPage(0,&page);
    assert(page.loaded && page.total==0);
    APP_TasksInit();
    APP_TasksReadPage(0,&page);
    assert(page.total==8 && !page.rows[0].completed);
    puts("PASS: offline demo, local completion, valid/invalid web replacement and reboot");
    return 0;
}
