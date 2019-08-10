@echo off

set INI_FILE=platformio.ini
set SETVAL=unset
set DIR=%CD%
set PORT=default
set UPORT=
set MPORT=
set WEBUI_FORCE=0
set WEBUI_ARGS=
set SILENT=0
set REDIR_ARG=
set REDIR_FILE=build_output.log
set BUILD_ONLY=0
SET ERROR_LEVEL=0


:loop

    echo %1 | findstr /R "^--[^-/].*$ ^[-/][^-/].*$" >NUL
    if not errorlevel 1 (
        echo '%1' | findstr /R /I "'[-/]*help' '[-/]*h'" >NUL
        if not errorlevel 1 (
            goto help
        )
        echo '%1' | findstr /R /I "'[-/]*dir' '[-/]*d'" >NUL
        if not errorlevel 1 (
            call :test_dir %1 %2
            set DIR=%CD%g
        ) else (
            echo '%1' | findstr /R /I "'[-/]*port' '[-/]*p'" >NUL
            if not errorlevel 1 (
                set UPORT=--upload-port %2
                set MPORT=--port %2
                set PORT=%2
            ) else (
                echo '%1' | findstr /R /I "'[-/]*force-webui-build' '[-/]*fw'" >NUL
                if not errorlevel 1 (
                    set WEBUI_FORCE=1
                    set WEBUI_ARGS=--force
                ) else (
                    echo '%1' | findstr /R /I "'[-/]*build-only' '[-/]*bo'" >NUL
                    if not errorlevel 1 (
                        set BUILD_ONLY=1
                    ) else (
                        echo '%1' | findstr /R /I "'[-/]*silent' '[-/]*s'" >NUL
                        if not errorlevel 1 (
                            set SILENT=1
                            set REDIR_ARG=^>^>%REDIR_FILE%
                        ) else (
                            goto invalid_arg
                        )
                    )
                )
            )
        )
    )

shift
if not "%~1"=="" goto loop

cd %DIR%
if NOT errorlevel 0 (
	echo %DIR% is not a directory
	goto help
)

call :line
echo "[BUILD] Configuration"
call :line
echo Workspace Dir: %DIR%
if %BUILD_ONLY% equ 0 (
    echo Upload/Monitor: %PORT%
)
echo Force webui build: %WEBUI_FORCE%
echo Silent: %SILENT%
if %SILENT% equ 1 (
    echo Output log: %REDIR_FILE%
    echo Begin >%REDIR_FILE%
)
call :line
echo "[BUILD] Starting build process..."

if %BUILD_ONLY% equ 1 goto :next1

call :line
if %WEBUI_FORCE% equ 1 (
    echo "[BUILD] Forcing webuid build..."
) else (
    echo "[BUILD] Checking webui for changes..."
)
call :line

set ERROR_MSG=NONE
call :launch php.exe %DIR%\lib\KFCWebFrameWork\bin\include\cli_tool.php %DIR%\KFCWebFrameWork.json -b spiffs -e env:esp12e -v %WEBUI_ARGS%
cd %DIR%
IF %ERROR_LEVEL% NEQ 127 (
    call :line
    echo "[BUILD] Uploading webui SPIFFS image"
    call :line
    SET ERROR_MSG=Failed to upload SPIFFS image
    call :launch platformio.exe run --target uploadfs %UPORT%
) else (
    call :line
    echo "[BUILD] Webui up to date"
)

call :line
echo "[BUILD] Compiling and uploading progam"
SET ERROR_MSG=Failed to compile or upload code
call :launch platformio.exe run --target upload %UPORT%
call :line

goto next2

:next1

call :line
echo "[BUILD] Compiling progam"
SET ERROR_MSG=Failed to compile or upload code
call :launch platformio.exe run
call :line

:next2

for /f "delims=" %%# in ('powershell get-date -format "{dd-MMM-yyyy HH:mm}"') do @set _date=%%#
for /f "tokens=1,2,3,4" %%a in ('C:\Users\sascha\.platformio\packages\toolchain-xtensa\bin\xtensa-lx106-elf-size.exe .\.pioenvs\esp12e\firmware.elf') do @set _elf=%%a %%b %%c %%d
echo %_date% %_elf% >> elf_data.log

"C:\Program Files\Git\usr\bin\tail.exe" -n 1 elf_data.log

if %BUILD_ONLY% equ 1 (
    exit
)

echo "[BUILD] Starting monitoring"
call :line
SET SILENT=0
SET REDIR_ARG=
SET ERROR_MSG=Failed to start monitor on port %PORT%
call :launch platformio device monitor --echo --baud 115200 %MPORT%

exit

:test_dir
    cd %2 >NUL
    if NOT errorlevel 1 (
        if not exist %INI_FILE% (
            echo %1: %CD%\%INI_FILE% not found
            goto help
        )
        exit /b
    ) else (
        echo %1: Directory %2 not found
        goto help
    )

:invalid_arg
    echo Invalid argument: %1
:help
    echo "usage: buildall.bat [--dir|-d|/d <workspace dir>] [--port|-p|/p <upload/monitor port>] [--build-only|-bo|/bo] <--force-webui-build|-fw|/fw> </silent|-s|/s> <--help|-h|/h>"
    exit 2
:line
    echo "[BUILD] --------------------------------------------------------------"
    exit /b
:launch
    echo "[BUILD] Launching: %*"
    if %SILENT% equ 1 (
        echo "[BUILD] Output redirected to: %REDIR_FILE%"
        echo Command: %* %REDIR_ARG%
        echo ---------------------------------------------------------------------------- %REDIR_ARG%
    )
    call :line
    echo %* %REDIR_ARG%
    %* %REDIR_ARG%
    SET /a ERROR_LEVEL=%errorlevel%
    echo "[BUILD] exit code %ERROR_LEVEL%"
    if NOT %ERROR_LEVEL% EQU 0 (
        if "%ERROR_MSG%"=="NONE" (
            exit /b
        )
        call :error Exit code %ERROR_LEVEL%: %ERROR_MSG%
    )
    exit /b

:error
    call :line
    echo "[BUILD] Fatal Error: %*"
    call :line
    echo "Output log: %REDIR_FILE"
    call :line
    rem type %REDIR_FILE%
    pause
    exit 1
