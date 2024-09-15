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







rem set PATH=%PYTHON_PATH%;%PYTHON_PATH%\Scripts;%IDF_PATH%;%GIT_PATH%;%VSCODE_PATH%


rem echo Installing ESP-IDF tools
rem python %IDF_PATH%\tools\idf_tools.py export


rem set IDF_TOOLS_PATHS_EXPORTS_FILE=paths-esp-idf-tools.bat
rem 
rem python.exe "%IDF_PATH%\tools\idf_tools.py" export >"%IDF_TOOLS_PATHS_EXPORTS_FILE%"
rem 
rem set PATH=%IDF_PATH%;%GIT_PATH%;%VSCODE_PATH%
rem call %IDF_TOOLS_PATHS_EXPORTS_FILE%
rem 
rem 
rem echo %PATH%
rem 
rem rem if %errorlevel% neq 0 (
rem     echo ERROR! Something wrong with the tools installation...
rem 	echo Delete all the folders and restart the installation.
rem 	goto __end
rem )

rem echo All done!
rem You can now run:
rem echo    export.bat
rem goto :__end

rem :__error_missing_requirements
rem     echo.
rem     echo Error^: The following tools are not installed in your environment.
rem     echo.
rem     echo %MISSING_REQUIREMENTS%
rem     echo.
rem     echo Please use the Windows Tool installer for setting up your environment.
rem rem    echo Download link: https://dl.espressif.com/dl/esp-idf/
rem rem    echo For more details please visit our website: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html
rem     goto :__end
rem 
rem :__help
rem     python.exe "%IDF_PATH%\tools\install_util.py" print_help bat
rem     goto :__end
rem 
rem :__end
rem exit /b %SCRIPT_EXIT_CODE%
