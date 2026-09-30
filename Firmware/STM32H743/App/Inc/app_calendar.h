#ifndef APP_CALENDAR_H
#define APP_CALENDAR_H
#include "calendar_data.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Call once after W6X_Net_Init, from WiFiTask. Network I/O has its own worker. */
bool APP_CalendarStart(void);
void APP_CalendarSetOnline(bool online);
void APP_CalendarRequestYear(uint16_t year);
void APP_CalendarRead(CalSnapshot *out);
#ifdef __cplusplus
}
#endif
#endif
