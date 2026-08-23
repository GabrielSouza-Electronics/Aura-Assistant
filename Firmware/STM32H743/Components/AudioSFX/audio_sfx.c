#include "audio_sfx.h"

#include <stdint.h>

typedef struct
{
    uint16_t start_frequency_hz;
    uint16_t end_frequency_hz;
    uint16_t duration_ms;
    uint16_t amplitude_permille;
} AUDIO_SFX_Tone_t;

typedef struct
{
    const AUDIO_SFX_Tone_t *tones;
    uint8_t tone_count;
} AUDIO_SFX_Definition_t;

static const int16_t audio_sfx_startup_voice_pcm[] = {
#include "audio_sfx_startup_voice.inc"
};

/* Sound recipes are const, therefore they remain in internal Flash. */
static const AUDIO_SFX_Tone_t audio_sfx_wake[] = {
    {440U, 660U, 120U, 180U},
    {660U, 880U, 140U, 200U},
    {880U, 1320U, 160U, 160U}
};
static const AUDIO_SFX_Tone_t audio_sfx_menu[] = {
    {900U, 1250U, 75U, 130U}
};
/* Short, soft navigation tick for rapid carousel/option movement. */
static const AUDIO_SFX_Tone_t audio_sfx_option_change[] = {
    {1650U, 1050U, 32U, 90U}
};
static const AUDIO_SFX_Tone_t audio_sfx_confirm[] = {
    {660U, 660U, 80U, 150U},
    {990U, 990U, 130U, 180U}
};
static const AUDIO_SFX_Tone_t audio_sfx_sleep[] = {
    {880U, 660U, 130U, 160U},
    {660U, 330U, 190U, 140U}
};
static const AUDIO_SFX_Tone_t audio_sfx_error[] = {
    {220U, 180U, 110U, 170U},
    {0U, 0U, 45U, 0U},
    {220U, 160U, 150U, 170U}
};

static const AUDIO_SFX_Definition_t audio_sfx_definitions[AUDIO_SFX_COUNT] = {
    {audio_sfx_wake, (uint8_t)(sizeof(audio_sfx_wake) / sizeof(audio_sfx_wake[0]))},
    {audio_sfx_menu, (uint8_t)(sizeof(audio_sfx_menu) / sizeof(audio_sfx_menu[0]))},
    {audio_sfx_option_change, (uint8_t)(sizeof(audio_sfx_option_change) / sizeof(audio_sfx_option_change[0]))},
    {audio_sfx_confirm, (uint8_t)(sizeof(audio_sfx_confirm) / sizeof(audio_sfx_confirm[0]))},
    {audio_sfx_sleep, (uint8_t)(sizeof(audio_sfx_sleep) / sizeof(audio_sfx_sleep[0]))},
    {audio_sfx_error, (uint8_t)(sizeof(audio_sfx_error) / sizeof(audio_sfx_error[0]))}
};

const int16_t *AUDIO_SFX_GetStartupVoice(size_t *sample_count)
{
    if (sample_count != NULL)
    {
        *sample_count = sizeof(audio_sfx_startup_voice_pcm) /
                        sizeof(audio_sfx_startup_voice_pcm[0]);
    }
    return audio_sfx_startup_voice_pcm;
}

static int16_t AUDIO_SFX_FastSine(uint32_t phase)
{
    const uint16_t phase16 = (uint16_t)(phase >> 16);
    const int32_t x = (phase16 < 32768U)
                          ? (int32_t)phase16
                          : (int32_t)phase16 - 65536;
    const int32_t absolute_x = (x < 0) ? -x : x;
    return (int16_t)((4 * x * (32768 - absolute_x)) / 32768);
}

size_t AUDIO_SFX_RenderStereo(AUDIO_SFX_Id_t effect,
                              int16_t *output,
                              size_t stereo_sample_capacity)
{
    const AUDIO_SFX_Definition_t *definition;
    size_t output_index = 0U;
    uint32_t phase = 0U;

    if ((output == NULL) || (effect >= AUDIO_SFX_COUNT))
    {
        return 0U;
    }

    definition = &audio_sfx_definitions[effect];
    for (uint32_t tone_index = 0U;
         tone_index < definition->tone_count;
         ++tone_index)
    {
        const AUDIO_SFX_Tone_t *tone = &definition->tones[tone_index];
        const uint32_t frame_count =
            ((uint32_t)tone->duration_ms * AUDIO_SFX_SAMPLE_RATE_HZ) / 1000U;
        const uint32_t ramp_frames = AUDIO_SFX_SAMPLE_RATE_HZ / 200U; /* 5 ms */

        for (uint32_t frame = 0U; frame < frame_count; ++frame)
        {
            uint32_t frequency_hz;
            uint32_t envelope = 1000U;
            int32_t sample;

            if ((output_index + 2U) > stereo_sample_capacity)
            {
                return output_index;
            }

            frequency_hz = tone->start_frequency_hz;
            if (frame_count > 1U)
            {
                const int32_t frequency_delta =
                    (int32_t)tone->end_frequency_hz -
                    (int32_t)tone->start_frequency_hz;
                frequency_hz = (uint32_t)((int32_t)tone->start_frequency_hz +
                    ((frequency_delta * (int32_t)frame) / (int32_t)(frame_count - 1U)));
            }

            if (frame < ramp_frames)
            {
                envelope = (frame * 1000U) / ramp_frames;
            }
            if ((frame_count - frame) <= ramp_frames)
            {
                const uint32_t release = ((frame_count - frame) * 1000U) / ramp_frames;
                if (release < envelope)
                {
                    envelope = release;
                }
            }

            phase += (uint32_t)(((uint64_t)frequency_hz << 32) /
                                AUDIO_SFX_SAMPLE_RATE_HZ);
            sample = AUDIO_SFX_FastSine(phase);
            sample = (sample * (int32_t)tone->amplitude_permille *
                      (int32_t)envelope) / 1000000;

            output[output_index++] = (int16_t)sample;
            output[output_index++] = (int16_t)sample;
        }
    }

    return output_index;
}
