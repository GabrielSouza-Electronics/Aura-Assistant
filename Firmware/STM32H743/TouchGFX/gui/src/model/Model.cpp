#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#if defined(STM32H743xx)
#include "app_hand_tracking.h"
#include "app_ui_audio.h"
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <cwchar>
#include <cstring>
#endif

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
#if defined(STM32H743xx)
    int8_t x = 0;
    int8_t y = 0;
    const bool present = APP_HandTracking_ReadPointer(&x, &y);
    if (modelListener)
    {
        modelListener->handUpdated(present, x / 10.0f, y / 10.0f);
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
