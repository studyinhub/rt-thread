@echo off

REM Function: ReadIniValue
REM Read the value of a specified variable from an INI file
REM Arguments:
REM   %1 - INI file path
REM   %2 - Section name
REM   %3 - Variable name
REM Return:
REM   Set the variable RET_VALUE with the read INI value
:ReadIniValue
setlocal enabledelayedexpansion

REM Read INI file path
set "INI_FILE=%~1"

REM Read section name and variable name
set "SECTION=%~2"
set "VARIABLE=%~3"

REM Initialize variable value
set "RET_VALUE="

REM Use FOR loop to read the INI file line by line
for /f "usebackq delims=" %%A in ("%INI_FILE%") do (
    REM Parse the content of each line
    set "LINE=%%A"
    REM Remove leading spaces and tabs
    set "LINE=!LINE: =!"
    set "LINE=!LINE:	=!"
    REM Check if it is a section name
    if "!LINE:~0,1!"=="[" (
        REM Remove the square brackets
        set "LINE=!LINE:~1,-1!"
        REM Check if it is the target section
        if /i "!LINE!"=="%SECTION%" (
            REM Set inside the target section
            set "IN_SECTION=true"
        ) else (
            REM Set outside the target section
            set "IN_SECTION=false"
        )
    ) else (
        REM Check if it is inside the target section
        if "!IN_SECTION!"=="true" (
            REM Check if it is the target variable
            for /f "tokens=1,* delims==" %%B in ("!LINE!") do (
                set "VAR_NAME=%%B"
                set "VAR_VALUE=%%C"
                REM Remove quotes from variable name and value
                set "VAR_NAME=!VAR_NAME:"=!"
                set "VAR_VALUE=!VAR_VALUE:"=!"
                REM Check if it is the target variable
                if /i "!VAR_NAME!"=="%VARIABLE%" (
                    REM Set the variable value
                    set "RET_VALUE=!VAR_VALUE!"
                )
            )
        )
    )
)

REM Return the variable value
endlocal & set "RET_VALUE=%RET_VALUE%"
exit /b

