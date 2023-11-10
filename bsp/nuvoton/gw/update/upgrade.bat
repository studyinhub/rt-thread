::[Bat To Exe Converter]
::
::fBE1pAF6MU+EWGDeyHIiJxFRTxCRAHmuEvgM+ufx6umTsXEPQK8rcJ3e27CyIvMD1kvxY5k52XRmn9wwGQtcfwauUh0xug4=
::YAwzoRdxOk+EWAjk
::fBw5plQjdDaDJGmW+0g1Kw9HcBaWKCaqFLoW7evv/NaVtgAJXPA6eYvn2KeeHO4G/ErwepgR0W1mjdsIDQ9dQgCufTMgr3pSoWGHec6fvG8=
::YAwzuBVtJxjWCl3EqQJgSA==
::ZR4luwNxJguZRRnVpBBoSA==
::Yhs/ulQjdF25
::cxAkpRVqdFKZSDk=
::cBs/ulQjdFy5
::ZR41oxFsdFKZSDk=
::eBoioBt6dFKZSDk=
::cRo6pxp7LAbNWATEpCI=
::egkzugNsPRvcWATEpCI=
::dAsiuh18IRvcCxnZtBJQ
::cRYluBh/LU+EWAnk
::YxY4rhs+aU+JeA==
::cxY6rQJ7JhzQF1fEqQJQ
::ZQ05rAF9IBncCkqN+0xwdVs0
::ZQ05rAF9IAHYFVzEqQJQ
::eg0/rx1wNQPfEVWB+kM9LVsJDGQ=
::fBEirQZwNQPfEVWB+kM9LVsJDGQ=
::cRolqwZ3JBvQF1fEqQJQ
::dhA7uBVwLU+EWDk=
::YQ03rBFzNR3SWATElA==
::dhAmsQZ3MwfNWATElA==
::ZQ0/vhVqMQ3MEVWAtB9wSA==
::Zg8zqx1/OA3MEVWAtB9wSA==
::dhA7pRFwIByZRRnk
::Zh4grVQjdDaDJGmW+0g1Kw9HcBaWKCaqFLoW7evv/NaVtgAJXPA6eYvn2KeeHO4G/ErwepgR0W1mjdsIDQ9dQgateh8jrGx95DTXZJbN41mvT1CMhg==
::YB416Ek+ZG8=
::
::
::978f952a14a936cc963da21a135fa983
@ECHO off
chcp 65001 > nul
SetLocal enableDelayedExpansion

Title Upgrade PT980 Web files. V0.2.2023.11.03

rem where curl

rem curl --version

rem call %com% :getIP2

REM https://www.cnblogs.com/liangblog/p/9835940.html
REM https://www.tutorialspoint.com/batch_script/batch_script_arrays.htm

REM https://newbedev.com/batch-file-read-file-names-from-a-directory-and-store-in-array

rem c://ProgramData/chocolatey/bin/curl.exe

rem  pushd %~dp0
rem  powershell.exe -command ^
rem "& {Get-ExecutionPolicy}"
rem  popd


rem https://www.cnblogs.com/zeo-to-one/p/11330156.html#%E4%BA%8Ccmd-%E5%91%BD%E4%BB%A4%E8%A1%8C-powershell-%E7%9A%84%E4%BD%BF%E7%94%A8
@rem set the execution mode of the PowerShell

set flag=1
@rem -c hello -Command
rem powershell -c "Get-ExecutionPolicy" |findstr "Restricted" >nul && set flag=0
if %flag% == 0 ( 
        :: powershell -ExecutionPolicy RemoteSigned  ???????
	rem powershell -c "Set-ExecutionPolicy AllSigned" 
	echo Allowed to use powershell
)


rem powershell -c "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"

rem choco install curl

echo Inited.....

if "%~nx0"=="upgrade.bat" (
    @REM echo cd:%cd%
    @REM echo ~dp0:%~dp0
    @REM echo ~nx0:%~nx0
    @REM echo ~f0:%~f0
    @REM echo APPDATA:%APPDATA%
    @REM echo TEMP:%TEMP%
    echo.
    @REM echo running in bat
    set RES_PATH=%cd%\res
    SET BAT_NAME=%~dpn0

) else (
    @REM echo running in exe
    set RES_PATH=%APPDATA%\res
)

@REM echo RES_PATH:%RES_PATH%
set WEBNET_PATH=%RES_PATH%\webnet
@REM echo WEBNET_PATH:%WEBNET_PATH%

Set LogFile=%BAT_NAME%.txt
@REM echo LogFile:%LogFile%

set UPLOAD_TXT_PATH=%RES_PATH%\upload.txt


set com=%RES_PATH%\utils.cmd
set ini_get=%RES_PATH%\ini_get_value.bat
set ini_set=%RES_PATH%\ini_set_value.bat
set ftime=%RES_PATH%\time.bat

@REM echo read_ini_value:%read_ini_value%


REM 读取INI文件 test.ini 中的变量 MyVariable 的�?
call %ini_get%  "%RES_PATH%\test.ini" "CONFIG" "IP"
set "IP=%RET_VALUE%"
@REM SET IP=192.168.2.200
echo Default IP:%IP%

@REM call %ftime% "yyyy-MM-dd HH:nn:ss"

@REM call %ini_set% "%RES_PATH%\test.ini" "CONFIG" "IP" "%RET_TIME%"



rem @XCOPY  %~dp0..\webnet %~dp0\webnet /i /S /Y

call %com% :log_d PT-980 Web

@REM call %com% :getIP1

If exist "%LogFile%" Del "%LogFile%"

rem https://zhuanlan.zhihu.com/p/51863566

rem https://bbs.csdn.net/topics/360258652?list=1357979


:MENU
@REM cls
echo 1) Scan device
echo 2) Manual input device ip
echo 3) Exit

set /p CHOICE=Enter your choice: 

if "%CHOICE%"=="1" goto SYNC_FILES
if "%CHOICE%"=="2" goto MANUAL_INPUT
if "%CHOICE%"=="3" goto EXIT
goto MENU

:SCAN_DEVICE
echo Begin to search device...


:MANUAL_INPUT
set /p MANUAL_IP=Enter the IP address manually: 

REM 校验 IP 地址格式
echo You input:!MANUAL_IP!
echo !MANUAL_IP! | findstr /r "\<[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\>" >nul

if errorlevel 1 (
    echo %MANUAL_IP%  is not valid,please check.
    timeout /t 2 >nul
    goto MANUAL_INPUT
) else (
    echo %MANUAL_IP%  is valid.Begin ping %MANUAL_IP%
    
    ping %MANUAL_IP% -n 1 -w 10 | find "TTL=" > nul

    if errorlevel 1 (
        echo Not found device at %MANUAL_IP%
        goto MANUAL_INPUT
    ) else (
        echo Find device at %MANUAL_IP%
        timeout /t 2 >nul
        set IP=%MANUAL_IP%
        goto UPDATE_IP
    )

)

:UPDATE_IP
echo Begin to upgrade ip address with %MANUAL_IP% in %UPLOAD_TXT_PATH%...
echo.
call %ini_set% "%RES_PATH%\test.ini" "CONFIG" "IP" "%MANUAL_IP%"


:SYNC_FILES
call %com% :log_d 设备 IP:%IP%

@for /f "delims=" %%f in ('dir  /b /s /a:-d "%WEBNET_PATH%\*.*"') do (
	set /a "idx+=1"
	set "FullPath=%%f"
	set "FileName[!idx!]=%%~nxf"
	set "FilePath[!idx!]=%%~dpFf" 
	set "RelativePath[!idx!]=!FullPath:%WEBNET_PATH%=!
)

for /L %%i in (1,1,%idx%) do (
	set "RelativePath[%%i]=!RelativePath[%%i]:\=/!"
	call %com% :log_d [%%i] "!FilePath[%%i]! ---> !RelativePath[%%i]!" 

	rem call %com% :log_d [%%i] "!FileName[%%i]!"

	rem route ADD 192.168.1.100 MASK 255.255.255.255 192.168.1.1  METRIC 3 IF 18
	
	rem c://ProgramData/chocolatey/bin/curl.exe -v -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	
	rem c://ProgramData/chocolatey/bin/curl.exe -v -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	
	
	rem curl --interface %IP% -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	curl -T !FilePath[%%i]! tftp://%IP%/!RelativePath[%%i]!
	
	
    REM ( 
        REM echo( [%%i] "!FileName[%%i]!"
        REM echo Path : "!FilePath[%%i]!"
        REM echo ************************************
    REM )>> "%LogFile%"
)

rem curl -T \\Mac\Home\Desktop\upload_win\webnet\admin\index.html tftp://192.168.1.100/admin/index.html

ECHO(
ECHO Total text files(s) : !idx!
TimeOut /T 1 /nobreak>nul

REM Start "" "%LogFile%"



rem set FileName
	
REM for /F "delims==" %%i in (set Files[) do (
	REM rem call echo %%i
	
	REM call echo x:%%x%%

	REM call echo Files[%%x%%]: %%Files[%x%]%%

	REM set /a "x+=1"

)
goto MENU
:EXIT
echo Exiting...
exit

endlocal
