echo off

set DEPENDENCY_ROOT=%~dp0thirdparty


echo ===INSTALLING VCPKG===
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



echo === !! DONE !! ===



pause