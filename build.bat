@echo off
REM RoverEngine Build Script Wrapper for Windows

where python >nul 2>nul
if %ERRORLEVEL% equ 0 (
    python "%~dp0misc\scripts\build.py" %*
    exit /B %ERRORLEVEL%
)

where python3 >nul 2>nul
if %ERRORLEVEL% equ 0 (
    python3 "%~dp0misc\scripts\build.py" %*
    exit /B %ERRORLEVEL%
)

where py >nul 2>nul
if %ERRORLEVEL% equ 0 (
    py -3 "%~dp0misc\scripts\build.py" %*
    exit /B %ERRORLEVEL%
)

echo Error: Neither 'python', 'python3', nor 'py' found in PATH. >&2
exit /B 1
