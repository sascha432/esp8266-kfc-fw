@echo off
setlocal
set "DIR=%~dp0"
if not exist "%DIR%.venv\Scripts\python.exe" (
    echo error: virtual environment not found
    echo.
    echo create it with:
    echo     py -3 -m venv "%DIR%.venv"
    echo     "%DIR%.venv\Scripts\python.exe" -m pip install -r "%DIR%requirements.txt"
    exit /b 1
)
"%DIR%.venv\Scripts\python.exe" "%DIR%audio_analyzer.py" %*
exit /b %ERRORLEVEL%
