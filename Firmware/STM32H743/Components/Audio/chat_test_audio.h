#ifndef CHAT_TEST_AUDIO_H
#define CHAT_TEST_AUDIO_H
#include <stdint.h>
#define CHAT_TEST_SAMPLE_RATE 48000U
#define CHAT_TEST_SAMPLE_COUNT 236160U
#ifdef __cplusplus
extern "C" {
#endif
extern const int16_t chat_test_pcm[CHAT_TEST_SAMPLE_COUNT];
#ifdef __cplusplus
}
#endif
#endif
