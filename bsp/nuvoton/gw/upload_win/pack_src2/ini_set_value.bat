@echo off
setlocal enabledelayedexpansion

REM Function: UpdateIniValue
REM Update the value of a specified key in an INI file
REM Arguments:
REM   %1 - INI file path
REM   %2 - Section name
REM   %3 - Variable name
REM   %4 - New value
:UpdateIniValue
setlocal enabledelayedexpansion

REM Read INI file path
set "INI_FILE=%~1"

REM Read section name, variable name, and new value
set "SECTION=%~2"
set "VARIABLE=%~3"
set "NEW_VALUE=%~4"

REM Create a temporary file for writing the updated INI content
set "TMP_FILE=%TEMP%\tmp.ini"

REM Initialize a flag to indicate if the variable was updated
set "UPDATED=false"

REM Use FOR loop to read the INI file line by line and update the value
(for /f "usebackq delims=" %%A in ("%INI_FILE%") do (
    REM Parse the content of each line
    set "LINE=%%A"
    REM Remove leading spaces and tabs
    set "LINE=!LINE: =!"
    set "LINE=!LINE:	=!"
    REM Check if it is a section name
    if "!LINE:~0,1!"=="[" (
        REM Check if it is the target section
        if /i "!LINE!"=="[%SECTION%]" (
            REM Set inside the target section
            set "IN_SECTION=true"
            REM Write the line as it is
            echo !LINE! >> "%TMP_FILE%"
        ) else (
            REM Set outside the target section
            set "IN_SECTION=false"
            REM Write the line as it is, including the square brackets
            echo !LINE! >> "%TMP_FILE%"
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
                    REM Update the variable value
                    echo !VAR_NAME!=!NEW_VALUE! >> "%TMP_FILE%"
                    REM Set the flag to indicate the variable was updated
                    set "UPDATED=true"
                ) else (
                    REM Write the line as it is
                    echo !LINE! >> "%TMP_FILE%"
                )
            )
        ) else (
            REM Write the line as it is
            echo !LINE! >> "%TMP_FILE%"
        )
    )
))

REM Replace the original INI file with the updated content
move /y "%TMP_FILE%" "%INI_FILE%" >nul

REM Display a message if the variable was updated or not
@REM if "%UPDATED%"=="true" (
@REM     echo Variable %VARIABLE% in section %SECTION% updated successfully.
@REM ) else (
@REM     echo Variable %VARIABLE% in section %SECTION% not found.
@REM )
endlocal & set "RET_VALUE=%UPDATED%"
exit /b