@echo off
set "remote=192.168.50.216:2022"
set "drive=f:\"

timeout 2
arm-none-eabi-gdb build/firmware.elf -q -batch -nx -ex "target extended-remote %remote%" -ex "shell timeout 2" -ex "monitor reset_usb_boot"
@REM :wait_for_drive
@REM if not exist %drive% (
@REM     goto :wait_for_drive
@REM )
@REM copy build\firmware.uf2 "%drive%"