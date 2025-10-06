@echo off
REM ESP32 Orchestra Build Script for Windows
REM สำหรับ build และ flash ESP32 Orchestra ด้วย ESP-IDF

setlocal enabledelayedexpansion

REM Colors (limited in Windows CMD)
set "INFO=[INFO]"
set "SUCCESS=[SUCCESS]"
set "WARNING=[WARNING]"
set "ERROR=[ERROR]"

:check_idf
echo %INFO% Checking ESP-IDF installation...
idf.py --version >nul 2>&1
if errorlevel 1 (
    echo %ERROR% ESP-IDF not found! Please install and run export.bat
    echo Run: %IDF_PATH%\export.bat
    exit /b 1
)
echo %SUCCESS% ESP-IDF found

:main
if "%~1"=="" goto help
if "%~1"=="help" goto help
if "%~1"=="build-all" goto build_all
if "%~1"=="build-conductor" goto build_conductor
if "%~1"=="build-musician" goto build_musician
if "%~1"=="flash-conductor" goto flash_conductor
if "%~1"=="flash-musician" goto flash_musician
if "%~1"=="monitor" goto monitor
if "%~1"=="clean-all" goto clean_all
if "%~1"=="setup-musicians" goto setup_musicians

echo %ERROR% Unknown command: %~1
echo Run '%~nx0 help' for available commands
exit /b 1

:build_all
echo %INFO% Building all projects...
call :build_project conductor Conductor
call :build_project musician Musician
echo %SUCCESS% All projects built successfully!
goto :eof

:build_conductor
call :build_project conductor Conductor
goto :eof

:build_musician
if not "%~2"=="" (
    call :update_musician_id %2
)
call :build_project musician Musician
goto :eof

:flash_conductor
if "%~2"=="" (
    echo %ERROR% Please specify port: %~nx0 flash-conductor COM3
    exit /b 1
)
call :build_project conductor Conductor
call :flash_project conductor Conductor %2
goto :eof

:flash_musician
if "%~2"=="" (
    echo %ERROR% Usage: %~nx0 flash-musician ^<musician_id^> ^<port^>
    echo Example: %~nx0 flash-musician 0 COM3
    exit /b 1
)
if "%~3"=="" (
    echo %ERROR% Usage: %~nx0 flash-musician ^<musician_id^> ^<port^>
    echo Example: %~nx0 flash-musician 0 COM3
    exit /b 1
)
call :update_musician_id %2
call :build_project musician Musician
call :flash_project musician Musician %3
goto :eof

:monitor
if "%~2"=="" (
    echo %ERROR% Usage: %~nx0 monitor ^<conductor^|musician^> ^<port^>
    echo Example: %~nx0 monitor conductor COM3
    exit /b 1
)
if "%~3"=="" (
    echo %ERROR% Usage: %~nx0 monitor ^<conductor^|musician^> ^<port^>
    echo Example: %~nx0 monitor conductor COM3
    exit /b 1
)
call :monitor_project %2 %2 %3
goto :eof

:clean_all
call :clean_project conductor Conductor
call :clean_project musician Musician
echo %SUCCESS% All projects cleaned!
goto :eof

:setup_musicians
if "%~4"=="" (
    echo %ERROR% Usage: %~nx0 setup-musicians ^<port1^> ^<port2^> ^<port3^> [port4]
    echo Example: %~nx0 setup-musicians COM3 COM4 COM5
    exit /b 1
)

echo %INFO% Setting up multiple musicians...

REM Flash musician 0
call :update_musician_id 0
call :build_project musician Musician
call :flash_project musician Musician %2

REM Flash musician 1
call :update_musician_id 1
call :build_project musician Musician
call :flash_project musician Musician %3

REM Flash musician 2
call :update_musician_id 2
call :build_project musician Musician
call :flash_project musician Musician %4

REM Flash musician 3 if port provided
if not "%~5"=="" (
    call :update_musician_id 3
    call :build_project musician Musician
    call :flash_project musician Musician %5
)

echo %SUCCESS% All musicians set up successfully!
echo %INFO% Now flash the conductor with: %~nx0 flash-conductor ^<port^>
goto :eof

:help
echo ESP32 Orchestra Build Script for Windows
echo.
echo Usage: %~nx0 ^<command^> [arguments]
echo.
echo Commands:
echo   build-all                     - Build conductor and musician
echo   build-conductor              - Build conductor only
echo   build-musician [id]          - Build musician (optionally set ID)
echo   flash-conductor ^<port^>       - Build and flash conductor
echo   flash-musician ^<id^> ^<port^>   - Build and flash musician with specific ID
echo   monitor ^<project^> ^<port^>     - Monitor project logs
echo   clean-all                    - Clean all projects
echo   setup-musicians ^<ports...^>   - Setup multiple musicians with different IDs
echo   help                         - Show this help
echo.
echo Examples:
echo   %~nx0 build-all
echo   %~nx0 flash-conductor COM3
echo   %~nx0 flash-musician 0 COM4
echo   %~nx0 setup-musicians COM3 COM4 COM5
echo   %~nx0 monitor conductor COM3
goto :eof

REM Functions
:build_project
set project_dir=%~1
set project_name=%~2

echo %INFO% Building %project_name%...
cd %project_dir%

REM Set target (only needed once)
idf.py set-target esp32

REM Build
idf.py build
if errorlevel 1 (
    echo %ERROR% Failed to build %project_name%
    cd ..
    exit /b 1
)

echo %SUCCESS% %project_name% built successfully
cd ..
goto :eof

:flash_project
set project_dir=%~1
set project_name=%~2
set port=%~3

echo %INFO% Flashing %project_name% to %port%...
cd %project_dir%

idf.py -p %port% flash
if errorlevel 1 (
    echo %ERROR% Failed to flash %project_name% to %port%
    echo %WARNING% Make sure the device is connected and the port is correct
    cd ..
    exit /b 1
)

echo %SUCCESS% %project_name% flashed successfully to %port%
cd ..
goto :eof

:monitor_project
set project_dir=%~1
set project_name=%~2
set port=%~3

echo %INFO% Starting monitor for %project_name% on %port%...
echo %WARNING% Press Ctrl+] to exit monitor
cd %project_dir%
idf.py -p %port% monitor
cd ..
goto :eof

:clean_project
set project_dir=%~1
set project_name=%~2

echo %INFO% Cleaning %project_name%...
cd %project_dir%
idf.py fullclean
echo %SUCCESS% %project_name% cleaned
cd ..
goto :eof

:update_musician_id
set musician_id=%~1
set file=musician\main\musician_main.c

if not exist "%file%" (
    echo %ERROR% Musician source file not found: %file%
    exit /b 1
)

echo %INFO% Updating MUSICIAN_ID to %musician_id%...

REM Backup original file
copy "%file%" "%file%.backup" >nul

REM Update MUSICIAN_ID using PowerShell (more reliable than batch)
powershell -Command "(Get-Content '%file%') -replace '#define MUSICIAN_ID.*', '#define MUSICIAN_ID %musician_id%  // 0=Part A, 1=Part B, 2=Part C, 3=Part D' | Set-Content '%file%'"

findstr /C:"#define MUSICIAN_ID %musician_id%" "%file%" >nul
if errorlevel 1 (
    echo %ERROR% Failed to update MUSICIAN_ID
    exit /b 1
)

echo %SUCCESS% MUSICIAN_ID updated to %musician_id%
goto :eof