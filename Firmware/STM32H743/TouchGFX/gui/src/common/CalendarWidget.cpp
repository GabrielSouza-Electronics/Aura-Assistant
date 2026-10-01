#include <gui/common/CalendarWidget.hpp>
#include "CalendarAssets.hpp"
#include <touchgfx/widgets/PixelDataWidget.hpp>
#include <string.h>
#if defined(STM32H743xx)
#include "app_calendar.h"
#else
#include <time.h>
#endif
using namespace touchgfx;
static_assert(CAL_DAY_NORMAL_31-CAL_DAY_NORMAL_01==30 &&
              CAL_DAY_WEEKEND_31-CAL_DAY_WEEKEND_01==30 &&
              CAL_DAY_MUTED_31-CAL_DAY_MUTED_01==30 &&
              CAL_DAY_HOLIDAY_31-CAL_DAY_HOLIDAY_01==30 &&
              CAL_MONTH_12-CAL_MONTH_01==11 && CAL_YEAR_9-CAL_YEAR_0==9 &&
              CAL_CLOCK_39-CAL_CLOCK_30==9 && CAL_WEEKDAY_6-CAL_WEEKDAY_0==6,
              "Calendar sprite sequences must be complete and contiguous");
CalendarWidget::CalendarWidget() : data(),ticks(0),pulse(255)
{ setPosition(0,0,480,480); setVisible(false); }
void CalendarWidget::read()
{
#if defined(STM32H743xx)
    APP_CalendarRead(&data);
#else
    // UTC+4 simulator clock; never fabricate holiday data.
    time_t now=time(NULL)+4*3600;
    const tm* t=gmtime(&now);
    if (t) {
        data.now.year=t->tm_year+1900; data.now.month=t->tm_mon+1;
        data.now.day=t->tm_mday; data.now.hour=t->tm_hour;
        data.now.minute=t->tm_min; data.now.second=t->tm_sec;
        data.time_valid=true;
    }
#endif
}
void CalendarWidget::enter()
{
    read(); navigation.enter(data); ticks=0; pulse=255; monthChanged=false;
#if defined(STM32H743xx)
    APP_CalendarRequestYear(navigation.year);
#endif
    setVisible(true); invalidate();
}
void CalendarWidget::todayArea()
{
    if (!data.time_valid || navigation.year!=data.now.year || navigation.month!=data.now.month) return;
    unsigned cell=Cal_Weekday(navigation.year,navigation.month,1)+data.now.day-1;
    int x=90+(cell%7)*50, y=243+(cell/7)*32;
    Rect dirty(y-22,480-x-22,44,44);
    invalidateRect(dirty);
}
int CalendarWidget::status() const
{
    if (!data.time_valid) return CAL_STATUS_SYNC;
    if (!data.holidays_valid || data.holidays.year!=navigation.year) return CAL_STATUS_LOADING;
    if (!data.online) return CAL_STATUS_OFFLINE;
    if (data.stale) return CAL_STATUS_STALE;
    if (data.holidays.estimated[navigation.month-1]) return CAL_STATUS_ESTIMATED;
    return CAL_STATUS_READY;
}
bool CalendarWidget::tick(bool present,float x,float y)
{
    unsigned oldYear=navigation.year,oldMonth=navigation.month;
    const int oldDirection=navigation.getDirection();
    const int gesture=navigation.gesture(present,x,y);
    if (gesture==2) return true;
    if (gesture!=0) monthChanged=true;
    if (oldDirection!=navigation.getDirection()) {
        // Logical 44x44 arrow halos, transformed into Portrait coordinates.
        Rect leftArrow(148,385,44,44),rightArrow(148,51,44,44);
        invalidateRect(leftArrow); invalidateRect(rightArrow);
    }
    ++ticks;
    if (ticks%6==0) {
        unsigned phase=ticks%90;
        pulse=(uint8_t)(70+(phase<45?phase:90-phase)*185/45);
        todayArea();
    }
    // Read the clock snapshot each frame: blink tracks actual seconds,
    // independently of LCD refresh rate and frame drops.
    {
        CalSnapshot previous=data;
        read(); navigation.synchronize(data);
        if (previous.time_valid!=data.time_valid || previous.now.day!=data.now.day ||
            previous.now.month!=data.now.month || previous.now.year!=data.now.year ||
            previous.holidays_valid!=data.holidays_valid || previous.online!=data.online ||
            previous.stale!=data.stale || memcmp(&previous.holidays,&data.holidays,sizeof(data.holidays))!=0) invalidate();
        else if (previous.now.minute!=data.now.minute || previous.now.hour!=data.now.hour)
        {
            Rect dirty(62,133,88,214);
            invalidateRect(dirty);
        }
        else if (previous.now.second!=data.now.second)
        {
            Rect dirty(62,229,88,22);
            invalidateRect(dirty);
        }
        if (ticks>=180) ticks=0;
    }
    if (oldYear!=navigation.year || oldMonth!=navigation.month) {
#if defined(STM32H743xx)
        APP_CalendarRequestYear(navigation.year);
#endif
        invalidate();
    }
    return false;
}
void CalendarWidget::sprite(int id,int x,int y,const Rect& area,uint8_t alpha) const
{
    const CalendarSprite& s=calendarSprites[id];
    // Logical (x,y) -> (y,479-x), matching the existing GlyphText renderer.
    Rect r(y-s.height/2,480-x-(s.width+1)/2,s.height,s.width);
    Rect dirty=r&area;
    if (dirty.isEmpty() || !alpha) return;
    dirty.x-=r.x; dirty.y-=r.y;
    translateRectToAbsolute(r);
    PixelDataWidget image;
    image.setPosition(r.x,r.y,r.width,r.height);
    image.setBitmapFormat(Bitmap::ARGB8888);
    // draw() only reads this pointer; the pixels remain const in QSPI.
    image.setPixelData(const_cast<uint8_t*>(s.pixels));
    image.setAlpha(alpha);
    image.draw(dirty);
}
void CalendarWidget::draw(const Rect& area) const
{
    // The shared logo, Wi-Fi and battery widgets occupy the top header.
    char clock[5]={'-', '-', ':', '-', '-'};
    if (data.time_valid) {
        clock[0]='0'+data.now.hour/10; clock[1]='0'+data.now.hour%10;
        clock[3]='0'+data.now.minute/10; clock[4]='0'+data.now.minute%10;
    }
    int left=133;
    for (int i=0;i<5;i++) {
        int id=clock[i]==':'?CAL_CLOCK_3A:clock[i]=='-'?CAL_CLOCK_2D:CAL_CLOCK_30+clock[i]-'0';
        int w=calendarSprites[id].width;
        // Toggle once per second, retaining the colon's layout space.
        if (clock[i]!=':' || !data.time_valid || (data.now.second%2)==0)
            sprite(id,left+w/2,106,area);
        left+=w;
    }
    if (navigation.year) {
        int monthId=CAL_MONTH_01+navigation.month-1;
        int w=calendarSprites[monthId].width;
        left=240-(w+9+68)/2;
        sprite(monthId,left+w/2,170,area);
        unsigned divisor=1000;
        for (unsigned i=0;i<4;i++,divisor/=10)
            sprite(CAL_YEAR_0+(navigation.year/divisor)%10,left+w+9+i*17+8,170,area);
        const int direction=navigation.getDirection();
        if (direction) sprite(CAL_TODAY,direction<0?73:407,170,area);
        sprite(CAL_ARROW_LEFT,73,170,area,direction<0?255:150);
        sprite(CAL_ARROW_RIGHT,407,170,area,direction>0?255:150);
        for (int col=0;col<7;col++) sprite(CAL_WEEKDAY_0+col,90+col*50,208,area);
        int offset=Cal_Weekday(navigation.year,navigation.month,1);
        int days=Cal_Days(navigation.year,navigation.month);
        int previous=Cal_Days(navigation.month==1?navigation.year-1:navigation.year,navigation.month==1?12:navigation.month-1);
        int cells=((offset+days+6)/7)*7;
        for (int cell=0;cell<cells;cell++) {
            int day=cell-offset+1;
            bool current=day>=1 && day<=days;
            int number=day<1?previous+day:day>days?day-days:day;
            int x=90+cell%7*50,y=243+cell/7*32;
            bool holiday=current && data.holidays_valid && data.holidays.year==navigation.year &&
                (data.holidays.days[navigation.month-1]&(1UL<<(number-1)));
            bool today=current && data.time_valid && navigation.year==data.now.year &&
                navigation.month==data.now.month && number==data.now.day;
            if (holiday) sprite(CAL_HOLIDAY,x,y,area);
            if (today) sprite(holiday?CAL_TODAY_HOLIDAY:CAL_TODAY,x,y,area,pulse);
            int base=!current?CAL_DAY_MUTED_01:holiday?CAL_DAY_HOLIDAY_01:cell%7>=5?CAL_DAY_WEEKEND_01:CAL_DAY_NORMAL_01;
            sprite(base+number-1,x,y,area);
        }
    }
    sprite(status(),240,427,area);
    sprite(CAL_HINT,240,447,area);
}
