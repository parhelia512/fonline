echo off

if [%FO_PROJECT_ROOT%] == [] (
    set FO_PROJECT_ROOT=.
)
if [%FO_ENGINE_ROOT%] == [] (
    set FO_ENGINE_ROOT=%~p0/..
)
if [%FO_WORKSPACE%] == [] (
    set FO_WORKSPACE=Workspace
)
if [%FO_OUTPUT%] == [] (
    set FO_OUTPUT=%FO_WORKSPACE%/output
)

echo Setup environment

CALL :NORMALIZEPATH %FO_PROJECT_ROOT%
set FO_PROJECT_ROOT=%RETVAL%
CALL :NORMALIZEPATH %FO_ENGINE_ROOT%
set FO_ENGINE_ROOT=%RETVAL%
CALL :NORMALIZEPATH %FO_WORKSPACE%
set FO_WORKSPACE=%RETVAL%
CALL :NORMALIZEPATH %FO_OUTPUT%
set FO_OUTPUT=%RETVAL%

echo FO_PROJECT_ROOT=%FO_PROJECT_ROOT%
echo FO_ENGINE_ROOT=%FO_ENGINE_ROOT%
echo FO_WORKSPACE=%FO_WORKSPACE%
echo FO_OUTPUT=%FO_OUTPUT%

if [%1] == [win32] (
    set BUILD_ARCH=Win32
) else if [%1] == [win64] (
    set BUILD_ARCH=x64
) else if [%1] == [uwp] (
    set BUILD_ARCH=x64
    set BUILD_CACHE=uwp.cache.cmake
) else if [%1] == [win32-clang] (
    set BUILD_ARCH=Win32
    set BUILD_TOOLSET=ClangCL
) else if [%1] == [win64-clang] (
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else (
    echo Invalid build argument
    exit /b 1
)

if [%2] == [client] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=1 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [server] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=1 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [single] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=1 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [editor] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=1 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [mapper] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=1 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [ascompiler] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [baker] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [toolset] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [full] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=1 -DFO_BUILD_SERVER=1 -DFO_BUILD_SINGLE=1 -DFO_BUILD_EDITOR=1 -DFO_BUILD_MAPPER=1 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0
) else if [%2] == [unit-tests] (
    set BUILD_TARGET=-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_SINGLE=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=1 -DFO_CODE_COVERAGE=0
) else (
    echo Invalid build argument
    exit /b 1
)

if [%3] == [] (
    set CONFIG=Release
) else (
    set CONFIG=%3
)

if not exist %FO_WORKSPACE% mkdir %FO_WORKSPACE%
cd %FO_WORKSPACE%

set BUILD_DIR=build-%1-%2-%CONFIG%
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

if not [%BUILD_TOOLSET%] == [] (
    cmake -A %BUILD_ARCH% -T %BUILD_TOOLSET% -DFO_OUTPUT_PATH="%FO_OUTPUT%" %BUILD_TARGET% "%FO_PROJECT_ROOT%"
) else if [%BUILD_CACHE%] == [] (
    cmake -A %BUILD_ARCH% -DFO_OUTPUT_PATH="%FO_OUTPUT%" %BUILD_TARGET% "%FO_PROJECT_ROOT%"
) else (
    cmake -A %BUILD_ARCH% -C "%FO_ENGINE_ROOT%\BuildTool\%BUILD_CACHE%" -DFO_OUTPUT_PATH="%FO_OUTPUT%" %BUILD_TARGET% "%FO_PROJECT_ROOT%"
)

cmake --build . --config %CONFIG% --parallel

exit /b

:NORMALIZEPATH
  set RETVAL=%~f1
  exit /b
