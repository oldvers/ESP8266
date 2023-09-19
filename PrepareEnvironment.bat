@echo off
@echo Setting environment for using Espressif IoT Development Framework for ESP8266

set CD_PATH=%~dp0
set CD_PATH=%CD_PATH:~0,-1%

set ROOT=%CD_PATH%\..

set IDF_PATH=%ROOT%\ESP8266IDF\sdk\ESP8266_RTOS_SDK
set IDF_TOOLS_PATH=%ROOT%\ESP8266IDF\tools
set VSC_TOOLS_PATH=%ROOT%\ESP8266IDF\vsc\tools
set MSYS_PATH=%ROOT%\ESP8266IDF\tools\tools\msys64\usr\bin
set PYTHON_PATH=%ROOT%\ESP8266IDF\python
set XTENSA_PATH=%ROOT%\ESP8266IDF\tools\tools\xtensa-lx106-elf\esp-2020r3-49-gd5524c1-8.4.0\xtensa-lx106-elf\bin
set CMAKE_PATH=%ROOT%\ESP8266IDF\tools\tools\cmake\3.13.4\bin
set MCONF_PATH=%ROOT%\ESP8266IDF\tools\tools\mconf\v4.6.0.0-idf-20190628
set NINJA_PATH=%ROOT%\ESP8266IDF\tools\tools\ninja\1.9.0
set IDF_EXE_PATH=%ROOT%\ESP8266IDF\tools\tools\idf-exe\1.0.1
set CCACHE_PATH=%ROOT%\ESP8266IDF\tools\tools\ccache\3.7
set IDF_SDK_TOOLS_PATH=%ROOT%\ESP8266IDF\sdk\ESP8266_RTOS_SDK\tools

set GIT_PATH=D:\Projects\01_Soft\01_Installed\Git_v2.39.2\cmd
set VSCODE_PATH=D:\Projects\01_Soft\01_Installed\MSVSCode\bin

set HOME=%IDF_PATH%\home

set IDF_TARGET=esp8266
set ESPBAUD=921600
set MONITORBAUD=115200

set PATH=%PYTHON_PATH%;%IDF_PATH%;%IDF_TOOLS_PATH%;%GIT_PATH%;%VSCODE_PATH%;%VSC_TOOLS_PATH%;%MSYS_PATH%;%PYTHON_PATH%\Scripts;%XTENSA_PATH%;%CMAKE_PATH%;%MCONF_PATH%;%NINJA_PATH%;%IDF_EXE_PATH%;%CCACHE_PATH%;%IDF_SDK_TOOLS_PATH%

rem python -m pip install --user -r %IDF_PATH%\requirements.txt
