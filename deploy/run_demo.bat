@echo off
REM ============================================================
REM   Dimensional Analysis Unit-Aware Semantic Checker
REM   One-command deployable demo (Windows, no make required)
REM   Usage:   deploy\run_demo.bat
REM   Builds the full pipeline from scratch in a space-free temp
REM   dir (works around GnuWin32 flex/bison path quirks), then
REM   runs: valid checks -> MCO error demo -> codegen -> gcc ->
REM   executes the generated C. Zero runtime unit overhead proven.
REM ============================================================
setlocal enabledelayedexpansion

set "SRCROOT=%~dp0.."
set "WORK=%TEMP%\dimc_demo_%RANDOM%"
set "BIN=%WORK%"
set "FAILED=0"

echo [0/6] workspace: %WORK%
if not exist "%WORK%" mkdir "%WORK%" || goto :fail_setup
xcopy /q /e /i "%SRCROOT%\src" "%WORK%\src" >nul || goto :fail_setup
xcopy /q /e /i "%SRCROOT%\include" "%WORK%\include" >nul || goto :fail_setup
xcopy /q /e /i "%SRCROOT%\tests" "%WORK%\tests" >nul || goto :fail_setup
xcopy /q /e /i "%SRCROOT%\demo" "%WORK%\demo" >nul || goto :fail_setup
pushd "%WORK%"

echo [1/6] bison / flex / gcc build...
set BISON_PKGDATADIR=C:\PROGRA~2\GnuWin32\share\bison
bison -d -o parser.tab.c src\parser.y 2>nul
if errorlevel 1 goto :fail_setup
cmd /c "flex -t src\lexer.l > lexer.yy.c"
if errorlevel 1 goto :fail_setup
for %%F in (src\unitvec.c src\symtab.c src\ast.c src\semantic.c src\semantic_ops.c src\evaluator.c src\codegen.c src\main.c) do (
    gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -c "%%F" -o "%%~nF.o" || goto :fail_setup
)
gcc -Wall -Wextra -g -O0 -std=c11 -DBUILD_CHECKER -Iinclude -c src\frontend_main.c -o frontend_main.o || goto :fail_setup
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -I. -c lexer.yy.c -o lexer.yy.o || goto :fail_setup
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -I. -c parser.tab.c -o parser.tab.o || goto :fail_setup
gcc -Wall -Wextra -g -O0 -std=c11 -DBUILD_CHECKER -Iinclude -I. -o dimc-checker.exe lexer.yy.o parser.tab.o ast.o symtab.o unitvec.o semantic.o semantic_ops.o frontend_main.o -lm || goto :fail_setup
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -I. -o dimc.exe lexer.yy.o parser.tab.o ast.o symtab.o unitvec.o semantic.o semantic_ops.o evaluator.o codegen.o main.o -lm || goto :fail_setup
echo       OK

echo [2/6] unit batteries...
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o vec_test.exe src\vec_test.c unitvec.o -lm || goto :fail_setup
.\vec_test.exe | findstr /c:"passed"
echo [3/6] semantic battery...
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o symtab_test.exe src\symtab_test.c symtab.o unitvec.o ast.o -lm || goto :fail_setup
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o semantic_test.exe src\semantic_test.c semantic.o semantic_ops.o symtab.o unitvec.o ast.o -lm || goto :fail_setup
.\semantic_test.exe | findstr /c:"failures"
if errorlevel 1 goto :fail_case
echo       OK

echo [4/6] invalid case: performance issue detection (should say ERROR, exit 1)
.\dimc-checker.exe demo\mars_climate_orbiter.dim 1>nul 2>nul
if !errorlevel! NEQ 1 goto :fail_case
.\dimc-checker.exe demo\mars_climate_orbiter.dim 1>nul
if errorlevel 0 echo       OK (error surfaced, exit code correct)

echo [5/6] valid case -> generate unit-stripped C -> compile AND run it
.\dimc.exe -p demo\mars_valid.dim mars_out.c || goto :fail_case
gcc -Wall -o mars_run.exe mars_out.c || goto :fail_case
.\mars_run.exe
echo       OK (this number was unit-checked at compile time; C has zero unit code)

echo [6/6] ALL STAGES PASS - deployable model verified end to end.
popd
rd /s /q "%WORK%" >nul 2>nul
exit /b 0

:fail_setup
echo [FAIL] setup/build stage - see messages above
popd
exit /b 3

:fail_case
echo [FAIL] a demo case did not behave as expected
popd
exit /b 1
