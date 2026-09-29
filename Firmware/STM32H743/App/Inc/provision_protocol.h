#ifndef PROVISION_PROTOCOL_H
#define PROVISION_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROV_MESSAGE_MAX 768U
#define PROV_CHUNK_MAX 20U
#define PROV_SSID_MAX 32U
#define PROV_PASSWORD_MAX 63U

typedef enum { PROV_STATUS, PROV_SCAN, PROV_CONNECT, PROV_FORGET } ProvCommandKind;
typedef struct {
    uint32_t id;
    ProvCommandKind kind;
    char ssid[PROV_SSID_MAX + 1U];
    char password[PROV_PASSWORD_MAX + 1U];
} ProvCommand;

typedef struct {
    char data[PROV_MESSAGE_MAX + 1U];
    size_t length;
    bool dropping;
} ProvFrame;

typedef enum { PROV_FRAME_MORE, PROV_FRAME_READY, PROV_FRAME_BAD } ProvFrameResult;

void Prov_Clear(void *data, size_t size);
void Prov_FrameReset(ProvFrame *frame);
/* READY owns a complete line until the caller resets the frame. */
ProvFrameResult Prov_FrameByte(ProvFrame *frame, uint8_t byte);
/* Strict v1 object; rejects duplicate/unknown fields and invalid UTF-8. */
const char *Prov_Parse(const char *json, ProvCommand *command);
bool Prov_ValidUtf8(const char *text);
/* JSON quoted string, including quotes and NUL. Returns false on overflow. */
bool Prov_Quote(const char *text, char *out, size_t capacity);

#endif
