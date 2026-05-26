@echo off
setlocal
cd /d %~dp0

set BOARD_DIR=sdk\boards\zoomtkldyna

set CFLAGS=-mcpu=cortex-m3 -mthumb -O2 -g3 ^
 -ffreestanding -fomit-frame-pointer ^
 -fno-exceptions -fno-unwind-tables ^
 -ffunction-sections -fdata-sections ^
 -Wall -Isdk\include -Iinclude -I%BOARD_DIR% -Iapp -Iapp\widgets

set LDFLAGS=-L sdk -T sdk\fr8000.ld --gc-sections --emit-relocs --undefined=fw_rtc_set

if not exist build mkdir build
del /q build\*.o 2>nul


for %%f in (src\*.c src\widgets\*.c fonts\*.c app\*.c app\widgets\*.c app\screens\*.c) do (
    arm-none-eabi-gcc %CFLAGS% -c %%f -o build\%%~nf.o || goto fail
)

for %%f in (sdk\src\*.c sdk\src\*.s) do (
    arm-none-eabi-gcc %CFLAGS% -c %%f -o build\%%~nf.o || goto fail
)

arm-none-eabi-ld %LDFLAGS% build\*.o -o build\meletricks.elf || goto fail
arm-none-eabi-objcopy -O binary build\meletricks.elf build\meletricks.bin || goto fail
echo Build OK

arm-none-eabi-nm --numeric-sort build\meletricks.elf | findstr /V " A "
goto end

:fail
echo Build FAILED
exit /b 1

:end
endlocal
