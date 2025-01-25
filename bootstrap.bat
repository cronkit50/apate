echo off

echo ===UNPACKING DPP DEPENDENCIES===

set DEPENDENCY_ROOT=%~dp0thirdparty
set DPP_ROOT=%DEPENDENCY_ROOT%\dpp\

rem CURRENT DPP LIBRARY IN USE - UPDATE WITH DPP AS NEEDED
set DPP_10_0_3_5_WIN64_DEBUG=libdpp-10.0.35-win64-debug-vs2022.zip
set DPP_10_0_3_5_WIN64_RELEASE=libdpp-10.0.35-win64-release-vs2022.zip

set DPP_WIN64_DEBUG_OUT_DIR=%DPP_ROOT%win64\debug\
set DPP_WIN64_RELEASE_OUT_DIR=%DPP_ROOT%win64\release\

rem DPP TARGET FILE POINTERS
set DPP_CURRENT_DEBUG_ZIP=%DPP_ROOT%%DPP_10_0_3_5_WIN64_DEBUG%
set DPP_CURRENT_RELEASE_ZIP=%DPP_ROOT%%DPP_10_0_3_5_WIN64_RELEASE%

if not exist %DPP_WIN64_DEBUG_OUT_DIR%\NUL (mkdir %DPP_WIN64_DEBUG_OUT_DIR%)
if not exist %DPP_WIN64_RELEASE_OUT_DIR%)\NUL (mkdir %DPP_WIN64_RELEASE_OUT_DIR%)

rem unzip the files from dpp's releases
echo Unpacking %DPP_CURRENT_DEBUG_ZIP% to %DPP_WIN64_DEBUG_OUT_DIR%...
powershell.exe -nologo -noprofile -command "& { $shell = New-Object -COM Shell.Application; $target = $shell.NameSpace('%DPP_WIN64_DEBUG_OUT_DIR%'); $zip = $shell.NameSpace('%DPP_CURRENT_DEBUG_ZIP%'); $target.CopyHere($zip.Items(), 16); }"

echo Unpacking %DPP_CURRENT_RELEASE_ZIP% to %DPP_WIN64_RELEASE_OUT_DIR%...
powershell.exe -nologo -noprofile -command "& { $shell = New-Object -COM Shell.Application; $target = $shell.NameSpace('%DPP_WIN64_RELEASE_OUT_DIR%'); $zip = $shell.NameSpace('%DPP_CURRENT_RELEASE_ZIP%'); $target.CopyHere($zip.Items(), 16); }"

pause