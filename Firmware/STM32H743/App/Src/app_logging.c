#include "logging.h"

/* X-CUBE-ST67W61 requires an application-provided logging sink. Keep it
   non-blocking until the product logging service is implemented. */
void vLoggingPrintf(uint32_t log_level,
                    const uint8_t metadata_print,
                    const uint32_t line_number,
                    const char *const p_file_name,
                    const char *const p_format,
                    ...)
{
    (void)log_level;
    (void)metadata_print;
    (void)line_number;
    (void)p_file_name;
    (void)p_format;
}
