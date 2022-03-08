@echo off
rem Y:\Projects\rtt-projects\rt-thread\bsp\nuvoton\gw\upload_win

rem set pack_src=Y:\Projects\rtt-projects\rt-thread\bsp\nuvoton\gw\upload_win

set "pack_src=%~dp0\pack_src"
set "target_dir=%~dp0\dist\"

rem @XCOPY  %~dp0tftpweb.bat %~dp0pack_src\ /i /Y


rem @XCOPY  %~dp0webnet %~dp0\pack_src\webnet /i /S /Y

%~dp0bat2exe.exe /source:%pack_src% /target:%target_dir% /s /y

rem @rd/s/q %~dp0upload_win\webnet


rem Y:\Projects\airsys-netgates\OneKeyBurnTool\tools\mc.exe cp %~dp0upload_win\tftpweb.exe aliyun/tftpweb

