"""Host checks for Settings navigation, value limits and backlight duty.

The real App settings module runs with BSP/service fakes. The BSP backlight
functions are extracted unchanged and exercised with a fake GPIO; this checks
logic, not interrupt timing, LED current, luminance or physical flicker.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='aura_settings_') as directory:
    tmp = Path(directory)
    exe = tmp / 'nav.exe'
    subprocess.run([args.cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'TouchGFX/gui/include'),
                    str(root / 'tests/host/test_settings_nav.cpp'),
                    str(root / 'TouchGFX/gui/src/common/SettingsLogic.cpp'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    (tmp / 'bsp_audio_out.h').write_text('''#include <stdint.h>
uint8_t BSP_AUDIO_OUT_GetVolume(void);
void BSP_AUDIO_OUT_SetVolume(uint8_t volume);
''')
    (tmp / 'settings.c').write_text(r'''
#include "app_ui_settings.h"
#include "bsp_lcd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int critical_depth;
static uint8_t volume = 5, brightness = 100;
static unsigned ble_requests;
static bool enabled, fail_brightness;
uint8_t BSP_AUDIO_OUT_GetVolume(void) { return volume; }
void BSP_AUDIO_OUT_SetVolume(uint8_t value) { volume = value > 10 ? 10 : value; }
uint8_t BSP_LCD_GetBrightness(void) { return brightness; }
BSP_LCD_Status_t BSP_LCD_SetBrightness(uint8_t value) {
    if (fail_brightness) return BSP_LCD_ERROR_NOT_INITIALIZED;
    brightness = value; return BSP_LCD_OK;
}
void APP_ProvisionRequestEnabled(bool value) { enabled = value; ++ble_requests; }
int main(void) {
    uint32_t version = 0;
    char text[APP_UI_SETTING_TEXT_SIZE];
    assert(APP_UISettings_ReadText(APP_UI_SETTING_BRIGHTNESS, &version, text, sizeof(text)));
    assert(!strcmp(text, "100%"));
    for (unsigned i=0; i<15; ++i) APP_UISettings_Request(APP_UI_SETTING_BRIGHTNESS, -1);
    assert(brightness == 10);
    APP_UISettings_Request(APP_UI_SETTING_BRIGHTNESS, 1); assert(brightness == 20);
    assert(APP_UISettings_ReadText(APP_UI_SETTING_BRIGHTNESS, &version, text, sizeof(text)));
    assert(!strcmp(text, "20%"));
    fail_brightness = true;
    APP_UISettings_Request(APP_UI_SETTING_BRIGHTNESS, 1);
    assert(!APP_UISettings_ReadText(APP_UI_SETTING_BRIGHTNESS, &version, text, sizeof(text)));
    fail_brightness = false;
    for (unsigned i=0; i<15; ++i) APP_UISettings_Request(APP_UI_SETTING_BRIGHTNESS, 1);
    assert(brightness == 100);
    for (unsigned i=0; i<15; ++i) APP_UISettings_Request(APP_UI_SETTING_SOUND, -1);
    assert(APP_UISettings_GetVolume() == 0);
    APP_UISettings_Request(APP_UI_SETTING_SOUND, 1); assert(volume == 1);
    for (unsigned i=0; i<15; ++i) APP_UISettings_Request(APP_UI_SETTING_SOUND, 1);
    assert(APP_UISettings_GetVolume() == 10);
    APP_UISettings_Request(APP_UI_SETTING_BLUETOOTH, 1); assert(enabled);
    APP_UISettings_Request(APP_UI_SETTING_BLUETOOTH, -1); assert(!enabled);
    APP_UISettings_Request(APP_UI_SETTING_WIFI, 1);
    APP_UISettings_Request(APP_UI_SETTING_WIFI, -1);
    APP_UISettings_Request(APP_UI_SETTING_BLUETOOTH, 0);
    assert(ble_requests == 2);
    const char *ssid = "01234567890123456789012345678901";
    APP_UISettings_SetText(APP_UI_SETTING_WIFI, ssid); version = 0;
    assert(APP_UISettings_ReadText(APP_UI_SETTING_WIFI, &version, text, sizeof(text)));
    assert(!strcmp(text, ssid));
    APP_UISettings_SetText(APP_UI_SETTING_WIFI, ssid);
    assert(!APP_UISettings_ReadText(APP_UI_SETTING_WIFI, &version, text, sizeof(text)));
    assert(critical_depth == 0);
    puts("PASS: Settings ranges, brightness errors, BLE requests and read-only SSID");
}
''')
    common = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
              '-I' + str(tmp), '-I' + str(root / 'tests/provisioning_fakes'),
              '-I' + str(root / 'App/Inc'), '-I' + str(root / 'BSP/Inc')]
    exe = tmp / 'settings.exe'
    subprocess.run([*common, str(root / 'App/Src/app_ui_settings.c'),
                    str(tmp / 'settings.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

    source = (root / 'BSP/Src/bsp_lcd.c').read_text()
    begin = source.index('BSP_LCD_Status_t BSP_LCD_SetBacklight(')
    end = source.index('BSP_LCD_Status_t BSP_LCD_WriteCommand(', begin)
    (tmp / 'pwm.c').write_text(r'''
#include "bsp_lcd.h"
#include <assert.h>
#include <stdio.h>
static bool lcd_initialized;
static volatile bool lcd_backlight_enabled;
static volatile uint8_t lcd_brightness = 100;
static uint8_t lcd_pwm_phase, lcd_pwm_duty;
static unsigned pin;
#define LCD_BL_GPIO_Port 1
#define LCD_BL_Pin 2
#define BSP_LCD_BACKLIGHT_ON 1
#define BSP_LCD_BACKLIGHT_OFF 0
static void HAL_GPIO_WritePin(int port, int number, unsigned value) {
    assert(port == 1 && number == 2); pin = value;
}
''' + source[begin:end] + r'''
int main(void) {
    BSP_LCD_BacklightTick1ms(); assert(pin == 0);
    assert(BSP_LCD_SetBrightness(50) == BSP_LCD_ERROR_NOT_INITIALIZED);
    lcd_initialized = true;
    assert(BSP_LCD_SetBrightness(0) == BSP_LCD_ERROR_INVALID_ARGUMENT);
    assert(BSP_LCD_SetBrightness(55) == BSP_LCD_ERROR_INVALID_ARGUMENT);
    assert(BSP_LCD_SetBrightness(110) == BSP_LCD_ERROR_INVALID_ARGUMENT);
    for (unsigned percent=10; percent<=100; percent+=10) {
        assert(BSP_LCD_SetBacklight(false) == BSP_LCD_OK);
        BSP_LCD_BacklightTick1ms();
        assert(BSP_LCD_SetBrightness(percent) == BSP_LCD_OK);
        assert(BSP_LCD_SetBacklight(true) == BSP_LCD_OK);
        unsigned high = 0;
        for (unsigned ms=0; ms<100; ++ms) { BSP_LCD_BacklightTick1ms(); high += pin; }
        assert(high == percent);
    }
    BSP_LCD_SetBacklight(false); assert(pin == 0);
    for (unsigned i=0; i<20; ++i) { BSP_LCD_BacklightTick1ms(); assert(pin == 0); }
    BSP_LCD_SetBrightness(10); BSP_LCD_SetBacklight(true);
    BSP_LCD_BacklightTick1ms(); assert(pin == 1);
    BSP_LCD_SetBrightness(100);
    for (unsigned i=1; i<10; ++i) { BSP_LCD_BacklightTick1ms(); assert(pin == 0); }
    for (unsigned i=0; i<10; ++i) { BSP_LCD_BacklightTick1ms(); assert(pin == 1); }
    puts("PASS: PWM 10..100%, off gating and period-boundary changes");
}
''')
    exe = tmp / 'pwm.exe'
    subprocess.run([*common, str(tmp / 'pwm.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
