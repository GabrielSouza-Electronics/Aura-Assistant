#include <gui/common/TasksWidget.hpp>
#include "TasksAssets.hpp"
#include <touchgfx/widgets/Image.hpp>
#include <touchgfx/widgets/PixelDataWidget.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <touchgfx/Color.hpp>
#include <stdio.h>
#include <string.h>
#if defined(STM32H743xx)
#include "app_tasks.h"
#include "app_reminders.h"
#include "app_calendar.h"
#else
#include <time.h>
static TaskStore demos[2];
#endif
using namespace touchgfx;
namespace {
constexpr int PROGRESS_SIZE=78;
// DMA2D-readable source; populated explicitly before the first draw.
#if defined(STM32H743xx)
__attribute__((section(".dma_buffer"), aligned(32)))
#endif
uint32_t progressPixels[PROGRESS_SIZE*PROGRESS_SIZE];
int progressSteps=-1;
void prepareProgress(unsigned steps)
{
    if (progressSteps==(int)steps) return;
    memset(progressPixels,0,sizeof(progressPixels));
    const float fraction=steps/40.0f;
    for (int dy=-38;dy<=38;++dy) for (int dx=-38;dx<=38;++dx) {
        const float distance=sqrtf((float)(dx*dx+dy*dy));
        const float edge=fabsf(distance-34);
        if (edge>4) continue;
        float angle=atan2f((float)dx,(float)-dy);
        if (angle<0) angle+=6.28318530718f;
        const bool active=steps && angle<=fraction*6.28318530718f;
        const float coverage=fminf(fmaxf(2.0f-edge,0.0f),1.0f);
        const uint8_t alpha=(uint8_t)(active?fmaxf(coverage*255,(4-edge)*18):coverage*255);
        // Logical (dx,dy) maps to framebuffer (dy,-dx).
        progressPixels[(38-dx)*PROGRESS_SIZE+dy+39]=
            ((uint32_t)alpha<<24)|(active?0x0030e7ffU:0x00172c3aU);
    }
    progressSteps=(int)steps;
}
}
TasksWidget::TasksWidget() { setPosition(0,0,480,480); setVisible(false); }
void TasksWidget::read()
{
#if defined(STM32H743xx)
    if (remindersMode) APP_RemindersReadPage(navigation.page,&data);
    else APP_TasksReadPage(navigation.page,&data);
    APP_CalendarRead(&clock);
#else
    TaskStore& demo=demos[remindersMode?1:0];
    time_t seconds=time(NULL)+4*3600; const tm* t=gmtime(&seconds);
    if (t) {
        clock.now={(uint16_t)(t->tm_year+1900),(uint8_t)(t->tm_mon+1),(uint8_t)t->tm_mday,
                   (uint8_t)t->tm_hour,(uint8_t)t->tm_min,(uint8_t)t->tm_sec}; clock.time_valid=true;
    }
    if (!demo.loaded && clock.time_valid) {
        const char* names[] = {"Prepare project report","Team meeting","Review PCB design","Plan training session",
                              "Send project update","Order PCB components","Review power budget","Plan weekend"};
        const char* reminders[] = {"Check project milestone","Call the team","Send PCB for review","Take a walking break",
                                  "Pay internet bill","Collect PCB components","Check battery charge","Call family"};
        TaskRecord records[8]={};
        for (unsigned i=0;i<8;++i) {
            records[i].id=i+1; records[i].deadline=clock.now;
            records[i].deadline.hour=9+i; records[i].deadline.minute=0; records[i].deadline.second=0;
            if (i>4) Cal_Advance(&records[i].deadline,(i-4)*86400);
            strncpy(records[i].title,remindersMode?reminders[i]:names[i],TASKS_TITLE_SIZE-1);
            records[i].category=(TaskCategory)(i%4);
        }
        (void)Tasks_Publish(&demo,records,8,1);
    }
    Tasks_ReadPage(&demo,navigation.page,&data);
#endif
    navigation.page=data.page;
    prepareProgress(Tasks_ProgressSteps(data.completed_count,data.total));
}
void TasksWidget::enter(bool reminders) { remindersMode=reminders; navigation.enter(); read(); changed=false; completed=reopened=false; revealTicks=focusPhase=0; setVisible(true); invalidate(); }
bool TasksWidget::tick(bool present,float x,float y,bool click)
{
    uint32_t oldVersion=data.version; unsigned oldPage=data.page,oldRow=navigation.row;
    focusPhase=(focusPhase+1)%90;
    const unsigned oldCount=data.count;
    const bool animating=revealTicks<28;
    if (animating) ++revealTicks;
    unsigned oldDay=clock.now.day,oldMinute=clock.now.minute; bool valid=clock.time_valid;
    read();
    int oldArrow=navigation.arrow;
    int action=navigation.tick(click?false:present,click?0:x,click?0:y,data.total);
    if (action<0) return true;
    if (action) { read(); changed=true; }
    if (click && !action && navigation.row<data.count) {
        uint32_t id=data.rows[navigation.row].id;
        bool newCompleted=false;
        bool toggled;
#if defined(STM32H743xx)
        toggled=remindersMode?APP_RemindersToggle(id,&newCompleted):APP_TasksToggle(id,&newCompleted);
#else
        TaskStore& demo=demos[remindersMode?1:0];
        toggled=Tasks_Toggle(&demo,id,&newCompleted);
        if (toggled) (void)Tasks_AcknowledgeChange(&demo,id,newCompleted,true);
#endif
        if (toggled) { changed=true; completed=newCompleted; reopened=!newCompleted; }
        read();
    }
    if (oldPage!=data.page || (!oldCount && data.count)) revealTicks=0;
    if (animating || oldVersion!=data.version || oldPage!=data.page || oldRow!=navigation.row ||
        oldArrow!=navigation.arrow || oldDay!=clock.now.day || oldMinute!=clock.now.minute || valid!=clock.time_valid)
        invalidate();
    if (data.count) {
        // Repaint only the current card for its breathing light.
        touchgfx::Rect focusArea(133+navigation.row*63,65,57,350);
        invalidateRect(focusArea);
    }
    return false;
}
void TasksWidget::sprite(const SettingsStyleSprite& s,int x,int y,const Rect& area,uint8_t alpha) const
{
    Rect r(y,480-x-s.height,s.width,s.height); Rect dirty=r&area;
    if (dirty.isEmpty() || !alpha) return;
    dirty.x-=r.x; dirty.y-=r.y; translateRectToAbsolute(r);
    Image image; image.setPosition(r.x,r.y,r.width,r.height);

    image.setBitmap(Bitmap(s.bitmapId));
    image.setAlpha(alpha); image.draw(dirty);
}
void TasksWidget::text(const char* value,int x,int y,int maxWidth,bool title,bool strike,const Rect& area,uint8_t alpha) const
{
    int advance=0,inkWidth=0,inkTop=23,inkBottom=0;
    const uint8_t* p=reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p==' ') { advance+=title?TASK_TITLE_SPACE16:SETTINGS_STYLE_SPACE16; ++p; continue; }
        const SettingsStyleSprite* g;
        if (p[0]==0xe2 && p[1]==0x80 && p[2]==0xa2) { g=&taskBullets[title?1:0]; p+=3; }
        else {
            unsigned code=*p++; if(code<33 || code>126) code='?';
            g=title?&taskTitleGlyphs[code-33]:&settingsStyleGlyphs[code-33];
            if(title) {
                if(taskTitleInk[code-33][0]<inkTop) inkTop=taskTitleInk[code-33][0];
                if(taskTitleInk[code-33][1]>inkBottom) inkBottom=taskTitleInk[code-33][1];
            }
        }
        int pos=(advance+8)>>4;
        if (pos+g->height>maxWidth) {
            // Explicit ellipsis when the remaining title cannot fit.
            const auto& dot=title?taskTitleGlyphs['.'-33]:settingsStyleGlyphs['.'-33];
            for (int i=0;i<2;++i) sprite(dot,x+maxWidth-2*dot.height+i*dot.height,y,area,alpha);
            inkWidth=maxWidth; break;
        }
        sprite(*g,x+pos,y,area,strike?(uint8_t)(170U*alpha/255U):alpha);
        inkWidth=pos+g->height; advance+=g->advance16;
    }
    if (strike && inkWidth) {
        // Strike through the actual union of glyph ink, never the baseline.
        Rect line(y+(inkTop+inkBottom)/2,480-x-inkWidth,1,inkWidth); Rect dirty=line&area;
        if (!dirty.isEmpty()) { translateRectToAbsolute(dirty); HAL::lcd().fillRect(dirty,Color::getColorFromRGB(139,175,199),alpha); }
    }
}
void TasksWidget::drawProgress(const Rect& area) const
{
    // Shift by approximately 30% left and 10% down of the 78 px glow bounds.
    constexpr int cx=354,cy=86;
    Rect bounds(cy-39,480-cx-39,78,78);
    if ((bounds&area).isEmpty()) return;
    // A single clipped ARGB blit replaces hundreds of tiny LCD fill operations.
    Rect dirty=bounds&area;
    dirty.x-=bounds.x; dirty.y-=bounds.y;
    translateRectToAbsolute(bounds);
    PixelDataWidget ring;
    ring.setPosition(bounds.x,bounds.y,bounds.width,bounds.height);
    ring.setBitmapFormat(Bitmap::ARGB8888);
    ring.setPixelData(reinterpret_cast<uint8_t*>(progressPixels));
    ring.draw(dirty);
    char count[12]; snprintf(count,sizeof(count),"%u/%u",data.completed_count,data.total);
    const char* lines[]={count,"completed"};
    for (unsigned line=0;line<2;++line) {
        int advance=0,width=0;
        for (const char* p=lines[line];*p;++p) {
            const auto& glyph=settingsStyleGlyphs[(unsigned char)*p-33];
            width=((advance+8)>>4)+glyph.height; advance+=glyph.advance16;
        }
        text(lines[line],cx-width/2,cy-16+15*line,70,false,false,area);
    }
}
void TasksWidget::draw(const Rect& area) const
{
    char value[40];
    drawProgress(area);
    if (!data.total) {
        text(data.loaded?(remindersMode?"No reminders":"No tasks"):"Waiting for web sync",115,218,270,true,false,area);
    } else {
        for (unsigned i=0;i<data.count;++i) {
            // Same cascade as Settings: 4-tick stagger, 16-tick smoothstep,
            // alternating 44-pixel horizontal slide and fade-in.
            float progress=fminf(fmaxf(((int)revealTicks-4*(int)i)/16.0f,0.0f),1.0f);
            progress=progress*progress*(3.0f-2.0f*progress);
            const uint8_t alpha=(uint8_t)(255*progress);
            const int slide=(int)((1-progress)*44*(i%2?-1:1));
            const auto& task=data.rows[i]; int y=133+i*63;
            sprite(taskSprites[TASK_PANEL],65+slide,y,area,alpha);
            bool selected=i==navigation.row;
            if (selected) {
                const float pulse=0.5f+0.5f*cosf(focusPhase*6.28318530718f/90.0f);
                const uint8_t cardAlpha=(uint8_t)(alpha*(200.0f+55.0f*pulse)/255.0f);
                const uint8_t haloAlpha=(uint8_t)(alpha*(100.0f+155.0f*pulse)/255.0f);
                sprite(taskSprites[TASK_PANEL_FOCUS],65+slide,y,area,cardAlpha);
                sprite(taskSprites[TASK_FOCUS_HALO],68+slide,y+7,area,haloAlpha);
            }
            int id=task.completed?(selected?TASK_FOCUS_DONE:TASK_DONE):(selected?TASK_FOCUS:TASK_CIRCLE);
            sprite(taskSprites[id],68+slide,y+7,area,alpha);
            text(task.title,110+slide,y+7,185,true,task.completed,area,alpha);
            sprite(taskSprites[TASK_CLOCK],110+slide,y+30,area,alpha);
            if (Tasks_DeadlineText(&task.deadline,&clock.now,clock.time_valid,value,sizeof(value)))
                text(value,133+slide,y+31,166,false,false,area,alpha);
            const auto& tag=taskSprites[TASK_PERSONAL_TAG+(unsigned)task.category];
            // Portrait sprites store logical height in width (rotated layout).
            const int tagY=y+(taskSprites[TASK_PANEL].width-tag.width)/2;
            sprite(tag,303+slide,tagY,area,alpha);
        }
    }
    unsigned pages=(data.total+3)/4;
    if (navigation.arrow==-1) sprite(taskSprites[TASK_ARROW_GLOW],98,383,area);
    if (navigation.arrow==1) sprite(taskSprites[TASK_ARROW_GLOW],352,383,area);
    sprite(taskSprites[TASK_LEFT],106,391,area,data.page>0?210:55);
    sprite(taskSprites[TASK_RIGHT],360,391,area,(unsigned)data.page+1<pages?210:55);
    snprintf(value,sizeof(value),"%u / %u",pages?data.page+1:0,pages);
    text(value,224,401,80,false,false,area);
    const bool selectedCompleted=navigation.row<data.count && data.rows[navigation.row].completed;
    const char* hints[2]={selectedCompleted?"TAP TO REOPEN":"TAP TO COMPLETE",
                         "HOLD CLOSE TO GO BACK"};
    for (unsigned line=0; line<2; ++line) {
        int hintAdvance=0, hintWidth=0;
        for (const char* p=hints[line]; *p; ++p) {
            if (*p==' ') { hintAdvance+=SETTINGS_STYLE_SPACE16; continue; }
            const auto& glyph=settingsStyleGlyphs[(unsigned char)*p-33];
            hintWidth=((hintAdvance+8)>>4)+glyph.height;
            hintAdvance+=glyph.advance16;
        }
        text(hints[line],(480-hintWidth)/2,422+line*15,280,false,false,area);
    }
}
