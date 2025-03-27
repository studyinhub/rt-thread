@echo off
REM Function: FormatTime
REM Format the current time according to the specified format
REM Arguments:
REM   %1 - Time format (e.g., "HH:mm:ss", "yyyy-MM-dd HH:nn:ss")


@REM :GetSystemTime


:FormatTime
setlocal enabledelayedexpansion

REM Read the time format argument
set "FORMAT=%~1"
@REM if not defined FORMAT exit /b 1
if "%FORMAT%"=="" set "FORMAT=yyyy-MM-dd HH:nn:ss"

@REM echo FORMAT:%FORMAT%
@REM for /f "tokens=1-7 delims=," %%a in ('%SystemRoot%\System32\wbem\wmic.exe PATH Win32_LocalTime GET Day^,Hour^,Minute^,Month^,Second^,Year /Format:csv') do set "DT=%%g-%%e-%%b %%c:%%d:%%f" exit /b

@REM echo DT:%DT%

REM Get the current date and time
for /f "tokens=1-6 delims=/:., " %%a in ('echo %date% %time:') do (
  set "DT=%%a-%%b-%%c %%d:%%e:%%f"
)
for /f "delims=" %%a in ('powershell Get-Date -Format "yyyy-MM-dd HH:mm:ss"') do set "DT=%%a"

for /f "tokens=2-4 delims=/ " %%A in ('echo %date%') do (
    set "CURRENT_DATE=%%C-%%A-%%B"
)
for /f "tokens=1-3 delims=:." %%A in ('echo %time%') do (
    set "CURRENT_TIME=%%A:%%B:%%C"
    set "HOUR=%%A"
    set "MINUTE=%%B"
    set "SEC=%%C"
)

@REM echo CURRENT_DATE:%CURRENT_DATE%
@REM echo CURRENT_TIME:%CURRENT_TIME%

set "YEAR=%CURRENT_DATE:~0,4%"
set "MONTH=%CURRENT_DATE:~5,2%"
set "DAY=%CURRENT_DATE:~8,2%"

@REM echo YEAR:%YEAR%
@REM echo MONTH:%MONTH%
@REM echo DAY:%DAY%

if "%HOUR:~0,1%"==" " set "HOUR=0%HOUR:~1%"

@REM echo HOUR:%HOUR%
@REM echo MINUTE:%MINUTE%
@REM echo SEC:%SEC%

set "HOUR=%CURRENT_TIME:~0,2%"
set "MINUTE=%CURRENT_TIME:~3,2%"
set "SEC=%CURRENT_TIME:~6,2%"


REM Replace the format specifiers with the corresponding values
set "FORMATTED_TIME=!FORMAT!"
set "FORMATTED_TIME=!FORMATTED_TIME:yyyy=%YEAR%!"
set "FORMATTED_TIME=!FORMATTED_TIME:yy=%YEAR:~-2%!"
set FORMATTED_TIME=!FORMATTED_TIME:MM=%MONTH%!

set "FORMATTED_TIME=!FORMATTED_TIME:dd=%DAY%!"
set "FORMATTED_TIME=!FORMATTED_TIME:HH=%HOUR%!"
set "FORMATTED_TIME=!FORMATTED_TIME:hh=%HOUR:~1,1%!"
set "FORMATTED_TIME=!FORMATTED_TIME:nn=%MINUTE%!"
set "FORMATTED_TIME=!FORMATTED_TIME:ss=%SEC%!"

@REM set "DT="
@REM call :GetSystemTime
@REM echo DT:%DT%

@REM set "T=%DT%"
@REM set "T=%T: Year=%year%"
@REM set "T=%T: Month=%month%"
@REM set "T=%T: Day=%day%" 
@REM set "T=%T: Hour=%hour%"
@REM set "T=%T: Minute=%minute%"
@REM set "T=%T: Second=%second%"
@REM exit /b


REM Output the formatted time
@REM echo FORMATTED_TIME:%FORMATTED_TIME%

endlocal & set "RET_TIME=%FORMATTED_TIME%"
exit /b




