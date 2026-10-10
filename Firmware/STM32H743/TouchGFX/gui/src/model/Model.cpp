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
#include "app_avatar.h"
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <cwchar>
#include <cstring>
#include <cstdlib>
#include "../../../../Components/Audio/chat_test_audio.h"
#include "../../../../Components/Audio/chat_test_timeline.h"
#endif

#if defined(_WIN32) && !defined(STM32H743xx)
namespace {
bool chatAudioOpen=false;
MCIERROR chatMci(const wchar_t* command, wchar_t* result=nullptr, UINT capacity=0)
{
    typedef MCIERROR (WINAPI *SendFn)(LPCWSTR, LPWSTR, UINT, HWND);
    static HMODULE library=LoadLibraryW(L"winmm.dll");
    static SendFn send=nullptr;
    if (!send && library) {
        const FARPROC address=GetProcAddress(library,"mciSendStringW");
        static_assert(sizeof(send)==sizeof(address), "Windows function pointer size");
        std::memcpy(&send,&address,sizeof(send));
    }
    return send ? send(command,result,capacity,nullptr) : 1U;
}
}
#endif

void Model::setChatAudioActive(bool active)
{
#if defined(STM32H743xx)
    APP_UIAudio_SetChatActive(active);
#elif defined(_WIN32)
    if (chatAudioOpen) { chatMci(L"close aura_chat_test"); chatAudioOpen=false; }
    if (!active || simVolume==0U) return;
    wchar_t path[MAX_PATH];
    const DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if (length==0U || length>=MAX_PATH) return;
    wchar_t* end=wcsrchr(path,L'\\');
    if (!end) return;
    *(end+1)=L'\0';
    const wchar_t* relative=L"..\\..\\..\\aura_assets\\audio\\chat_test.wav";
    if (wcslen(path)+wcslen(relative)>=MAX_PATH) return;
    wcscat(path,relative);
    wchar_t command[MAX_PATH+80];
    swprintf(command,sizeof(command)/sizeof(command[0]),
             L"open \"%ls\" type waveaudio alias aura_chat_test",path);
    if (chatMci(command)!=0U) return;
    chatAudioOpen=true;
    if (chatMci(L"set aura_chat_test time format samples")!=0U ||
        chatMci(L"play aura_chat_test from 0")!=0U) {
        chatMci(L"close aura_chat_test"); chatAudioOpen=false;
    }
#else
    (void)active;
#endif
}

bool Model::readChatSpeechFrame(uint8_t& request)
{
#if defined(STM32H743xx)
    return APP_UIAudio_ReadChatFrame(&request);
#elif defined(_WIN32)
    if (!chatAudioOpen) return false;
    wchar_t result[32];
    if (chatMci(L"status aura_chat_test mode",result,32)!=0U || wcscmp(result,L"playing")!=0) {
        chatMci(L"close aura_chat_test"); chatAudioOpen=false;
        return false;
    }
    if (chatMci(L"status aura_chat_test position",result,32)!=0U) return false;
    const uint32_t sample=static_cast<uint32_t>(wcstoul(result,nullptr,10));
    request=SpeechAnimation_Frame(chat_test_cues,CHAT_TEST_CUE_COUNT,sample,CHAT_TEST_SAMPLE_RATE);
    return true;
#else
    (void)request;
    return false;
#endif
}

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
    const bool pointerClick = APP_HandTracking_TakeClick();
    const bool near = APP_HandTracking_ReadNear();
    const bool back = APP_HandTracking_TakeBack();
    if (modelListener)
    {
        bool preparing=false, speaking=false;
        APP_Avatar_Read(&preparing, &speaking);
        modelListener->avatarFlagsUpdated(preparing, speaking);
        modelListener->handUpdated(present, x / 10.0f, y / 10.0f);
        modelListener->handNearUpdated(near);
        if (back) modelListener->handBackRequested();
        else if (pointerClick) modelListener->handClicked();

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
        case MenuSound::TaskComplete: APP_UIAudio_Request(APP_UI_AUDIO_TASK_COMPLETE); break;
        case MenuSound::TaskReopen: APP_UIAudio_Request(APP_UI_AUDIO_TASK_REOPEN); break;
        default: break;
    }
#elif defined(_WIN32)
    const wchar_t *name = nullptr;
    switch (sound)
    {
        case MenuSound::Tick: name = L"10_nav_tick.wav"; break;
        case MenuSound::Enter: name = L"20_menu_enter.wav"; break;
        case MenuSound::Exit: name = L"21_menu_exit.wav"; break;
        case MenuSound::TaskComplete: name = L"40_success.wav"; break;
        case MenuSound::TaskReopen: name = L"21_menu_exit.wav"; break;
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
