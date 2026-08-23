#include "logging.h"

#include "app.h"

#include <stdarg.h>
#include <stdio.h>

/* X-CUBE-ST67W61 requires an application-provided logging sink. Bring-up
   status is exposed through Live Watch diagnostics, so keep this backend
   non-blocking until a dedicated UART/ring-buffer logger is introduced. */
void vLoggingPrintf(uint32_t log_level,
                    const uint8_t metadata_print,
                    const uint32_t line_number,
                    const char *const p_file_name,
                    const char *const p_format,
                    ...)
{
    va_list arguments;
    uint32_t slot;

    (void)metadata_print;
    (void)p_file_name;

    slot = app_wifi_diagnostics.log_count % 8U;
    app_wifi_diagnostics.log_history_level[slot] = log_level;
    app_wifi_diagnostics.log_history_line[slot] = line_number;
    va_start(arguments, p_format);
    (void)vsnprintf((char *)(void *)app_wifi_diagnostics.log_history[slot],
                    sizeof(app_wifi_diagnostics.log_history[slot]), p_format,
                    arguments);
    va_end(arguments);

    app_wifi_diagnostics.last_log_level = log_level;
    app_wifi_diagnostics.last_log_line = line_number;
    ++app_wifi_diagnostics.log_count;
    app_wifi_diagnostics.log_history_next =
        app_wifi_diagnostics.log_count % 8U;
    va_start(arguments, p_format);
    (void)vsnprintf((char *)(void *)app_wifi_diagnostics.last_log,
                    sizeof(app_wifi_diagnostics.last_log), p_format,
                    arguments);
    va_end(arguments);
}
