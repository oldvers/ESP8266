@echo off

set MSYSTEM=MSYS_PATH

for /R %%f in (CMakeLists.txt) do @if exist %%f del /F "%%f"

call python %VSC_TOOLS_PATH%\convert_to_cmake.py %CD% --debug

set MSYSTEM=