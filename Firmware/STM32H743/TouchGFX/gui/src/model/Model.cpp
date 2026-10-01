#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <math.h>
#if defined(STM32H743xx)
extern "C"
{
#include "app.h"
}
#include "app_hand_tracking.h"
#include "app_ui_audio.h"
#include "app_ui_settings.h"
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <cwchar>
#include <cstring>
#endif

Model::Model() : modelListener(0), lastWifiLevel(0xFFU), lastVolume(0xFFU)
{
    for (uint32_t& v : settingVersion) v = 0U;
}

void Model::tick()
{
#if defined(STM32H743xx)
    int8_t x = 0;
    int8_t y = 0;
    const bool present = APP_HandTracking_ReadPointer(&x, &y);
    const bool click = APP_HandTracking_TakeClick();
    if (modelListener)
    {
        modelListener->handUpdated(present, x / 10.0f, y / 10.0f);
        if (present && click) modelListener->handClicked();

        /* The simulator never pushes a level, so the View keeps its demo value. */
        const uint8_t wifiLevel = APP_WiFi_GetSignalLevel();
        if (wifiLevel != lastWifiLevel)
        {
            lastWifiLevel = wifiLevel;
            modelListener->wifiLevelUpdated(wifiLevel);
        }

        /* Settings values: the simulator keeps the View's demo texts. */
        static_assert(sizeof(settingVersion) / sizeof(settingVersion[0])
                      == APP_UI_SETTING_TEXT_COUNT, "Settings item count");
        char text[APP_UI_SETTING_TEXT_SIZE];
        for (uint8_t i = 0; i < APP_UI_SETTING_TEXT_COUNT; i++)
        {
            if (APP_UISettings_ReadText((APP_UISetting_t)i, &settingVersion[i],
                                        text, sizeof(text)))
            {
                modelListener->settingTextUpdated(i, text);
            }
        }
        const uint8_t volume = APP_UISettings_GetVolume();
        if (volume != lastVolume)
        {
            lastVolume = volume;
            modelListener->volumeUpdated(volume);
        }
    }
#endif
}

void Model::setHeroBreath(uint8_t level)
{
#if defined(STM32H743xx)
    APP_LED_SetHeroBreath(level);
#else
    (void)level;
#endif
}

void Model::setCarouselLED(float angle, uint8_t visibility)
{
#if defined(STM32H743xx)
    const float fullTurn = 6.28318530718f;
    float phase = fmodf(angle, fullTurn);
    if (phase < 0.0f) phase += fullTurn;
    APP_LED_SetCarousel((uint16_t)(phase * (65535.0f / fullTurn)), visibility);
#else
    (void)angle;
    (void)visibility;
#endif
}

void Model::pulseMenuEnterLED()
{
#if defined(STM32H743xx)
    APP_LED_MenuEnterPulse();
#endif
}

void Model::setLEDBreathColor(uint8_t red, uint8_t green, uint8_t blue)
{
#if defined(STM32H743xx)
    APP_LED_SetBreathColor(red, green, blue);
#else
    (void)red;
    (void)green;
    (void)blue;
#endif
}

bool Model::startupReady() const
{
#if defined(STM32H743xx)
    return APP_GetInitStatus() == APP_INIT_OK;
#else
    return true;
#endif
}

void Model::completeStartup()
{
#if defined(STM32H743xx)
    APP_DisplayStartupComplete();
#endif
}

void Model::requestSetting(uint8_t item, int8_t delta)
{
#if defined(STM32H743xx)
    /* Sound e aplicado pelo App; os demais vao para o hook do dono. O
       texto novo volta pelo caminho normal (settingTextUpdated). */
    APP_UISettings_Request((APP_UISetting_t)item, delta);
#else
    if (!modelListener) return;
    switch (item)
    {
    case 0:
        simWifi = (delta > 0);
        modelListener->settingTextUpdated(0, simWifi ? "HomeNet" : "Off");
        break;
    case 1:
        simBluetooth = (delta > 0);
        modelListener->settingTextUpdated(1, simBluetooth ? "On" : "Off");
        break;
    case 2:
    {
        int b = simBrightness + delta * 10;
        simBrightness = (uint8_t)(b < 10 ? 10 : (b > 100 ? 100 : b));
        char text[5];
        int n = 0;
        if (simBrightness >= 100) text[n++] = '1';
        if (simBrightness >= 10) text[n++] = (char)('0' + (simBrightness / 10) % 10);
        text[n++] = (char)('0' + simBrightness % 10);
        text[n++] = '%';
        text[n] = '\0';
        modelListener->settingTextUpdated(2, text);
        break;
    }
    case 3:
    {
        int v = simVolume + delta;
        simVolume = (uint8_t)(v < 0 ? 0 : (v > 10 ? 10 : v));
        modelListener->volumeUpdated(simVolume);
        break;
    }
    default:
        break;
    }
#endif
}

void Model::playMenuSound(MenuSound sound)
{
#if defined(STM32H743xx)
    switch (sound)
    {
        case MenuSound::Tick: APP_UIAudio_Request(APP_UI_AUDIO_TICK); break;
        case MenuSound::Enter: APP_UIAudio_Request(APP_UI_AUDIO_ENTER); break;
        case MenuSound::Exit: APP_UIAudio_Request(APP_UI_AUDIO_EXIT); break;
        default: break;
    }
#elif defined(_WIN32)
    const wchar_t *name = nullptr;
    switch (sound)
    {
        case MenuSound::Tick: name = L"10_nav_tick.wav"; break;
        case MenuSound::Enter: name = L"20_menu_enter.wav"; break;
        case MenuSound::Exit: name = L"21_menu_exit.wav"; break;
        default: return;
    }
    // Resolve from TouchGFX/build/bin, independent of the simulator's cwd.
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    wchar_t *end = wcsrchr(path, L'\\');
    if (!end) return;
    *(end + 1) = L'\0';
    const wchar_t *relative = L"..\\..\\..\\aura_assets\\audio\\ui\\";
    if (wcslen(path) + wcslen(relative) + wcslen(name) >= MAX_PATH) return;
    wcscat(path, relative);
    wcscat(path, name);
    // Dynamic loading avoids modifying the Designer-generated link settings.
    typedef BOOL (WINAPI *PlaySoundFn)(LPCWSTR, HMODULE, DWORD);
    static HMODULE library = LoadLibraryW(L"winmm.dll");
    static PlaySoundFn play = nullptr;
    if (!play && library)
    {
        const FARPROC address = GetProcAddress(library, "PlaySoundW");
        static_assert(sizeof(play) == sizeof(address), "Windows function pointer size");
        std::memcpy(&play, &address, sizeof(play));
    }
    if (play) play(path, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#else
    (void)sound;
#endif
}
