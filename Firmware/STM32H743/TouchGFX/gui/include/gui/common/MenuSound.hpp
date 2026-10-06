#ifndef MENU_SOUND_HPP
#define MENU_SOUND_HPP

enum class MenuSound { None, Tick, Enter, Exit, TaskComplete, TaskReopen };

inline MenuSound menuSoundForTransition(int oldScreen, int oldItem,
                                       int newScreen, int newItem,
                                       bool wasPresent, bool present)
{
    if (oldScreen < 0 && newScreen >= 0) return MenuSound::Enter;
    if (oldScreen >= 0 && newScreen < 0) return MenuSound::Exit;
    // Presence edges on the home/carousel screen; never repeat while held.
    if (oldScreen < 0 && newScreen < 0 && wasPresent != present)
        return present ? MenuSound::Enter : MenuSound::Exit;
    if (oldScreen < 0 && newScreen < 0 && present && oldItem != newItem)
        return MenuSound::Tick;
    return MenuSound::None;
}
#endif
