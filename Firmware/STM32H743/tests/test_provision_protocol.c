#include "provision_protocol.h"
#include "w61_at_credential.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void bad(const char *s)
{
    ProvCommand command;
    assert(Prov_Parse(s, &command) != NULL);
    Prov_Clear(&command, sizeof(command));
}

int main(void)
{
    ProvCommand command;
    assert(!Prov_Parse("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}", &command));
    assert(command.id == 1 && command.kind == PROV_SCAN);
    const char *request = "{\"password\":\"1234\\\"\\\\,890\",\"ssid\":\"Caf\\u00e9 \\ud83d\\ude00\",\"cmd\":\"connect\",\"id\":4294967295,\"v\":1}";
    assert(!Prov_Parse(request, &command));
    assert(!strcmp(command.ssid, "Caf\xc3\xa9 \xf0\x9f\x98\x80"));
    assert(!strcmp(command.password, "1234\"\\,890"));
    assert(command.id == UINT32_MAX);
    char quoted[200], escaped[127];
    assert(Prov_Quote(command.ssid, quoted, sizeof(quoted)));
    assert(!Prov_Quote(command.ssid, quoted, 3));
    assert(W61_CredentialEscape((uint8_t *)command.password, 63, escaped, sizeof(escaped)));
    assert(!strcmp(escaped, "1234\\\"\\\\\\,890"));
    assert(!W61_CredentialEscape((uint8_t *)"a\r\nAT+RESET", 63, escaped, sizeof(escaped)));
    memset(command.password, '\\', 63); command.password[63] = 0;
    assert(W61_CredentialEscape((uint8_t *)command.password, 63, escaped, sizeof(escaped)));
    assert(strlen(escaped) == 126);
    assert(!W61_CredentialEscape((uint8_t *)command.password, 63, escaped, 126));
    bad(""); bad("{}"); bad("[]"); bad("null");
    bad("{\"v\":1,\"v\":1,\"id\":1,\"cmd\":\"scan\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"scan\",}");
    bad("{\"v\":1,\"id\":01,\"cmd\":\"scan\"}");
    bad("{\"v\":1,\"id\":0,\"cmd\":\"scan\"}");
    bad("{\"v\":1,\"id\":4294967296,\"cmd\":\"scan\"}");
    bad("{\"v\":1,\"id\":1.0,\"cmd\":\"scan\"}");
    bad("{\"v\":2,\"id\":1,\"cmd\":\"scan\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}garbage");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"scan\",\"password\":\"12345678\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"x\",\"password\":\"short\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"\\u0000\",\"password\":\"\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"\\ud800\",\"password\":\"\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"a\\nAT\",\"password\":\"\"}");
    bad("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"\xc0\xaf\",\"password\":\"\"}");
    assert(!Prov_ValidUtf8("\xed\xa0\x80"));
    assert(!Prov_ValidUtf8("\xf4\x90\x80\x80"));
    assert(!Prov_ValidUtf8("\xe2\x82"));
    char ssid[34], password[65], message[800];
    memset(ssid, 's', 32); ssid[32] = 0;
    memset(password, 'p', 63); password[63] = 0;
    snprintf(message, sizeof(message), "{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"%s\",\"password\":\"%s\"}", ssid, password);
    assert(!Prov_Parse(message, &command));
    ssid[32] = 's'; ssid[33] = 0;
    snprintf(message, sizeof(message), "{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"%s\",\"password\":\"%s\"}", ssid, password);
    bad(message);
    ProvFrame frame;
    Prov_FrameReset(&frame);
    for (size_t i = 0; i < strlen(request); ++i) { assert(Prov_FrameByte(&frame, request[i]) == PROV_FRAME_MORE); }
    assert(Prov_FrameByte(&frame, '\n') == PROV_FRAME_READY);
    assert(!Prov_Parse(frame.data, &command));
    Prov_FrameReset(&frame);
    for (size_t i = 0; i <= PROV_MESSAGE_MAX; ++i) { (void)Prov_FrameByte(&frame, 'a'); }
    assert(Prov_FrameByte(&frame, '\n') == PROV_FRAME_BAD);
    assert(frame.length == 0 && !frame.dropping);
    (void)Prov_FrameByte(&frame, 0);
    assert(Prov_FrameByte(&frame, '\n') == PROV_FRAME_BAD);
    /* Every truncated prefix must be rejected without reading past its NUL. */
    for (size_t i = 0; i < strlen(request); ++i)
    { memcpy(message, request, i); message[i] = 0; bad(message); }
    Prov_Clear(&command, sizeof(command));
    for (size_t i = 0; i < sizeof(command); ++i) { assert(((uint8_t *)&command)[i] == 0); }
    puts("protocol: JSON, Unicode, boundaries, framing and AT escaping passed");
    return 0;
}
