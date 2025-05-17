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

echo ===COPYING DPP RELEASE===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\dpp*" "%DEPENDENCY_ROOT%\dpp\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\libcrypto*" "%DEPENDENCY_ROOT%\dpp\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\zlib*" "%DEPENDENCY_ROOT%\dpp\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\opus*" "%DEPENDENCY_ROOT%\dpp\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\libssl*" "%DEPENDENCY_ROOT%\dpp\win64\bin\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\dpp*" "%DEPENDENCY_ROOT%\dpp\win64\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\libcrypto*" "%DEPENDENCY_ROOT%\dpp\win64\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\zlib*" "%DEPENDENCY_ROOT%\dpp\win64\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\opus*" "%DEPENDENCY_ROOT%\dpp\win64\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\libssl*" "%DEPENDENCY_ROOT%\dpp\win64\lib\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\include\dpp\" "%DEPENDENCY_ROOT%\dpp\include\dpp\"

echo ===COPYING DPP DEBUG===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\dpp*" "%DEPENDENCY_ROOT%\dpp\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\libcrypto*" "%DEPENDENCY_ROOT%\dpp\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\zlib*" "%DEPENDENCY_ROOT%\dpp\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\opus*" "%DEPENDENCY_ROOT%\dpp\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\libssl*" "%DEPENDENCY_ROOT%\dpp\win64\debug\bin\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\dpp*" "%DEPENDENCY_ROOT%\dpp\win64\debug\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\libcrypto*" "%DEPENDENCY_ROOT%\dpp\win64\debug\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\zlib*" "%DEPENDENCY_ROOT%\dpp\win64\debug\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\opus*" "%DEPENDENCY_ROOT%\dpp\win64\debug\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\libssl*" "%DEPENDENCY_ROOT%\dpp\win64\debug\lib\"

echo ===COPYING NLOHMANN JSON===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\include\nlohmann\" "%DEPENDENCY_ROOT%\nlohmann\include\json\"


echo ===COPYING LIBCURL RELEASE===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\libcurl*" "%DEPENDENCY_ROOT%\curl\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\zlib*" "%DEPENDENCY_ROOT%\curl\win64\bin\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\zlib*" "%DEPENDENCY_ROOT%\curl\win64\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\libcurl*" "%DEPENDENCY_ROOT%\curl\win64\lib\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\include\curl\" "%DEPENDENCY_ROOT%\curl\include\curl\"

echo ===COPYING LIBCURL DEBUG===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\libcurl*" "%DEPENDENCY_ROOT%\curl\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\zlib*" "%DEPENDENCY_ROOT%\curl\win64\debug\bin\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\zlib*" "%DEPENDENCY_ROOT%\curl\win64\debug\lib\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\libcurl*" "%DEPENDENCY_ROOT%\curl\win64\debug\lib\"

echo ===COPYING SQLITE3 RELEASE===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\bin\sqlite*" "%DEPENDENCY_ROOT%\sqlite3\win64\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\lib\sqlite*" "%DEPENDENCY_ROOT%\sqlite3\win64\lib\"

xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\include\sqlite*" "%DEPENDENCY_ROOT%\sqlite3\include\sqlite3\"

echo ===COPYING SQLITE3 DEBUG===
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\bin\sqlite*" "%DEPENDENCY_ROOT%\sqlite3\win64\debug\bin\"
xcopy /s "%VCPKG_INSTALLED_DIR%\x64-windows\debug\lib\sqlite*" "%DEPENDENCY_ROOT%\sqlite3\win64\debug\lib\"
pause