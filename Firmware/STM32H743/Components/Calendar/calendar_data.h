#ifndef AURA_CALENDAR_DATA_H
#define AURA_CALENDAR_DATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CAL_FIRST_YEAR 2020
#define CAL_LAST_YEAR 2099
typedef struct { uint16_t year; uint8_t month, day, hour, minute, second; } CalDate;
typedef struct {
    uint16_t year;
    uint32_t days[12]; /* bit zero = first day */
    uint32_t estimated[12];
} CalHolidays;
typedef struct {
    CalDate now;
    CalHolidays holidays;
    bool time_valid, holidays_valid, online, stale;
} CalSnapshot;
unsigned Cal_Days(unsigned year, unsigned month);
bool Cal_Valid(const CalDate *date);
unsigned Cal_Weekday(unsigned year, unsigned month, unsigned day); /* Mon = 0 */
void Cal_Advance(CalDate *date, uint32_t seconds);
bool Cal_ParseTime(const char *json, CalDate *out);
bool Cal_ParseHolidays(const char *json, unsigned year, CalHolidays *out);
/* Incremental HTTP/1.x framing: 0 = more, -1 = reject, 1 = complete.
 * Decode is separate so fragmented chunks never mutate the input early. */
int Cal_HttpBody(char *raw, size_t length, bool eof, bool decode,
                 char **body, size_t *body_length);
#ifdef __cplusplus
}
#endif
#endif
