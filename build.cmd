@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cl /nologo /LD /O2 /MT /EHsc /std:c++17 auto_turn.cpp /link /OUT:..\DLMods\guide_path_auto_turn.dll user32.lib
