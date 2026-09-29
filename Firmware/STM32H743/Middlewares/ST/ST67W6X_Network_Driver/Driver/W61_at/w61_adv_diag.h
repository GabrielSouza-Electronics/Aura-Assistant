/* Aura: bounded, credential-free observations of AT+BLEADVSTART only. */
#ifndef W61_ADV_DIAG_H
#define W61_ADV_DIAG_H
#include <stdint.h>
#include <string.h>
typedef struct {
    uint32_t attempts;
    uint32_t active;
    uint32_t terminal; /* 0: none, 1: OK, 2: ERROR */
    int32_t raw_return; /* modem_cmd_send result, before W61/W6X translation */
    uint32_t error_code_valid;
    uint32_t error_code;
} W61_AdvDiagnostics;
extern volatile W61_AdvDiagnostics w61_adv_diagnostics;

static inline void W61_AdvObserve(const char *line)
{
    if (!w61_adv_diagnostics.active) { return; }
    if (strcmp(line, "OK") == 0) { w61_adv_diagnostics.terminal = 1U; }
    else if (strcmp(line, "ERROR") == 0) { w61_adv_diagnostics.terminal = 2U; }
    else if (strncmp(line, "ERR CODE:", 9U) == 0)
    {
        const char *p = line + 9;
        while (*p == ' ') { ++p; }
        if (p[0] != '0' || (p[1] != 'x' && p[1] != 'X')) { return; }
        p += 2;
        uint32_t value = 0U, count = 0U;
        while (*p != '\0')
        {
            unsigned digit;
            if (*p >= '0' && *p <= '9') { digit = (unsigned)(*p - '0'); }
            else if (*p >= 'a' && *p <= 'f') { digit = (unsigned)(*p - 'a') + 10U; }
            else if (*p >= 'A' && *p <= 'F') { digit = (unsigned)(*p - 'A') + 10U; }
            else { return; }
            if (++count > 8U) { return; }
            value = (value << 4) | digit;
            ++p;
        }
        if (count != 0U)
        {
            w61_adv_diagnostics.error_code = value;
            w61_adv_diagnostics.error_code_valid = 1U;
        }
    }
}
#endif
