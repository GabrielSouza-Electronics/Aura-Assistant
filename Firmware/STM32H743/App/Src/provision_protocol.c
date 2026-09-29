#include "provision_protocol.h"

#include <string.h>

void Prov_Clear(void *data, size_t size)
{
    volatile uint8_t *p = data;
    while (size-- != 0U) { *p++ = 0U; }
}

void Prov_FrameReset(ProvFrame *frame) { Prov_Clear(frame, sizeof(*frame)); }

ProvFrameResult Prov_FrameByte(ProvFrame *frame, uint8_t byte)
{
    if (byte == '\n')
    {
        if (frame->dropping || frame->length == 0U)
        {
            Prov_FrameReset(frame);
            return PROV_FRAME_BAD;
        }
        frame->data[frame->length] = '\0';
        return PROV_FRAME_READY;
    }
    if (byte == 0U || frame->length == PROV_MESSAGE_MAX)
    {
        Prov_Clear(frame->data, sizeof(frame->data));
        frame->length = 0U;
        frame->dropping = true;
    }
    if (!frame->dropping) { frame->data[frame->length++] = (char)byte; }
    return PROV_FRAME_MORE;
}

bool Prov_ValidUtf8(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    while (*p != 0U)
    {
        uint32_t cp = *p++;
        unsigned count;
        uint32_t minimum;
        if (cp < 0x80U) { continue; }
        if (cp >= 0xc2U && cp <= 0xdfU) { count = 1U; minimum = 0x80U; cp &= 31U; }
        else if (cp >= 0xe0U && cp <= 0xefU) { count = 2U; minimum = 0x800U; cp &= 15U; }
        else if (cp >= 0xf0U && cp <= 0xf4U) { count = 3U; minimum = 0x10000U; cp &= 7U; }
        else { return false; }
        while (count-- != 0U)
        {
            if ((*p & 0xc0U) != 0x80U) { return false; }
            cp = (cp << 6U) | (*p++ & 63U);
        }
        if (cp < minimum || cp > 0x10ffffU || (cp >= 0xd800U && cp <= 0xdfffU)) { return false; }
    }
    return true;
}

static void Space(const char **p)
{
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') { ++*p; }
}

static bool Hex4(const char **p, uint32_t *value)
{
    *value = 0U;
    for (unsigned i = 0; i < 4U; ++i)
    {
        unsigned char c = (unsigned char)**p;
        unsigned digit;
        if (c >= '0' && c <= '9') { digit = c - '0'; }
        else if (c >= 'a' && c <= 'f') { digit = c - 'a' + 10U; }
        else if (c >= 'A' && c <= 'F') { digit = c - 'A' + 10U; }
        else { return false; }
        ++*p;
        *value = (*value << 4U) | digit;
    }
    return true;
}

static bool String(const char **p, char *out, size_t cap)
{
    size_t n = 0U;
    if (**p != '"') { return false; }
    ++*p;
    while (**p != '"')
    {
        uint8_t bytes[4];
        unsigned length = 1U;
        unsigned char c = (unsigned char)**p;
        if (c < 0x20U) { return false; }
        ++*p;
        bytes[0] = c;
        if (c == '\\')
        {
            c = (unsigned char)**p;
            if (c == 0U) { return false; }
            ++*p;
            switch (c)
            {
                case '"': case '\\': case '/': bytes[0] = c; break;
                case 'b': bytes[0] = '\b'; break;
                case 'f': bytes[0] = '\f'; break;
                case 'n': bytes[0] = '\n'; break;
                case 'r': bytes[0] = '\r'; break;
                case 't': bytes[0] = '\t'; break;
                case 'u':
                {
                    uint32_t cp, low;
                    if (!Hex4(p, &cp) || cp == 0U) { return false; }
                    if (cp >= 0xd800U && cp <= 0xdbffU)
                    {
                        if ((*p)[0] != '\\' || (*p)[1] != 'u') { return false; }
                        *p += 2;
                        if (!Hex4(p, &low) || low < 0xdc00U || low > 0xdfffU) { return false; }
                        cp = 0x10000U + ((cp - 0xd800U) << 10U) + low - 0xdc00U;
                    }
                    else if (cp >= 0xdc00U && cp <= 0xdfffU) { return false; }
                    if (cp < 0x80U) { bytes[0] = (uint8_t)cp; }
                    else
                    {
                        length = cp < 0x800U ? 2U : (cp < 0x10000U ? 3U : 4U);
                        uint32_t rest = cp;
                        for (unsigned i = length - 1U; i > 0U; --i)
                        { bytes[i] = 0x80U | (rest & 63U); rest >>= 6U; }
                        bytes[0] = (length == 2U ? 0xc0U : (length == 3U ? 0xe0U : 0xf0U)) | rest;
                    }
                    break;
                }
                default: return false;
            }
        }
        if (n + length >= cap) { return false; }
        memcpy(out + n, bytes, length);
        n += length;
    }
    ++*p;
    out[n] = '\0';
    return Prov_ValidUtf8(out);
}

static bool Number(const char **p, uint32_t *out)
{
    *out = 0U;
    if (**p < '0' || **p > '9') { return false; }
    if (**p == '0' && (*p)[1] >= '0' && (*p)[1] <= '9') { return false; }
    while (**p >= '0' && **p <= '9')
    {
        uint32_t digit = (uint32_t)(**p - '0');
        if (*out > (UINT32_MAX - digit) / 10U) { return false; }
        *out = *out * 10U + digit;
        ++*p;
    }
    return true;
}

static bool CredentialText(const char *s)
{
    for (; *s != '\0'; ++s)
    { if ((unsigned char)*s < 0x20U || (unsigned char)*s == 0x7fU) { return false; } }
    return true;
}

const char *Prov_Parse(const char *json, ProvCommand *command)
{
    const char *p = json;
    char key[16], cmd[16] = {0};
    uint32_t version = 0U, fields = 0U;
    memset(command, 0, sizeof(*command));
    Space(&p);
    if (*p++ != '{') { return "invalid_json"; }
    for (;;)
    {
        uint32_t bit;
        Space(&p);
        if (!String(&p, key, sizeof(key))) { return "invalid_json"; }
        Space(&p);
        if (*p++ != ':') { return "invalid_json"; }
        Space(&p);
        if (strcmp(key, "v") == 0) { bit = 1U; if (!Number(&p, &version)) { return "invalid_json"; } }
        else if (strcmp(key, "id") == 0) { bit = 2U; if (!Number(&p, &command->id)) { return "invalid_json"; } }
        else if (strcmp(key, "cmd") == 0) { bit = 4U; if (!String(&p, cmd, sizeof(cmd))) { return "invalid_json"; } }
        else if (strcmp(key, "ssid") == 0) { bit = 8U; if (!String(&p, command->ssid, sizeof(command->ssid))) { return "invalid_ssid"; } }
        else if (strcmp(key, "password") == 0) { bit = 16U; if (!String(&p, command->password, sizeof(command->password))) { return "invalid_password"; } }
        else { return "unknown_field"; }
        if ((fields & bit) != 0U) { return "duplicate_field"; }
        fields |= bit;
        Space(&p);
        if (*p == '}') { ++p; break; }
        if (*p++ != ',') { return "invalid_json"; }
    }
    Space(&p);
    if (*p != '\0' || (fields & 7U) != 7U || command->id == 0U) { return "invalid_request"; }
    if (version != 1U) { return "unsupported_version"; }
    if (strcmp(cmd, "get_status") == 0) { command->kind = PROV_STATUS; }
    else if (strcmp(cmd, "scan") == 0) { command->kind = PROV_SCAN; }
    else if (strcmp(cmd, "forget") == 0) { command->kind = PROV_FORGET; }
    else if (strcmp(cmd, "connect") == 0) { command->kind = PROV_CONNECT; }
    else { return "unknown_command"; }
    if (command->kind == PROV_CONNECT)
    {
        if (fields != 31U || command->ssid[0] == '\0' || !CredentialText(command->ssid)) { return "invalid_ssid"; }
        size_t length = strlen(command->password);
        if ((length != 0U && length < 8U) || !CredentialText(command->password)) { return "invalid_password"; }
    }
    else if (fields != 7U) { return "unexpected_credentials"; }
    return NULL;
}

bool Prov_Quote(const char *text, char *out, size_t capacity)
{
    static const char hex[] = "0123456789abcdef";
    size_t n = 0U;
    if (capacity < 3U || !Prov_ValidUtf8(text)) { return false; }
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p)
    {
        size_t count = *p < 0x20U ? 6U : ((*p == '"' || *p == '\\') ? 2U : 1U);
        if (n + count + 2U > capacity) { return false; }
        if (count == 6U)
        { out[n++] = '\\'; out[n++] = 'u'; out[n++] = '0'; out[n++] = '0'; out[n++] = hex[*p >> 4U]; out[n++] = hex[*p & 15U]; }
        else { if (count == 2U) { out[n++] = '\\'; } out[n++] = (char)*p; }
    }
    out[n++] = '"'; out[n] = '\0';
    return true;
}
