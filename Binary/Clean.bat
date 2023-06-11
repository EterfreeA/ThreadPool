@echo off

set _number=0
for %%i in (%*) do (set /a _number+=1)

if %_number% geq 1 (
	set _file=%1
) else (
	set /p _file=ÎÄ¼ş£º
)

set _current=%~dp0
set _path=%_current%%_file%
echo %_path%

if exist "%_path%" (del "%_path%")
