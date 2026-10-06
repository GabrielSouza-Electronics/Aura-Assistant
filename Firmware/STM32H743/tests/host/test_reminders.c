#include "app_reminders.h"
#include "app_tasks.h"
#include "web_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    TaskPage tasks,reminders;
    bool completed; uint32_t id;
    APP_TasksInit(); APP_RemindersInit();
    APP_TasksReadPage(0,&tasks); APP_RemindersReadPage(0,&reminders);
    assert(tasks.total==8 && reminders.total==8);
    assert(strcmp(tasks.rows[0].title,reminders.rows[0].title));
    assert(APP_RemindersToggle(reminders.rows[0].id,&completed) && completed);
    APP_TasksReadPage(0,&tasks); assert(!tasks.rows[0].completed);
    assert(APP_RemindersToggle(reminders.rows[0].id,&completed) && !completed);
    assert(!APP_RemindersNextChange(&id,&completed));
    APP_RemindersReadPage(1,&reminders); assert(reminders.count==4 && reminders.page==1);
    TaskRecord one={1,{2026,10,6,12,0,0},"Real reminder",TASK_PERSONAL,false};
    assert(APP_RemindersPublish(&one,1,0));
    assert(APP_RemindersToggle(1,&completed) && completed);
    assert(APP_RemindersNextChange(&id,&completed) && id==1 && completed);
    assert(APP_RemindersAcknowledgeChange(id,completed,true));
    APP_TasksReadPage(0,&tasks); assert(tasks.total==8 && !tasks.rows[0].completed);
    const char *json="{\"schemaVersion\":1,\"revision\":3,\"timeZone\":\"Asia/Dubai\","
        "\"clock\":{\"year\":2026,\"month\":10,\"day\":6,\"hour\":12,\"minute\":0,\"second\":0},"
        "\"calendar\":{\"year\":2026,\"holidays\":[]},\"tasks\":[],\"reminders\":[{"
        "\"id\":1,\"title\":\"Call family\",\"category\":\"Personal\",\"completed\":false,"
        "\"deadline\":{\"year\":2026,\"month\":10,\"day\":7,\"hour\":9,\"minute\":0,\"second\":0}}]}";
    WebState state;
    assert(Web_ParseState(json,2026,&state));
    assert(state.reminders_present && state.reminder_count==1 && state.count==0);
    assert(!strcmp(state.reminders[0].title,"Call family"));
    char copy[1024]; size_t length=strlen(json);
    for (size_t n=0;n<length;++n) { memcpy(copy,json,n); copy[n]=0; assert(!Web_ParseState(copy,2026,&state)); }
    char *position;
    strcpy(copy,json); position=strstr(copy,"Personal"); memcpy(position,"Invalid!",8);
    assert(!Web_ParseState(copy,2026,&state));
    puts("PASS: independent reminders/demo/online state, reopen and reminder JSON validation");
    return 0;
}
