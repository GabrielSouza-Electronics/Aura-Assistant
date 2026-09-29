/* Aura local integration: bounded escaping of raw credentials for quoted AT
 * arguments. Keep this patch when updating the ST middleware. */
#ifndef W61_AT_CREDENTIAL_H
#define W61_AT_CREDENTIAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline void W61_CredentialClear(void *data, size_t length)
{
  volatile unsigned char *p = data;
  while (length-- != 0U) { *p++ = 0U; }
}

static inline bool W61_CredentialEscape(const uint8_t *src, size_t maximum,
                                       char *dst, size_t capacity)
{
  size_t n = 0U;
  for (size_t i = 0U; i <= maximum; ++i)
  {
    unsigned char c = src[i];
    if (c == 0U)
    {
      if (n >= capacity) { return false; }
      dst[n] = '\0'; return true;
    }
    if (i == maximum || c < 0x20U || c == 0x7fU) { return false; }
    bool escape = c == ',' || c == '"' || c == '\\';
    if (n + (escape ? 2U : 1U) >= capacity) { return false; }
    if (escape) { dst[n++] = '\\'; }
    dst[n++] = (char)c;
  }
  return false;
}
#endif
