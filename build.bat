@echo off
setlocal

REM Check if cl.exe is available
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: cl.exe not found. Please run this script from a Developer Command Prompt for VS.
    exit /b 1
)

if not exist build\Release mkdir build\Release

echo Compiling resources...
rc.exe /v /fo resources\app.res resources\app.rc
if %errorlevel% neq 0 (
    echo Resource compilation failed.
    exit /b %errorlevel%
)

echo Compiling C++ Application...
cl.exe /nologo /O2 /EHsc /std:c++latest /D "NDEBUG" /D "_WINDOWS" /W3 /Fe"build\Release\MicMute-S.exe" ^
    src\main.cpp src\audio.cpp src\globals.cpp src\settings.cpp src\tray.cpp ^
    src\overlay.cpp src\ui.cpp src\recorder.cpp src\WasapiRecorder.cpp ^
    src\StreamingWavWriter.cpp src\ui_controls.cpp src\call_recorder.cpp ^
    src\http_server.cpp ^
    resources\app.res ^
    user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib Mmdevapi.lib advapi32.lib dwmapi.lib ws2_32.lib Winhttp.lib

if %errorlevel% neq 0 (
    echo Compilation failed.
    exit /b %errorlevel%
)

echo Build successful! Output: build\Release\MicMute-S.exe
endlocal
