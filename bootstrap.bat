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

echo ===BUILDING LIBCURL...===
set VCPKG_DIR=%CD%\vcpkg
set VCPKG_INSTALLED_DIR=%CD%\VCPKG_INSTALLED\
if not exist "%VCPKG_DIR%" (
    echo vcpkg doesn't existing... Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg.git %VCPKG_DIR%
    pushd %VCPKG_DIR%
    call bootstrap-vcpkg.bat
    popd
)

%VCPKG_DIR%\vcpkg.exe install

xcopy /s %VCPKG_INSTALLED_DIR%\x64-windows\bin\ %DEPENDENCY_ROOT%\curl\win64\bin\
xcopy /s %VCPKG_INSTALLED_DIR%\x64-windows\lib\ %DEPENDENCY_ROOT%\curl\win64\lib\

pause