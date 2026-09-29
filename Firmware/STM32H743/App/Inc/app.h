#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stddef.h>

#include "bsp_power.h"
#include "bsp_imu.h"
#include "bsp_tof.h"
#include "bsp_audio_out.h"
#include "bsp_audio_in.h"
#include "bsp_wifi.h"
#include "bsp_led.h"

typedef enum
{
    APP_INIT_NOT_STARTED = 0,
    APP_INIT_OK,
    APP_INIT_FLASH_BSP_ERROR,
    APP_INIT_FLASH_MEMORY_MAPPED_ERROR,
    APP_INIT_FLASH_CONTENT_ERROR,
    APP_INIT_LCD_BSP_ERROR,
    APP_INIT_LCD_CONTROLLER_ERROR,
    APP_INIT_LCD_FRAMEBUFFER_ERROR,
    APP_INIT_LCD_BACKLIGHT_ERROR,
    APP_INIT_LED_BSP_ERROR,
    APP_INIT_LED_FILL_ERROR,
    APP_INIT_LED_TRANSFER_ERROR,
    APP_INIT_WAITING_FOR_TOF
} APP_InitStatus_t;

typedef struct
{
    uint32_t magic;
    uint32_t poll_count;
    uint32_t init_status;
    uint32_t system_core_clock_hz;
    uint32_t ltdc_gcr;
    uint32_t ltdc_cpsr;
    uint32_t ltdc_cdsr;
    uint32_t ltdc_isr;
    uint32_t ltdc_ier;
    uint32_t ltdc_error_count;
    uint32_t ltdc_last_error;
    uint32_t line_change_count;
    uint32_t layer_cfb_address;
    uint32_t framebuffer_first_word;
    uint32_t qspi_probe_word;
    uint32_t gpioa_idr;
    uint32_t gpioc_idr;
    uint32_t gpioe_idr;
    uint32_t gpiog_idr;
    uint32_t lcd_serial_clock_edges;
    uint32_t lcd_command_count;
} APP_DisplayDiagnostics_t;

extern volatile APP_DisplayDiagnostics_t app_display_diagnostics;
extern volatile BSP_LED_Status_t app_led_status;

typedef struct
{
    uint32_t magic;
    uint32_t update_count;
    uint32_t error_count;
    BSP_POWER_Status_t status;
    BSP_POWER_Data_t data;
} APP_PowerDiagnostics_t;

extern volatile APP_PowerDiagnostics_t app_power_diagnostics;

typedef struct
{
    uint32_t magic;
    uint32_t update_count;
    uint32_t error_count;
    BSP_IMU_Status_t init_status;
    BSP_IMU_Status_t read_status;
    BSP_IMU_Data_t data;
} APP_IMUDiagnostics_t;

typedef struct
{
    uint32_t magic;
    uint32_t update_count;
    uint32_t no_data_count;
    uint32_t error_count;
    BSP_TOF_Status_t init_status;
    BSP_TOF_Status_t read_status;
    BSP_TOF_Data_t data;
} APP_TOFDiagnostics_t;

extern volatile APP_IMUDiagnostics_t app_imu_diagnostics;
extern volatile APP_TOFDiagnostics_t app_tof_diagnostics;

typedef struct
{
    uint32_t magic;
    uint32_t play_count;
    uint32_t error_count;
    BSP_AUDIO_OUT_Status_t init_status;
    BSP_AUDIO_OUT_Status_t last_status;
    bool busy;
    uint32_t last_hal_error;
} APP_AudioOutDiagnostics_t;

extern volatile APP_AudioOutDiagnostics_t app_audio_out_diagnostics;

/* Hardware validation mode: microphone PDM -> PCM -> I2S speaker loopback.
   It owns the audio output path, so UI sound effects are suspended. */
#define APP_AUDIO_ECHO_TEST 0U

typedef struct
{
    uint32_t magic;
    uint32_t block_count;
    uint32_t error_count;
    uint32_t overrun_count;
    uint32_t output_underrun_count;
    uint32_t peak_left;
    uint32_t peak_right;
    BSP_AUDIO_IN_Status_t init_status;
    BSP_AUDIO_IN_Status_t start_status;
    BSP_AUDIO_IN_Status_t last_input_status;
    BSP_AUDIO_OUT_Status_t last_output_status;
    uint32_t input_hal_error;
    uint32_t output_hal_error;
    uint32_t recorded_samples;
    uint32_t processed_peak;
    uint32_t combined_peak;
    uint32_t noise_floor;
    uint32_t gate_threshold;
    uint32_t gain_q12;
    int8_t right_polarity;
    bool speech_detected;
    bool recording;
    bool playing;
    bool running;
} APP_AudioEchoDiagnostics_t;

extern volatile APP_AudioEchoDiagnostics_t app_audio_echo_diagnostics;

typedef enum
{
    APP_WIFI_STATE_OFF = 0,
    APP_WIFI_STATE_POWERED,
    APP_WIFI_STATE_CORE_READY,
    APP_WIFI_STATE_RADIOS_READY,
    APP_WIFI_STATE_BLE_ADVERTISING,
    APP_WIFI_STATE_ERROR
} APP_WiFiState_t;

typedef struct
{
    uint32_t magic;
    APP_WiFiState_t state;
    uint32_t error_count;
    BSP_WIFI_Status_t bsp_status;
    uint32_t core_status;
    uint32_t callback_status;
    uint32_t wifi_init_status;
    uint32_t net_init_status;
    uint32_t heap_min_free;
    uint32_t wifi_scan_status;
    uint32_t wifi_scan_callback_status;
    uint32_t wifi_ap_count;
    int32_t wifi_best_rssi;
    int32_t wifi_rssi;          /* connected AP, INT32_MIN when unknown */
    uint32_t wifi_signal_level; /* 0 = no IP, 1..3 = bars shown on the display */
    uint32_t ble_init_status;
    uint32_t provision_init_status;
    uint32_t ble_address_status;
    uint32_t ble_adv_status;
    uint32_t last_wifi_event;
    uint32_t last_net_event;
    uint32_t last_ble_event;
    uint32_t last_driver_error;
    uint32_t first_driver_error;
    const char *first_driver_error_function;
    const char *last_driver_error_function;
    uint32_t ble_connection_count;
    uint8_t module_mac[6];
    uint8_t module_sdk_version[4];
    uint8_t module_at_version[4];
    uint8_t module_build_date[32];
    char module_name[25];
    uint32_t module_id;
    uint8_t ble_address[6];
    bool powered;
    bool enabled;
    bool ready;
    bool wifi_connected;
    bool wifi_has_ip;
    bool ble_connected;
} APP_WiFiDiagnostics_t;

extern volatile APP_WiFiDiagnostics_t app_wifi_diagnostics;

void APP_Init(void);
void APP_LEDTask(void);
/* Normalized Hero breath (0..255), published by the UI task. */
void APP_LED_SetHeroBreath(uint8_t level);
/* One turn = 65536 phase units; visibility 0 restores the idle breath. */
void APP_LED_SetCarousel(uint16_t phase, uint8_t visibility);
/* UI-task entry confirmation; one brief brightness dip, never fully off. */
void APP_LED_MenuEnterPulse(void);
void APP_PowerTask(void);
void APP_SensorTask(void);
void APP_SystemTask(void);
void APP_AudioOutputTask(void);
void APP_AudioInputTask(void);
void APP_WiFiTask(void);
/* Display signal level: 0 = no IP (crossed icon), 1..3 = bars from RSSI.
 * Safe to call from any task (single byte read). */
uint8_t APP_WiFi_GetSignalLevel(void);
const int16_t *APP_AudioRecording_Get(size_t *sample_count,
                                      uint32_t *sample_rate_hz);
APP_InitStatus_t APP_GetInitStatus(void);
void APP_DisplayDiagnosticsPoll(void);
/* UI-task request after the splash finishes; release occurs after rendering. */
void APP_DisplayStartupComplete(void);
void APP_DisplayFramePresented(void);

#endif
