@echo off
if defined MSYSTEM (
    echo This .bat file is for Windows CMD.EXE shell only.
    goto :__end
)

rem Setup the current directory
set CD_PATH=%~dp0
set CD_PATH=%CD_PATH:~0,-1%

rem Setup the files used
set PATHS_WIN_TOOLS_FILE=paths-win-tools.bat
set PATHS_IDF_TOOLS_FILE=paths-esp-idf-tools.bat
set PARAMETERS_IDF_TOOLS_FILE=parameters.bat

rem Setup the environment variables in the correct order
rem Setup the Windows tools paths
call %PATHS_WIN_TOOLS_FILE%

rem Setup the ESP IDF paths
set IDF_PATH=%IDF_ROOT_PATH%\ESP8266_RTOS_SDK
set IDF_TOOLS_PATH=%IDF_ROOT_PATH%

rem Setup the PATH environment variable
set PATH=%GIT_PATH%;%VSCODE_PATH%

rem Setup the ESP IDF tools paths
call %PATHS_IDF_TOOLS_FILE%

rem Setup the ESP IDF additional parameters
call %PARAMETERS_IDF_TOOLS_FILE%

rem Switch to the project's folder
pushd .
cd ..

rem Call the VS Code
start /b code .

rem Restore the folder
popd
