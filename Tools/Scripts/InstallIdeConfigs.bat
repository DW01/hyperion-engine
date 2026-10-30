@echo off
setlocal

REM Copies the editor launch/debug templates from Tools\IDE into .vscode and .zed. Existing files are left alone;
REM delete one to get the template back.
set "HYP_REPO_DIR=%~dp0..\.."
set "HYP_IDE_DIR=%~dp0..\IDE"

call :INSTALL "%HYP_IDE_DIR%\VSCode\launch.Windows.json" .vscode launch.json
call :INSTALL "%HYP_IDE_DIR%\VSCode\tasks.json" .vscode tasks.json
call :INSTALL "%HYP_IDE_DIR%\Zed\debug.Windows.json" .zed debug.json
exit /b 0

:INSTALL
if exist "%HYP_REPO_DIR%\%~2\%~3" exit /b 0
if not exist "%HYP_REPO_DIR%\%~2" mkdir "%HYP_REPO_DIR%\%~2"
copy "%~1" "%HYP_REPO_DIR%\%~2\%~3" >nul && echo Installed %~2\%~3
exit /b 0
