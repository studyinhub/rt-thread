@ECHO off

Title ??????????????? Designed by YangXiYuan V0.1

set com=%~dp0utils.cmd

rem @XCOPY  %~dp0..\webnet %~dp0\webnet /i /S /Y

call %com% :log_d ??????

call %com% :getIP1


SET IP=192.168.1.200
echo IP:%IP%

where curl

curl --version

rem call %com% :getIP2

REM https://www.cnblogs.com/liangblog/p/9835940.html
REM https://www.tutorialspoint.com/batch_script/batch_script_arrays.htm

REM https://newbedev.com/batch-file-read-file-names-from-a-directory-and-store-in-array

rem c://ProgramData/chocolatey/bin/curl.exe

pushd %~dp0
rem powershell.exe -command ^
  "& {Get-ExecutionPolicy}"
popd


rem https://www.cnblogs.com/zeo-to-one/p/11330156.html#%E4%BA%8Ccmd-%E5%91%BD%E4%BB%A4%E8%A1%8C-powershell-%E7%9A%84%E4%BD%BF%E7%94%A8
@rem set the execution mode of the PowerShell

set flag=1
@rem -c ??? -Command
rem powershell -c "Get-ExecutionPolicy" |findstr "Restricted" >nul && set flag=0
if %flag% == 0 ( 
        :: powershell -ExecutionPolicy RemoteSigned  ???????
	rem powershell -c "Set-ExecutionPolicy AllSigned" 
	echo Allowed to use powershell
)


rem powershell -c "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"

rem choco install curl



set "MasterFolder=%~dp0webnet\"
Set LogFile=%~dpn0.txt

If exist "%LogFile%" Del "%LogFile%"

rem https://zhuanlan.zhihu.com/p/51863566

rem https://bbs.csdn.net/topics/360258652?list=1357979
SetLocal enableDelayedExpansion

@for /f "delims=" %%f in ('dir  /b /s /a:a "%MasterFolder%\*.*"') do (
	set /a "idx+=1"
	set "FullPath=%%f"
	
	set "FileName[!idx!]=%%~nxf"
	set "FilePath[!idx!]=%%~dpFf" 
	rem ??????????????¡è??
	set "RelativePath[!idx!]=!FullPath:%MasterFolder%=!
)



rem ??????????
for /L %%i in (1,1,%idx%) do (
	set "RelativePath[%%i]=!RelativePath[%%i]:\=/!"
	call %com% :log_d [%%i] "!FilePath[%%i]! ---> !RelativePath[%%i]!" 
	rem call %com% :log_d [%%i] "!FileName[%%i]!"

	
	rem ?????????????????????????????????????????
	rem route ADD 192.168.1.100 MASK 255.255.255.255 192.168.1.1  METRIC 3 IF 18
	
	rem c://ProgramData/chocolatey/bin/curl.exe -v -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	
	rem c://ProgramData/chocolatey/bin/curl.exe -v -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	
	
	rem curl --interface %IP% -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	curl -T !FilePath[%%i]! tftp://192.168.1.100/!RelativePath[%%i]!
	
	
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
endlocal

rem ???????¡ì?
rem set FileName
	
REM for /F "delims==" %%i in (set Files[) do (
	REM rem call echo %%i
	
	REM call echo x:%%x%%

	REM call echo Files[%%x%%]: %%Files[%x%]%%

	REM set /a "x+=1"

REM )
PAUSE