@echo off
@echo Setting environment for using Espressif IoT Development Framework for ESP8266

set IDF_PATH=P:\Work\10_ESP8266\ESP8266IDF\sdk\ESP8266_RTOS_SDK
set IDF_TOOLS_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools
set GIT_PATH=C:\Work\Git\cmd
set VSCODE_PATH=C:\Work\MSVSCode\bin
set IDF_PYTHON_ENV_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\python_env\rtos3.4_py3.7_env
set XTENSA_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\xtensa-lx106-elf\esp-2020r3-49-gd5524c1-8.4.0\xtensa-lx106-elf\bin
set CMAKE_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\cmake\3.13.4\bin
set MCONF_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\mconf\v4.6.0.0-idf-20190628
set NINJA_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\ninja\1.9.0
set IDF_EXE_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\idf-exe\1.0.1
set CCACHE_PATH=P:\Work\10_ESP8266\ESP8266IDF\tools\tools\ccache\3.7
set IDF_SDK_TOOLS_PATH=P:\Work\10_ESP8266\ESP8266IDF\sdk\ESP8266_RTOS_SDK\tools

set CD_PATH=%~dp0
set CD_PATH=%CD_PATH:~0,-1%

set PATH=%IDF_PATH%;%IDF_TOOLS_PATH%;%GIT_PATH%;%VSCODE_PATH%;%IDF_PYTHON_ENV_PATH%\Scripts;%XTENSA_PATH%;%CMAKE_PATH%;%MCONF_PATH%;%NINJA_PATH%;%IDF_EXE_PATH%;%CCACHE_PATH%;%IDF_SDK_TOOLS_PATH%;%CD_PATH%
