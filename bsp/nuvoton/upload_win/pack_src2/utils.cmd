@echo off
call %*
goto :EOF

:getIP1
for /f "tokens=4" %%a in ('route print^|findstr 0.0.0.0.*0.0.0.0') do (
 set IP=%%a
)

:getIP2
for /f "tokens=15" %%i in ('ipconfig ^| find /i "ip address"') do (echo %%i)
rem pause>nul

:log_d
echo [94m%1 %2[0m


rem https://social.technet.microsoft.com/Forums/lync/en-US/628d3f9f-0ab8-44a6-9184-a631106a64ba/how-to-include-and-call-a-function-defined-in-other-batch-file?forum=ITCG
rem https://gist.githubusercontent.com/mlocati/fdabcaeb8071d5c75a2d51712db24011/raw/b710612d6320df7e146508094e84b92b34c77d48/win10colors.cmd