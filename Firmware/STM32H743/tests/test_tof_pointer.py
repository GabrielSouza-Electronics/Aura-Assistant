"""Host integration test: python tests/test_tof_pointer.py --compiler <g++>.

Compiles the tracker, Model, menu and disabled touch adapter. RTOS primitives and
unrelated application services are stubbed. Does not validate scheduling or hardware.
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
    (tmp / "test.cpp").write_text(r"""#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "app_hand_tracking.h"
extern "C" {
#include "app.h"
}
#include "app_ui_audio.h"
#include "app_ui_settings.h"
#include "STM32TouchController.hpp"
#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/MenuLogic.hpp>
#include <gui/common/HandInput.hpp>
static uint32_t now;
static APP_UIAudioEvent_t requested = APP_UI_AUDIO_NONE;
extern "C" void APP_UIAudio_Request(APP_UIAudioEvent_t event) { requested = event; }
extern "C" void APP_LED_SetHeroBreath(uint8_t) {}
extern "C" void APP_LED_SetCarousel(uint16_t, uint8_t) {}
extern "C" void APP_LED_MenuEnterPulse(void) {}
extern "C" void APP_LED_SetBreathColor(uint8_t, uint8_t, uint8_t) {}
extern "C" uint8_t APP_WiFi_GetSignalLevel(void) { return 0; }
extern "C" bool APP_UISettings_ReadText(APP_UISetting_t, uint32_t*, char*, size_t)
{ return false; }
extern "C" uint8_t APP_UISettings_GetVolume(void) { return 5; }
extern "C" void APP_UISettings_Request(APP_UISetting_t, int8_t) {}
static APP_InitStatus_t startupStatus = APP_INIT_WAITING_FOR_TOF;
static unsigned startupCompletions;
extern "C" APP_InitStatus_t APP_GetInitStatus(void) { return startupStatus; }
extern "C" void APP_DisplayStartupComplete(void) { ++startupCompletions; }
extern "C" uint32_t xTaskGetTickCount(void) { return now; }

class Input : public ModelListener {
public:
    bool present = false;
    unsigned clicks = 0, backs = 0;
    bool near = false;
    void handNearUpdated(bool n) override { near=n; }
    void handBackRequested() override { ++backs; }
    void handClicked() override { ++clicks; }
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
    assert(!model.startupReady());
    startupStatus = APP_INIT_OK;
    assert(model.startupReady());
    model.completeStartup(); assert(startupCompletions == 1);
    Input input;
    model.bind(&input);
    model.playMenuSound(MenuSound::Tick); assert(requested == APP_UI_AUDIO_TICK);
    model.playMenuSound(MenuSound::Enter); assert(requested == APP_UI_AUDIO_ENTER);
    model.playMenuSound(MenuSound::Exit); assert(requested == APP_UI_AUDIO_EXIT);
    model.playMenuSound(MenuSound::TaskComplete); assert(requested == APP_UI_AUDIO_TASK_COMPLETE);
    model.playMenuSound(MenuSound::TaskReopen); assert(requested == APP_UI_AUDIO_TASK_REOPEN);
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
    frame(150); model.tick(); assert(!input.present);
    frame(149); model.tick();
    assert(input.present && input.x == 0 && input.y == 0);
    // Hysteresis: retain selection mode through 200 mm, release above it.
    frame(150); model.tick(); assert(input.present);
    frame(199); model.tick(); assert(input.present);
    frame(200); model.tick(); assert(input.present);
    frame(201); model.tick();
    assert(!input.present && input.x == 0 && input.y == 0);
    frame(200); model.tick(); assert(!input.present);
    frame(150); model.tick(); assert(!input.present);
    frame(149); model.tick(); assert(input.present);
    frame(100); model.tick(); assert(input.present);
    frame(0); model.tick(); assert(!input.present);
    frame(100); now = 599; model.tick(); assert(input.present);
    now = 600; model.tick(); assert(!input.present);
    frame(100); model.tick(); assert(input.present);
    APP_HandTracking_Reset(); model.tick(); assert(!input.present);
    now = UINT32_MAX - 100; frame(100);
    now = 498; model.tick(); assert(input.present);
    now = 499; model.tick(); assert(!input.present);

    // A closer fingertip wins over all 15 palm zones, even by just 1 mm.
    const int gaps[] = {1, 80, 200};
    for (int finger = 0; finger < 16; ++finger) {
        for (int gap : gaps) {
            BSP_TOF_Data_t data = {};
            for (int zone = 0; zone < 16; ++zone) {
                data.targets_detected[zone] = 1;
                data.target_status[zone] = 5;
                data.distance_mm[zone] = 100 + gap;
            }
            data.distance_mm[finger] = 100;
            APP_HandTracking_Reset();
            APP_HandTracking_Process(&data); model.tick();
            const int coords[] = {-10, -3, 3, 10};
            assert(app_hand_tracking.raw_x == coords[finger % 4]);
            assert(app_hand_tracking.raw_y == coords[finger / 4]);
            assert(app_hand_tracking.raw_z_mm == 100);
            assert(app_hand_tracking.valid_zone_count == 1);
            assert(input.present);
        }
    }
    // Invalid near returns cannot steal the pointer; equal minima share X/Y.
    BSP_TOF_Data_t nearest = {};
    nearest.targets_detected[0] = nearest.targets_detected[15] = 1;
    nearest.target_status[0] = 4; nearest.distance_mm[0] = 20;
    nearest.target_status[15] = 9; nearest.distance_mm[15] = 100;
    APP_HandTracking_Reset(); APP_HandTracking_Process(&nearest);
    assert(app_hand_tracking.raw_x == 10 && app_hand_tracking.raw_y == 10);
    nearest.target_status[0] = 5; nearest.distance_mm[0] = 100;
    APP_HandTracking_Process(&nearest);
    assert(app_hand_tracking.raw_x == 0 && app_hand_tracking.raw_y == 0);
    assert(app_hand_tracking.valid_zone_count == 2);
    nearest.distance_mm[0] = 0;
    APP_HandTracking_Process(&nearest);
    assert(app_hand_tracking.raw_x == 10);
    nearest.distance_mm[0] = 20; nearest.targets_detected[0] = 0;
    APP_HandTracking_Process(&nearest);
    assert(app_hand_tracking.raw_x == 10);

    // Native Y range: no offset or gain, zero is neutral.
    const uint16_t yMasks[] = {0x000F, 0x400B, 0xFFFF, 0xF000};
    const int yAxes[] = {-10, -5, 0, 10};
    for (int i = 0; i < 4; ++i) {
        APP_HandTracking_Reset(); frame(100, yMasks[i]); model.tick();
        assert(app_hand_tracking.raw_y == yAxes[i]);
        assert(app_hand_tracking.y == yAxes[i]);
        assert(input.present && fabs(input.y - yAxes[i]/10.0f) < 0.000001f);
    }

    // All sensor magnitudes reach the Model unchanged except normalization.
    const uint16_t masks[] = {0x0001,0x1007,0x0002,0x400B,0x0004,0x800E,0x0008};
    const int axes[] = {-10,-5,-3,0,3,5,10};
    for (int d = 0; d < 7; ++d) {
        APP_HandTracking_Reset(); frame(100, masks[d]); model.tick();
        assert(input.present && fabs(input.x - axes[d] / 10.0f) < 0.000001f);
        assert(fabs(input.y - app_hand_tracking.y / 10.0f) < 0.000001f);
        assert(app_hand_tracking.x == axes[d]);
        MenuLogic menu;
        menu.tick(input.present, input.x, input.y);
        assert(fabs(menu.getAngle() - (-0.055f * input.x)) < 0.000001f);
        const float angle = menu.getAngle();
        for (int i = 0; i < 10; ++i) menu.tick(true, 0, 0);
        assert(fabs(menu.getAngle()) <= fabs(angle));
        const float centeredAngle = menu.getAngle();
        menu.tick(false, 1, -1);
        assert(menu.getAngle() == centeredAngle); // Release cannot rotate.
    }
    // Every integer X, both directions; Y cannot change angular velocity.
    for (int axis = -10; axis <= 10; ++axis) {
        for (int vertical = -10; vertical <= 10; ++vertical) {
            MenuLogic menu;
            menu.tick(true, axis / 10.0f, vertical / 10.0f);
            const float expected = (abs(axis) <= 2 && vertical >= -5)
                ? 0.0f : -0.055f * axis / 10.0f;
            assert(fabs(menu.getAngle() - expected) < 0.000001f);
            menu.tick(true, -axis / 10.0f, vertical / 10.0f);
            assert(fabs(menu.getAngle()) < 0.000001f);
        }
    }
    MenuLogic dwell;
    // Center capture at 20%, release above 35%, continuous off-center spin.
    MenuLogic snap;
    for (int i=0; i<30; ++i) snap.tick(true,0.5f,0);
    const int selected = snap.getSelected();
    snap.tick(true,0.2f,0);
    for (int i=0; i<55; ++i) snap.tick(true,0.3f,0);
    assert(snap.getSelected() == selected && snap.getScreen() == -1);
    assert(fabs(snap.getAngle() + selected * 6.28318530718f / ML_COUNT) < 0.0031f);
    float before = snap.getAngle();
    snap.tick(true,0.4f,0);
    assert(fabs(snap.getAngle() - before + 0.022f) < 0.000001f);
    before = snap.getAngle();
    for (int i=0; i<200; ++i) snap.tick(true,0.3f,0);
    assert(snap.getScreen() == -1);
    assert(fabs(snap.getAngle() - before + 3.3f) < 0.0001f);
    // Strict <30 mm; release commits a short click, 2 s commits only back.
    APP_HandTracking_Reset(); now=1000;
    frame(30); model.tick(); assert(!input.near && input.clicks==0);
    frame(29); model.tick(); assert(input.near && input.clicks==0);
    now+=200; frame(30); model.tick(); assert(input.clicks==1 && !input.near);
    model.tick(); assert(input.clicks==1);
    frame(29); model.tick();
    for(int i=0;i<9;++i) { now+=200; frame(29); model.tick(); }
    now+=199; frame(29); model.tick(); assert(input.backs==0 && input.clicks==1);
    ++now; frame(29); model.tick(); assert(input.backs==1 && input.clicks==1);
    for(int i=0;i<15;++i) { now+=200; frame(29); model.tick(); }
    assert(input.backs==1);
    frame(30); model.tick(); assert(input.clicks==1);
    frame(20); model.tick(); now+=200; frame(0); model.tick(); assert(input.clicks==2);
    frame(29); model.tick(); now+=600; model.tick(); assert(!input.near);
    frame(29); model.tick(); assert(input.backs==1);
    APP_HandTracking_Reset(); model.tick();
    now=UINT32_MAX-1000; frame(29); model.tick();
    for(int i=0;i<10;++i) { now+=200; frame(29); model.tick(); }
    assert(input.backs==2);
    frame(30); model.tick(); assert(input.clicks==2);

    MenuLogic clicked;
    MenuLogic proximity;
    for (int i=0;i<30;++i) proximity.tick(true,0.7f,0);
    const float heldAngle=proximity.getAngle();
    const int heldItem=proximity.getSelected();
    const float visible=proximity.getVisibility();
    for (int i=0;i<120;++i) {
        proximity.tick(true,1,-1,false,true);
        assert(proximity.getVisibility()>=visible);
        assert(proximity.getAngle()==heldAngle && proximity.getSelected()==heldItem);
        assert(proximity.getScreen()==-1 && proximity.getDwellStage()==-1);
    }
    proximity.tick(true,0,0,true);
    assert(proximity.getScreen()==heldItem);

    clicked.tick(true, 1, 0, true);
    assert(clicked.getScreen() == 0 && clicked.getAngle() == 0);
    assert(clicked.takeNavEvent() == 1); // Same visual effect as dwell.
    clicked.tick(true, 0, 0, true);
    assert(clicked.takeNavEvent() == 0); // No entry inside an open submenu.
    for (int i=1; i<HandInput::BACK_HOLD; ++i) clicked.tick(true,0,-1);
    assert(clicked.getScreen() == 0); // A short pull-down does not go back.
    clicked.tick(true,0,-1);
    assert(clicked.getScreen() == 0);
    clicked.close();
    clicked.tick(true,0,0,true); assert(clicked.getScreen() == 0);
    MenuLogic rejected;
    rejected.tick(false,0,0,true); assert(rejected.getScreen() == -1);
    rejected.tick(true,0,-1,true); assert(rejected.getScreen() == -1);

    for (int i=0; i<71; ++i) dwell.tick(true,0,0);
    assert(dwell.getScreen() == -1);
    dwell.tick(true,0,0); assert(dwell.getScreen() == 0);
    for (int i=0; i<HandInput::BACK_HOLD; ++i) dwell.tick(true,0,-1);
    assert(dwell.getScreen() == 0);
    dwell.close();
    MenuLogic noShortcut;
    for (int i=0; i<12; ++i) noShortcut.tick(true,0,1);
    assert(noShortcut.getScreen() == -1);
    // The old mouse-emulation path is disabled, preventing a second input source.
    STM32TouchController controller;
    int32_t x=123, y=456;
    assert(!controller.sampleTouch(x,y) && x==123 && y==456);
    puts("PASS: Model input, distance/click hysteresis, timeout, center capture, continuous rotation, reversal and navigation");
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
