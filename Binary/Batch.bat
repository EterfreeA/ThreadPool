@echo off & setlocal EnableDelayedExpansion

set _number=0
for %%i in (%*) do (set /a _number+=1)

if %_number% geq 2 (
	set _exeFile=%1
	set _outFile=%2
) else if %_number% equ 1 (
	set _exeFile=%1
	set /p _outFile=����ļ���
) else (
	set /p _exeFile=�����ļ���
	set /p _outFile=����ļ���
)

set _current=%~dp0
set _exePath=%_current%%_exeFile%
set _outPath=%_current%%_outFile%

echo %_exePath%
echo %_outPath%

for /l %%i in (1,1,10) do (
	"%_exePath%" 1>>"%_outPath%"
	echo.>>"%_outPath%"
)
