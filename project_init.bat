@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

REM =========================================================
REM 1) 工程名 = 当前目录名
REM =========================================================
for %%i in (.) do set "CUR_DIR=%%~nxi"

REM =========================================================
REM 2) 找到第一个 .uvprojx / .uvoptx（提速：不做全目录循环）
REM =========================================================
set "UVPROJX="
for /f "delims=" %%f in ('dir /b /a:-d "*.uvprojx" 2^>nul') do (
    set "UVPROJX=%%f"
    goto :found_uvprojx
)
:found_uvprojx

set "UVOPTX="
for /f "delims=" %%f in ('dir /b /a:-d "*.uvoptx" 2^>nul') do (
    set "UVOPTX=%%f"
    goto :found_uvoptx
)
:found_uvoptx

REM =========================================================
REM 2.5) 判断是否为 Keil5 工程根目录
REM     必须同时存在 .uvprojx 和 .uvoptx
REM =========================================================
if not defined UVPROJX exit /b 0
if not defined UVOPTX exit /b 0

REM =========================================================
REM 3) 重命名工程文件（如果存在且不同名）
REM =========================================================
if defined UVPROJX (
    if /I not "%UVPROJX%"=="%CUR_DIR%.uvprojx" (
        ren "%UVPROJX%" "%CUR_DIR%.uvprojx"
    )
)

if defined UVOPTX (
    if /I not "%UVOPTX%"=="%CUR_DIR%.uvoptx" (
        ren "%UVOPTX%" "%CUR_DIR%.uvoptx"
    )
)

REM =========================================================
REM 4) 删除 Keil 缓存文件（Keil 会自动重建）
REM =========================================================
del /q *.uvguix.* >nul 2>nul
del /q EventRecorderStub.scvd >nul 2>nul

REM =========================================================
REM 5) 修改 .uvprojx 内 <OutputName>...</OutputName>
REM =========================================================
if exist "%CUR_DIR%.uvprojx" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$p = '%CUR_DIR%.uvprojx';" ^
      "$name = '%CUR_DIR%';" ^
      "$enc = New-Object System.Text.UTF8Encoding($false);" ^
      "$c = [System.IO.File]::ReadAllText($p, $enc);" ^
      "if($c -match ('<OutputName>' + [Regex]::Escape($name) + '</OutputName>')) { exit 0 }" ^
      "$c2 = [Regex]::Replace($c, '<OutputName>[^<]*</OutputName>', '<OutputName>' + $name + '</OutputName>');" ^
      "[System.IO.File]::WriteAllText($p, $c2, $enc);"
)

REM =========================================================
REM 6) 打开工程
REM =========================================================
if exist "%CUR_DIR%.uvprojx" (
    start "" "%CUR_DIR%.uvprojx"
)