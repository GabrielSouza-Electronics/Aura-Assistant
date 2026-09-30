# Startup overlay validation

## Files inspected and modified
Inspected Screen1View.cpp/.hpp, generated base widget positions, existing divider
assets, typography generator, CMake and simulator Makefile. Modified the user
Screen1View classes and explicit build entries. Added gen_startup.py and generated
StartupAssets.cpp/.hpp; updated preview_startup.py and startup_text PNG/GIF.

## LCD configuration and sources
Existing 480x480 RGB565 single framebuffer and Portrait rotation preserved.
No hardware initialization changes or new datasheet assumptions. Pixel sprites
follow the existing calendar/typography QSPI rendering path.

## Architecture
Three PixelDataWidget instances display STARTING, PLEASE WAIT with four fixed
ellipsis stages, DEVELOPED BY:, and GABRIEL SOUZA. No AI ASSISTANT title.
Existing divider and animated spark are reused at logical y=370; spark moves
sinusoidally along the line. Independent modulo-360 animation phase continues
even when startupTicks saturates during sensor wait. Dock transition fades the
overlay, hides its widgets and restores normal divider coordinates/alpha.
Original readiness gate and startup completion callback preserved.

## Validation and memory
python -B aura_assets/gen/gen_startup.py: six sprites validated with transparent
edges and <=256 colors. Updated approved preview generated without title.
cmake --build --preset Debug: passed; log build/Debug/startup-build.log.
DTCM 126040 bytes (+144), internal flash 728492 (+1408), QSPI 4177020 (+133352).
Framebuffer, D2 and D3 unchanged. No extra framebuffer or application heap buffer.
Existing objcopy warning for empty external load segment remains.

## Remaining assumptions and hardware checks
Animation timing uses existing GUI ticks (nominal 60 Hz). No hardware flashing
or physical validation performed. Program matching internal/QSPI images, then
check prolonged startup wait, dot animation, spark motion and completion fade.
