"""Compile the real audio BSP against a fake HAL; verify restart and DMA ownership."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import wave
import struct
import re

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='gcc')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'Components/Audio/ui_audio.c').read_text()
for symbol, name in [('nav_tick', '10_nav_tick'), ('menu_enter', '20_menu_enter'), ('menu_exit', '21_menu_exit'), ('task_complete', '40_success')]:
    with wave.open(str(root / f'aura_assets/audio/ui/{name}.wav')) as wav:
        samples = struct.unpack('<' + 'h' * wav.getnframes(), wav.readframes(wav.getnframes()))
    array = source.split(f'const int16_t ui_{symbol}[')[1].split('{')[1].split('}')[0]
    assert tuple(map(int, re.findall(r'-?\d+', array))) == samples

with tempfile.TemporaryDirectory(prefix='aura_audio_') as directory:
    tmp = Path(directory)
    (tmp / 'i2s.h').write_text(r'''
#ifndef FAKE_I2S_H
#define FAKE_I2S_H
#include <stdint.h>
typedef struct { struct { int Mode; } Init; } DMA_HandleTypeDef;
typedef struct { DMA_HandleTypeDef *hdmatx; uint32_t ErrorCode; } I2S_HandleTypeDef;
extern I2S_HandleTypeDef hi2s1;
#define HAL_OK 0
#define HAL_I2S_ERROR_NONE 0
#define DMA_NORMAL 0
#define DMA_CIRCULAR 1
#define HAL_DMA_STATE_READY 0
#define HAL_DMA_ERROR_NO_XFER 4
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define I2S_SDMODE_GPIO_Port 0
#define I2S_SDMODE_Pin 0
#define __NOP() ((void)0)
#define __HAL_I2S_DISABLE(h) ((void)(h))
int HAL_I2S_DMAStop(I2S_HandleTypeDef *h);
int HAL_I2S_Transmit_DMA(I2S_HandleTypeDef *h, uint16_t *p, uint16_t n);
int HAL_DMA_Init(DMA_HandleTypeDef *h);
int HAL_DMA_DeInit(DMA_HandleTypeDef *h);
int HAL_DMA_GetState(DMA_HandleTypeDef *h);
uint32_t HAL_DMA_GetError(DMA_HandleTypeDef *h);
void HAL_GPIO_WritePin(int port, int pin, int state);
void SCB_CleanDCache_by_Addr(uint32_t *p, int32_t n);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
#endif
''')
    (tmp / 'main.h').write_text('#include "i2s.h"\n')
    (tmp / 'test.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include "i2s.h"
#include "bsp_audio_out.h"
static DMA_HandleTypeDef dma;
I2S_HandleTypeDef hi2s1 = {&dma, 0};
static int active, starts, stops, fail_start, fail_stop;
static uint32_t dma_error;
static uint16_t *buffer, count;
int HAL_I2S_DMAStop(I2S_HandleTypeDef *h) {
    (void)h; ++stops;
    if (fail_stop) { dma_error=8; return 1; }
    if (!active) { dma_error=HAL_DMA_ERROR_NO_XFER; return 1; }
    active=0; dma_error=0; return 0;
}
int HAL_DMA_GetState(DMA_HandleTypeDef *h) { (void)h; return active ? 1 : HAL_DMA_STATE_READY; }
uint32_t HAL_DMA_GetError(DMA_HandleTypeDef *h) { (void)h; return dma_error; }
int HAL_I2S_Transmit_DMA(I2S_HandleTypeDef *h, uint16_t *p, uint16_t n) {
    (void)h; assert(!active); if(fail_start) return 1;
    active=1; ++starts; buffer=p; count=n; return 0;
}
int HAL_DMA_Init(DMA_HandleTypeDef *h) { (void)h; return 0; }
int HAL_DMA_DeInit(DMA_HandleTypeDef *h) { (void)h; return 0; }
void HAL_GPIO_WritePin(int port,int pin,int state) { (void)port;(void)pin;(void)state; }
void SCB_CleanDCache_by_Addr(uint32_t *p,int32_t n) { (void)p;(void)n; }
uint32_t HAL_GetTick(void) { return 0; }
void HAL_Delay(uint32_t ms) { (void)ms; }
static unsigned stream_step;
static void prepare_wait(void) { stream_step=0; }
static bool stream_wait(uint32_t timeout_ms) {
    (void)timeout_ms;
    assert(stream_step < 3);
    if (stream_step++ % 2 == 0) BSP_AUDIO_OUT_HalfTransferCallback();
    else BSP_AUDIO_OUT_TransferCompleteCallback();
    return true;
}
int main(void) {
    const int16_t tick[] = {100, -200, 300, -400};
    const int16_t enter[] = {500, 600};
    assert(BSP_AUDIO_OUT_Init() == BSP_AUDIO_OUT_OK);
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4) == BSP_AUDIO_OUT_OK);
    assert(active && BSP_AUDIO_OUT_IsBusy() && count==8);
    assert((int16_t)buffer[0]==50 && (int16_t)buffer[1]==50);
    assert((int16_t)buffer[2]==-100 && (int16_t)buffer[3]==-100);
    buffer[0]=999; // Simulate observing later playback, then restart same tick.
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4)==BSP_AUDIO_OUT_OK);
    assert(starts==2 && stops==2 && buffer[0]==50 && count==8);
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(enter,2)==BSP_AUDIO_OUT_OK);
    assert(starts==3 && count==4 && buffer[0]==250);
    BSP_AUDIO_OUT_HalfTransferCallback(); assert(BSP_AUDIO_OUT_IsBusy());
    BSP_AUDIO_OUT_TransferCompleteCallback(); assert(!BSP_AUDIO_OUT_IsBusy() && !active);
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(NULL,1)==BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT);
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,24001)==BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT);
    dma.Init.Mode=DMA_CIRCULAR; // Left behind by the circular welcome stream.
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4)==BSP_AUDIO_OUT_OK);
    assert(dma.Init.Mode==DMA_NORMAL);
    BSP_AUDIO_OUT_TransferCompleteCallback();
    fail_start=1;
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4)==BSP_AUDIO_OUT_ERROR_DMA_START);
    assert(!BSP_AUDIO_OUT_IsBusy());
    fail_start=0;
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4)==BSP_AUDIO_OUT_OK);
    BSP_AUDIO_OUT_TransferCompleteCallback();
    fail_stop=1;
    assert(BSP_AUDIO_OUT_PlayEffect48kMono(tick,4)==BSP_AUDIO_OUT_ERROR_DMA);
    fail_stop=0;
    static int16_t success[33600];
    for (unsigned i=0;i<33600;++i) success[i]=1000;
    BSP_AUDIO_OUT_SetSynchronizationHooks(prepare_wait,stream_wait,NULL);
    assert(BSP_AUDIO_OUT_PlayPCM48kMonoBlocking(success,33600,2000)==BSP_AUDIO_OUT_OK);
    assert(stream_step==3 && !active && !BSP_AUDIO_OUT_IsBusy());
    puts("PASS: lossless assets, mono/stereo volume, restart, replacement, completion, ownership and start failure");
}
''')
    exe = tmp / 'test.exe'
    subprocess.run([args.compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    f'-I{tmp}', f'-I{root / "BSP/Inc"}',
                    str(root / 'BSP/Src/bsp_audio_out.c'), str(tmp / 'test.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

    (tmp / 'FreeRTOS.h').write_text('''#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
''')
    (tmp / 'task.h').write_text('')
    (tmp / 'cmsis_os2.h').write_text('''#include <stdint.h>
typedef void *osThreadId_t;
#define osFlagsWaitAny 0U
#define osWaitForever 0xffffffffU
osThreadId_t osThreadGetId(void);
uint32_t osThreadFlagsSet(osThreadId_t thread,uint32_t flags);
uint32_t osThreadFlagsWait(uint32_t flags,uint32_t options,uint32_t timeout);
int osDelay(uint32_t ticks);
''')
    (tmp / 'owner.c').write_text(r'''
#include "app_ui_audio.h"
#include "bsp_audio_out.h"
#include "ui_audio.h"
#include "cmsis_os2.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf done;
static unsigned played, effects;
osThreadId_t osThreadGetId(void) { return (void *)1; }
uint32_t osThreadFlagsSet(osThreadId_t thread,uint32_t flags) { assert(thread==(void *)1); return flags; }
int osDelay(uint32_t ticks) { (void)ticks; return 0; }
bool BSP_AUDIO_OUT_IsBusy(void) { return false; }
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayEffect48kMono(const int16_t *pcm,size_t count)
{ (void)pcm; (void)count; ++effects; return BSP_AUDIO_OUT_OK; }
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayPCM48kMonoBlocking(const int16_t *pcm,size_t count,uint32_t timeout)
{
    assert(timeout==2000);
    assert(played<4);
    assert(pcm==(played%2?ui_menu_exit:ui_task_complete));
    assert(count==(played%2?UI_MENU_EXIT_COUNT:UI_TASK_COMPLETE_COUNT));
    if (played++==0) {
        APP_UIAudio_Request(APP_UI_AUDIO_TASK_REOPEN);
        APP_UIAudio_Request(APP_UI_AUDIO_TICK);
    }
    return BSP_AUDIO_OUT_OK;
}
uint32_t osThreadFlagsWait(uint32_t flags,uint32_t options,uint32_t timeout)
{
    (void)flags; (void)options; assert(timeout==osWaitForever);
    assert(played==4 && effects==0); longjmp(done,1);
    return 0;
}
int main(void)
{
    APP_UIAudio_Request(APP_UI_AUDIO_TASK_COMPLETE);
    APP_UIAudio_Request(APP_UI_AUDIO_TASK_REOPEN);
    APP_UIAudio_Request(APP_UI_AUDIO_TASK_COMPLETE);
    if (!setjmp(done)) APP_UIAudio_Run();
    puts("PASS: distinct completion/reopen PCM, FIFO sound order and requests during playback");
    return 0;
}
''')
    exe = tmp / 'owner.exe'
    subprocess.run([args.compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
        f'-I{tmp}', f'-I{root / "BSP/Inc"}', f'-I{root / "App/Inc"}',
        f'-I{root / "Components/Audio"}', str(root / 'App/Src/app_ui_audio.c'),
        str(root / 'Components/Audio/ui_audio.c'), str(tmp / 'owner.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
