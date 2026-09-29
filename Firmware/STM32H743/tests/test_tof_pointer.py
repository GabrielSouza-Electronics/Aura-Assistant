"""Host integration test: python tests/test_tof_pointer.py --compiler <g++>.

Compiles the tracker, Model, menu and disabled touch adapter; only the RTOS clock and
critical sections are stubbed. Does not validate RTOS scheduling or hardware.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", default="g++")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="aura_tof_") as directory:
    tmp = Path(directory)
    (tmp / "FreeRTOS.h").write_text("""#include <stdint.h>
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) (ms)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
""")
    (tmp / "task.h").write_text("""#ifdef __cplusplus
extern "C" {
#endif
TickType_t xTaskGetTickCount(void);
#ifdef __cplusplus
}
#endif
""")
    (tmp / "test.cpp").write_text(r"""#include <cassert>
#include <cmath>
#include <cstdio>
#include "app_hand_tracking.h"
#include "app_ui_audio.h"
#include "STM32TouchController.hpp"
#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/MenuLogic.hpp>
static uint32_t now;
static APP_UIAudioEvent_t requested = APP_UI_AUDIO_NONE;
extern "C" void APP_UIAudio_Request(APP_UIAudioEvent_t event) { requested = event; }
extern "C" uint32_t xTaskGetTickCount(void) { return now; }

class Input : public ModelListener {
public:
    bool present = false;
    float x = 0, y = 0;
    void handUpdated(bool p, float a, float b) override {
        present = p; x = a; y = b;
    }
};
static void frame(int mm, uint16_t mask = 0xFFFF) {
    BSP_TOF_Data_t data = {};
    for (int i = 0; i < 16; ++i) {
        if (mask & (1U << i)) {
            data.targets_detected[i] = mm > 0;
            data.target_status[i] = 5;
            data.distance_mm[i] = mm;
        }
    }
    APP_HandTracking_Process(&data);
}
int main() {
    Model model;
    Input input;
    model.bind(&input);
    model.playMenuSound(MenuSound::Tick); assert(requested == APP_UI_AUDIO_TICK);
    model.playMenuSound(MenuSound::Enter); assert(requested == APP_UI_AUDIO_ENTER);
    model.playMenuSound(MenuSound::Exit); assert(requested == APP_UI_AUDIO_EXIT);
    assert(menuSoundForTransition(-1, 0, -1, 1, true, true) == MenuSound::Tick);
    assert(menuSoundForTransition(-1, 4, -1, 0, true, true) == MenuSound::Tick);
    assert(menuSoundForTransition(-1, 1, 1, 1, true, true) == MenuSound::Enter);
    assert(menuSoundForTransition(1, 1, -1, 2, true, true) == MenuSound::Exit);
    assert(menuSoundForTransition(1, 1, 1, 2, true, true) == MenuSound::None);
    assert(menuSoundForTransition(-1, 1, -1, 1, true, true) == MenuSound::None);
    assert(menuSoundForTransition(-1, 1, -1, 2, false, false) == MenuSound::None);
    assert(menuSoundForTransition(-1, 0, -1, 0, false, true) == MenuSound::Enter);
    assert(menuSoundForTransition(-1, 0, -1, 0, true, false) == MenuSound::Exit);
    assert(menuSoundForTransition(-1, 0, -1, 1, false, true) == MenuSound::Enter);
    assert(menuSoundForTransition(-1, 0, -1, 0, false, false) == MenuSound::None);
    assert(menuSoundForTransition(0, 0, 0, 0, true, false) == MenuSound::None);
    assert(menuSoundForTransition(0, 0, 0, 0, false, true) == MenuSound::None);
    assert(menuSoundForTransition(0, 0, -1, 0, true, false) == MenuSound::Exit);
    model.tick();
    assert(!input.present);
    frame(149); model.tick();
    assert(input.present && input.x == 0 && input.y == 0);
    frame(150); model.tick();
    assert(!input.present && input.x == 0 && input.y == 0);
    frame(100); model.tick(); assert(input.present);
    frame(0); model.tick(); assert(!input.present);
    frame(100); now = 599; model.tick(); assert(input.present);
    now = 600; model.tick(); assert(!input.present);
    frame(100); model.tick(); assert(input.present);
    APP_HandTracking_Reset(); model.tick(); assert(!input.present);
    now = UINT32_MAX - 100; frame(100);
    now = 498; model.tick(); assert(input.present);
    now = 499; model.tick(); assert(!input.present);

    // Native Y range: no offset or gain, zero is neutral.
    const uint16_t yMasks[] = {0x000F, 0x400B, 0xFFFF, 0xF000};
    const int yAxes[] = {-10, -5, 0, 10};
    for (int i = 0; i < 4; ++i) {
        APP_HandTracking_Reset(); frame(100, yMasks[i]); model.tick();
        assert(app_hand_tracking.raw_y == yAxes[i]);
        assert(app_hand_tracking.y == yAxes[i]);
        assert(input.present && std::fabs(input.y - yAxes[i]/10.0f) < 0.000001f);
    }

    // All sensor magnitudes reach the Model unchanged except normalization.
    const uint16_t masks[] = {0x0001,0x1007,0x0002,0x400B,0x0004,0x800E,0x0008};
    const int axes[] = {-10,-5,-3,0,3,5,10};
    for (int d = 0; d < 7; ++d) {
        APP_HandTracking_Reset(); frame(100, masks[d]); model.tick();
        assert(input.present && std::fabs(input.x - axes[d] / 10.0f) < 0.000001f);
        assert(std::fabs(input.y - app_hand_tracking.y / 10.0f) < 0.000001f);
        assert(app_hand_tracking.menu_speed == (axes[d] < 0 ? -axes[d] : axes[d]));
        MenuLogic menu;
        menu.tick(input.present, input.x, input.y);
        assert(std::fabs(menu.getAngle() - (-0.055f * input.x)) < 0.000001f);
        const float angle = menu.getAngle();
        for (int i = 0; i < 10; ++i) menu.tick(true, 0, 0);
        assert(menu.getAngle() == angle); // No snap at X=0.
        menu.tick(false, 1, -1);
        assert(menu.getAngle() == angle); // Release cannot rotate.
    }
    // Every integer X, both directions; Y cannot change angular velocity.
    for (int axis = -10; axis <= 10; ++axis) {
        for (int vertical = -10; vertical <= 10; ++vertical) {
            MenuLogic menu;
            menu.tick(true, axis / 10.0f, vertical / 10.0f);
            assert(std::fabs(menu.getAngle() + 0.055f * axis / 10.0f) < 0.000001f);
            menu.tick(true, -axis / 10.0f, vertical / 10.0f);
            assert(std::fabs(menu.getAngle()) < 0.000001f);
        }
    }
    MenuLogic dwell;
    for (int i=0; i<71; ++i) dwell.tick(true,0,0);
    assert(dwell.getScreen() == -1);
    dwell.tick(true,0,0); assert(dwell.getScreen() == 0);
    for (int i=0; i<12; ++i) dwell.tick(true,0,-1);
    assert(dwell.getScreen() == -1);
    MenuLogic noShortcut;
    for (int i=0; i<12; ++i) noShortcut.tick(true,0,1);
    assert(noShortcut.getScreen() == -1);
    // The old mouse-emulation path is disabled, preventing a second input source.
    STM32TouchController controller;
    int32_t x=123, y=456;
    assert(!controller.sampleTouch(x,y) && x==123 && y==456);
    puts("PASS: direct Model input, lifecycle/timeout, proportional X at all 21 values, Y independence, no snap, reversal and navigation");
}
""")
    includes = [tmp, root / "App/Inc", root / "BSP/Inc",
                root / "TouchGFX/target", root / "TouchGFX/gui/include",
                root / "Middlewares/ST/touchgfx/framework/include"]
    common = [args.compiler, "-Wall", "-Wextra", "-Werror", "-DSTM32H743xx"]
    common += [f"-I{path}" for path in includes]
    obj = tmp / "tracker.o"
    subprocess.run(common + ["-x", "c", "-std=c11", "-c",
                             str(root / "App/Src/app_hand_tracking.c"),
                             "-o", str(obj)], check=True)
    executable = tmp / "test.exe"
    subprocess.run(common + ["-std=c++11", str(tmp / "test.cpp"), str(obj),
                             str(root / "TouchGFX/target/STM32TouchController.cpp"),
                             str(root / "TouchGFX/gui/src/common/MenuLogic.cpp"),
                             str(root / "TouchGFX/gui/src/model/Model.cpp"),
                             "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
