@echo off
rem builds mdv.exe - one freestanding Win32 executable, no CRT
cd /d %~dp0
rem machine-specific tool paths go in build.local.bat (not committed)
if exist "%~dp0build.local.bat" call "%~dp0build.local.bat"
if not defined WINDRES set WINDRES=windres
if not defined GCC set GCC=gcc
rem NOTE: no -fdata-sections here - it makes ld dump zero-initialized static
rem arrays (e.g. the enumerated font-name table) into .data instead of .bss,
rem bloating the file with disk-stored zero bytes for no runtime benefit.
set CFLAGS=-Os -flto -nostdlib -nostartfiles -fno-ident -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fmerge-all-constants -falign-functions=1 -falign-jumps=1 -falign-loops=1 -falign-labels=1
set LFLAGS=-Wl,-e,_start -Wl,--gc-sections -Wl,--subsystem,windows -Wl,--build-id=none -s
%WINDRES% mdv.rc -O coff -o mdv.res.o
if errorlevel 1 (echo RESOURCE BUILD FAILED & exit /b 1)
%GCC% %CFLAGS% %LFLAGS% -o mdv.exe mdv.c mdv.res.o -lkernel32 -luser32 -lgdi32 -lshell32
if errorlevel 1 (del mdv.res.o 2>nul & echo BUILD FAILED & exit /b 1)
del mdv.res.o 2>nul
echo built mdv.exe
