@echo off
set MO_LANE=C:\UEProjects\MO57_Lane
set DOTNET_ROOT=D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64
set DOTNET_MULTILEVEL_LOOKUP=0
set PATH=%DOTNET_ROOT%;%PATH%
rem MSVC 14.44 from DARKSIDE, a private AutoSDK folder: DARKTOWER's Visual Studio has only 14.33 / 14.34 (5.8 needs 14.38+); nothing installed, this process only.
set UE_SDKS_ROOT=C:\Users\Wes\mo57_lane\sdks
cd /d %MO_LANE%
echo [lane] %DATE% %TIME% build starts
git log --oneline -1
rem Below normal priority: DARKTOWER may be rendering (the promo's Blender tasks): they keep the machine first.
start "" /b /wait /belownormal python -u %MO_LANE%\Tools\ue.py build
set BUILD_EXIT=%ERRORLEVEL%
echo [lane] %DATE% %TIME% BUILD_EXIT=%BUILD_EXIT%
if not "%BUILD_EXIT%"=="0" exit /b %BUILD_EXIT%
start "" /b /wait /belownormal python -u %MO_LANE%\Tools\ue.py auto --filter MOFramework
set AUTO_EXIT=%ERRORLEVEL%
echo [lane] %DATE% %TIME% AUTO_EXIT=%AUTO_EXIT%
exit /b %AUTO_EXIT%
