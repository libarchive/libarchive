@ECHO OFF
IF NOT "%BE%"=="mingw" (
  IF NOT "%BE%"=="msvc" (
    IF NOT "%BE%"=="cygwin-gcc"  (
      ECHO Environment variable BE must be cygwin-gcc, mingw or msvc
      EXIT /b 1
    )
  )
)

SET ORIGPATH=%PATH%
IF "%BE%"=="mingw" (
  SET MINGWPATH=C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;D:\msys64\%MSYSTEM%\bin;C:\Program Files\cmake\bin
)

REM Target architecture for the MSVC build. Defaults to x64 when unset.
IF "%ARCH%"=="" SET "ARCH=x64"
IF /I "%ARCH%"=="x64" GOTO :ARCH_X64
IF /I "%ARCH%"=="arm64" GOTO :ARCH_ARM64
ECHO Environment variable ARCH must be x64 or arm64
EXIT /b 1

:ARCH_X64
SET "CMAKE_PLATFORM=x64"
SET "VCPKG_TRIPLET=x64-windows-static"
SET "INSTALL_ROOT=C:\Program Files (x86)"
GOTO :ARCH_DONE

:ARCH_ARM64
SET "CMAKE_PLATFORM=ARM64"
SET "VCPKG_TRIPLET=arm64-windows-static"
SET "INSTALL_ROOT=C:\Program Files"
GOTO :ARCH_DONE

:ARCH_DONE
IF "%1%"=="configure" (
  IF "%BE%"=="mingw" (
    SET PATH=%MINGWPATH%
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "MinGW Makefiles" ..\.. || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "%CMAKE_GENERATOR%" -A %CMAKE_PLATFORM% -D CMAKE_BUILD_TYPE="Release" -D CMAKE_INSTALL_PREFIX="%INSTALL_ROOT%\libarchive" --toolchain "%VCPKG_INSTALLATION_ROOT%\scripts\buildsystems\vcpkg.cmake" -D VCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET% -D VCPKG_MANIFEST_DIR=%GITHUB_WORKSPACE%\build\ci\github_actions ..\.. || EXIT /b 1
  ) ELSE IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    SET CYGWIN_NOWINPATH=1
    D:\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a configure" || EXIT /b 1
  )
) ELSE IF "%1%"=="build" (
  IF "%BE%"=="mingw" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    mingw32-make -j %NUMBER_OF_PROCESSORS% VERBOSE=1 || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
  ) ELSE IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    SET MAKE_ARGS=-j %NUMBER_OF_PROCESSORS%
    SET CYGWIN_NOWINPATH=1
    D:\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a build" || EXIT /b 1
  )
) ELSE IF "%1%"=="test" (
  SET _VERBOSITY_LEVEL=2
  IF "%BE%"=="mingw" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    SET SKIP_TEST_SPARSE=1
    mingw32-make test VERBOSE=1 || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
  ) ELSE IF "%BE%"=="cygwin-gcc" (
    REM SET BS=cmake
    REM SET CYGWIN_NOWINPATH=1
    REM SET SKIP_TEST_SPARSE=1
    ECHO "Skipping tests on this platform"
    REM D:\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a test" || EXIT /b 1
    EXIT /b 0
  )
) ELSE IF "%1%"=="install" (
  IF "%BE%"=="mingw" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  ) ELSE IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    SET CYGWIN_NOWINPATH=1
    D:\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a install" || EXIT /b 1
    REM Exit early here; the build.sh install step for Cygwin prints the version.
    EXIT /b 0
  )
  "%INSTALL_ROOT%\libarchive\bin\bsdtar.exe" --version
) ELSE IF "%1"=="artifact" (
  IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    SET CYGWIN_NOWINPATH=1
    D:\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a artifact" || EXIT /b 1
    EXIT /b 0
  )

  C:\windows\system32\tar.exe -c -C "%INSTALL_ROOT%" --format=zip -f libarchive.zip libarchive
) ELSE (
  ECHO "Usage: %0% deplibs|configure|build|test|install|artifact"
  @EXIT /b 0
)
@EXIT /b 0
