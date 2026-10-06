#pragma once
#include <touchgfx/widgets/Widget.hpp>
#include <gui/common/TasksLogic.hpp>
#include "tasks_data.h"
struct SettingsStyleSprite;
class TasksWidget : public touchgfx::Widget {
public:
    TasksWidget();
    void enter(bool reminders=false);
    bool tick(bool present,float x,float y,bool click);
    bool takeChanged() { bool value=changed; changed=false; return value; }
    bool takeCompleted() { bool value=completed; completed=false; return value; }
    bool takeReopened() { bool value=reopened; reopened=false; return value; }
    float headerProgress() const { return navigation.headerProgress(); }
    void draw(const touchgfx::Rect& area) const override;
    touchgfx::Rect getSolidRect() const override { return touchgfx::Rect(); }
private:
    TaskPage data{};
    CalSnapshot clock{};
    TasksLogic navigation;
    bool changed=false;
    bool completed=false;
    bool reopened=false;
    unsigned revealTicks=0;
    unsigned focusPhase=0;
    bool remindersMode=false;
    void read();
    void drawProgress(const touchgfx::Rect& area) const;
    void sprite(const SettingsStyleSprite& s,int x,int y,const touchgfx::Rect& area,uint8_t alpha=255) const;
    void text(const char* value,int x,int y,int maxWidth,bool title,bool strike,const touchgfx::Rect& area,uint8_t alpha=255) const;
};
