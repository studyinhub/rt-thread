@echo off
rem Y:\Projects\rtt-projects\rt-thread\bsp\nuvoton\gw\upload_win

set pack_src=Y:\Projects\rtt-projects\rt-thread\bsp\nuvoton\gw\upload_win

@XCOPY  %~dp0webnet %~dp0upload_win\webnet /i /S /Y
%~dp0bat2exe.exe /source:%pack_src% /target:%pack_src% /s /y

@rd/s/q %~dp0upload_win\webnet


Y:\Projects\airsys-netgates\OneKeyBurnTool\tools\mc.exe cp %~dp0upload_win\tftpweb.exe aliyun/tftpweb

