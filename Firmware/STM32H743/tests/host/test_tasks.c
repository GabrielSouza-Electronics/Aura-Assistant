#include "tasks_data.h"
#include "web_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    TaskStore store={0}; TaskRecord rows[32]={0}; TaskPage page;
    for (unsigned i=0;i<32;++i) {
        rows[i].id=i+1; rows[i].deadline=(CalDate){2028,2,29,(uint8_t)(23-i%24),0,0};
        strcpy(rows[i].title,"Test task"); rows[i].category=TASK_WORK;
    }
    assert(Tasks_Publish(&store,rows,32,1));
    assert(!Tasks_Publish(&store,rows,32,1));
    Tasks_ReadPage(&store,0,&page); assert(page.count==4 && page.total==32);
    for (unsigned i=1;i<32;++i)
        assert(store.records[store.order[i-1]].deadline.hour<=store.records[store.order[i]].deadline.hour);
    uint32_t id=page.rows[0].id,pending=0;
    assert(Tasks_Complete(&store,id)); assert(!Tasks_Complete(&store,id));
    assert(Tasks_NextPending(&store,&pending)&&pending==id);
    assert(Tasks_Publish(&store,rows,32,2)); // Pending survives an incoming snapshot.
    Tasks_ReadPage(&store,0,&page); assert(page.rows[0].completed);
    assert(Tasks_Acknowledge(&store,id,false));
    Tasks_ReadPage(&store,0,&page); assert(!page.rows[0].completed);
    assert(Tasks_Complete(&store,id)); assert(Tasks_Acknowledge(&store,id,true));
    Tasks_ReadPage(&store,999,&page); assert(page.page==7 && page.count==4);
    rows[1].id=rows[0].id; assert(!Tasks_Publish(&store,rows,32,3));
    assert(store.revision==2); rows[1].id=2;
    assert(Tasks_Publish(&store,rows,5,3)); Tasks_ReadPage(&store,999,&page); assert(page.page==1 && page.count==1);
    assert(Tasks_Publish(&store,NULL,0,4)); Tasks_ReadPage(&store,0,&page); assert(!page.count && page.loaded);
    char text[40]; CalDate now={2027,12,31,12,0,0},due={2028,1,1,9,30,0};
    TaskStore toggle={0}; TaskRecord one={99,{2026,10,6,9,0,0},"Toggle task",TASK_WORK,false};
    bool completed;
    assert(Tasks_Publish(&toggle,&one,1,1));
    assert(Tasks_Toggle(&toggle,99,&completed) && completed);
    assert(Tasks_NextChange(&toggle,&pending,&completed) && completed);
    assert(Tasks_Toggle(&toggle,99,&completed) && !completed); // Undo before first PUT returns.
    assert(Tasks_AcknowledgeChange(&toggle,99,true,true));
    assert(Tasks_NextChange(&toggle,&pending,&completed) && !completed);
    Tasks_ReadPage(&toggle,0,&page); assert(!page.rows[0].completed);
    one.completed=true;
    assert(Tasks_Publish(&toggle,&one,1,2)); // Old online state cannot undo pending reopen.
    Tasks_ReadPage(&toggle,0,&page); assert(!page.rows[0].completed);
    assert(Tasks_AcknowledgeChange(&toggle,99,false,false)); // Rejected reopen rolls back.
    Tasks_ReadPage(&toggle,0,&page); assert(page.rows[0].completed);
    assert(Tasks_Toggle(&toggle,99,&completed) && !completed);
    assert(Tasks_AcknowledgeChange(&toggle,99,false,true));
    Tasks_ReadPage(&toggle,0,&page); assert(!page.rows[0].completed);
    assert(Tasks_Toggle(&toggle,99,&completed) && completed);
    assert(Tasks_Toggle(&toggle,99,&completed) && !completed);
    one.completed=false;
    assert(Tasks_Publish(&toggle,&one,1,3));
    assert(!Tasks_NextChange(&toggle,&pending,&completed));
    assert(!Tasks_Toggle(&toggle,100,&completed));
    assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strstr(text,"Tomorrow"));
    now=due; due=(CalDate){2027,12,31,9,30,0};
    assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strstr(text,"Yesterday"));
    now=(CalDate){2028,3,1,0,0,0}; due=(CalDate){2028,2,29,23,59,0};
    assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strstr(text,"Yesterday"));
    assert(!Tasks_DeadlineText(&due,&now,true,text,3));
    /* Large snapshots must work with both supported HTTP body encodings. */
    static char response[33000]; char *body; size_t body_length;
    size_t header=(size_t)sprintf(response,"HTTP/1.1 200 OK\r\nContent-Length: 26000\r\n\r\n");
    memset(response+header,'x',26000);
    assert(Cal_HttpBody(response,header+26000,true,false,&body,&body_length)==1);
    assert(body_length==26000);
    header=(size_t)sprintf(response,"HTTP/1.1 200 OK\r\nContent-Length: 32769\r\n\r\n");
    assert(Cal_HttpBody(response,header,true,false,&body,&body_length)==-1);
    header=(size_t)sprintf(response,"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n6590\r\n");
    memset(response+header,'x',26000);
    memcpy(response+header+26000,"\r\n0\r\n\r\n",7);
    assert(Cal_HttpBody(response,header+26007,true,true,&body,&body_length)==1);
    assert(body_length==26000 && body[0]=='x' && body[25999]=='x');
    header=(size_t)sprintf(response,"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFFFFFFFFFF\r\n");
    assert(Cal_HttpBody(response,header,true,false,&body,&body_length)==-1);
    unsigned cases=0;
    TaskStore forty={0}; TaskRecord entries[40]={0};
    for (unsigned i=0;i<40;++i) {
        entries[i].id=i+1; entries[i].deadline=(CalDate){2026,10,6,12,0,0};
        strcpy(entries[i].title,"Progress item"); entries[i].category=TASK_WORK;
    }
    assert(Tasks_Publish(&forty,entries,40,1));
    assert(!Tasks_Publish(&forty,entries,41,2));
    for (unsigned i=0;i<40;++i) {
        assert(Tasks_Toggle(&forty,i+1,&completed) && completed);
        Tasks_ReadPage(&forty,9,&page);
        assert(page.total==40 && page.page==9 && page.completed_count==i+1);
        assert(Tasks_ProgressSteps(page.completed_count,page.total)==i+1);
    }
    assert(Tasks_Publish(&forty,entries,40,2));
    Tasks_ReadPage(&forty,9,&page); assert(page.completed_count==40);
    assert(Tasks_AcknowledgeChange(&forty,40,true,true));
    assert(Tasks_Toggle(&forty,40,&completed) && !completed);
    Tasks_ReadPage(&forty,9,&page); assert(page.completed_count==39);
    assert(Tasks_AcknowledgeChange(&forty,40,false,true));
    assert(Tasks_ProgressSteps(0,0)==0 && Tasks_ProgressSteps(1,3)==13);
    assert(Tasks_ProgressSteps(3,3)==40);
    now=(CalDate){2028,3,1,12,0,0};
    for (unsigned h=0;h<24;++h) for(unsigned minute=0;minute<60;++minute) {
        due=(CalDate){2028,3,1,(uint8_t)h,(uint8_t)minute,0};
        assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strlen(text)==5); ++cases;
        due.day=2;
        assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strstr(text,"Tomorrow")); ++cases;
        due.month=2; due.day=29;
        assert(Tasks_DeadlineText(&due,&now,true,text,sizeof(text)) && strstr(text,"Yesterday")); ++cases;
    }
    for (unsigned m=1;m<=12;++m) for(unsigned d=1;d<=Cal_Days(2028,m);++d)
        for(unsigned h=0;h<24;++h) for(unsigned minute=0;minute<60;++minute) {
            due=(CalDate){2028,(uint8_t)m,(uint8_t)d,(uint8_t)h,(uint8_t)minute,0};
            assert(Tasks_DeadlineText(&due,&now,false,text,sizeof(text))); ++cases;
        }
    const char *json="{\"schemaVersion\":1,\"revision\":2,\"timeZone\":\"Asia/Dubai\","
        "\"clock\":{\"year\":2028,\"month\":2,\"day\":29,\"hour\":9,\"minute\":0,\"second\":0},"
        "\"calendar\":{\"year\":2028,\"holidays\":[\"2028-02-29\"]},\"tasks\":[{\"id\":4294967295,"
        "\"title\":\"Quote \\\"task\\\"\",\"category\":\"Project\",\"completed\":false,"
        "\"deadline\":{\"year\":2028,\"month\":2,\"day\":29,\"hour\":23,\"minute\":59,\"second\":0}}]}";
    WebState web;
    assert(Web_ParseState(json,2028,&web)); assert(web.count==1 && web.tasks[0].id==UINT32_MAX);
    assert(!Web_ParseState(json,2027,&web));
    char copy[1024]; size_t length=strlen(json);
    for (size_t i=0;i<length;++i) { memcpy(copy,json,i); copy[i]=0; assert(!Web_ParseState(copy,2028,&web)); }
    bool accepted=false;
    assert(Web_ParseCompletion("{\"schemaVersion\":1,\"id\":5,\"accepted\":true}",5,&accepted)&&accepted);
    assert(!Web_ParseCompletion("{\"schemaVersion\":1,\"id\":6,\"accepted\":true}",5,&accepted));
    printf("Tasks: sorting, 8 pages, pending/ack/reject, leap-year rollover, %u deadline combinations, JSON truncations passed.\n",cases);
}
