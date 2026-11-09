@echo off
rem ===========================================================================
rem sign_image.cmd - Windows equivalent of sign_image.sh
rem Sign an RZ/A3M application image for MCUboot.
rem
rem usage:
rem   sign_image.cmd ramload^|swap factory^|test^|raw IN.elf^|IN.bin VERSION UI_BANK OUT.bin [SLOT_ADDR]
rem
rem   factory  --pad --confirm   permanent image; first image in ram-load+revert
rem   test     --pad             trial boot
rem   raw      no trailer        OTA payload; the app calls rza3m_app_mark_pending()
rem   SLOT_ADDR (optional)       also write OUT.srec at this NOR address
rem                              slot A = 0x20100000, slot B = 0x20200000
rem
rem Optional environment variables:
rem   MCUBOOT_DIR   MCUboot repository (default: three folders above this script)
rem   SIGNING_KEY   private key; only when MCUboot is built with MCUBOOT_SIGN_*
rem   PYTHON        Python command (default: py -3, python, python3)
rem   OBJCOPY       objcopy for .elf input / .srec output
rem                 (default: aarch64-none-elf-objcopy on PATH)
rem
rem Layout values must match include/rza3m_flash_layout.h.
rem ===========================================================================
setlocal EnableExtensions DisableDelayedExpansion

set "HDR_SIZE=0x1000"
set "ALIGN=8"
set "SLOT_SIZE=0x100000"
set "ERASE_SIZE=0x10000"
set "LOAD_ADDR=0x401FF000"
set /a ENTRY_ADDR=LOAD_ADDR + HDR_SIZE
set /a MAX_SECTORS=SLOT_SIZE / ERASE_SIZE

rem ---- arguments ------------------------------------------------------------
if "%~6"=="" goto :usage
if not "%~8"=="" goto :usage

set "MODE=%~1"
set "KIND=%~2"
set "IN=%~f3"
set "IN_EXT=%~x3"
set "VER=%~4"
set "UI=%~5"
set "OUT=%~f6"
set "OUT_DIR=%~dp6"
set "SLOT_ADDR=%~7"

if /i not "%MODE%"=="ramload" if /i not "%MODE%"=="swap" (
    echo [sign] ERROR: mode must be ramload or swap
    exit /b 2
)
set "KIND_ARGS="
if /i "%KIND%"=="factory" set "KIND_ARGS=--pad --confirm"
if /i "%KIND%"=="test"    set "KIND_ARGS=--pad"
if /i "%KIND%"=="raw"     set "KIND_ARGS= "
if not defined KIND_ARGS (
    echo [sign] ERROR: kind must be factory, test or raw
    exit /b 2
)

echo %UI%| findstr /r "^[0-9][0-9]*$" >nul || (
    echo [sign] ERROR: ui_bank must be a decimal number
    exit /b 2
)
echo %VER%| findstr /r "^[0-9][0-9.+]*$" >nul || (
    echo [sign] ERROR: version must look like 1.2.3 or 1.2.3+4
    exit /b 2
)
if not exist "%IN%" (
    echo [sign] ERROR: input not found: %IN%
    exit /b 2
)
if /i "%IN%"=="%OUT%" (
    echo [sign] ERROR: input and output must differ
    exit /b 2
)

rem ---- tools ----------------------------------------------------------------
if not defined MCUBOOT_DIR set "MCUBOOT_DIR=%~dp0..\..\.."
for %%I in ("%MCUBOOT_DIR%") do set "MCUBOOT_DIR=%%~fI"
set "IMGTOOL=%MCUBOOT_DIR%\scripts\imgtool.py"
if not exist "%IMGTOOL%" (
    echo [sign] ERROR: imgtool not found: %IMGTOOL% - set MCUBOOT_DIR
    exit /b 3
)

if not defined OBJCOPY set "OBJCOPY=aarch64-none-elf-objcopy"

set "PY="
if defined PYTHON set "PY=%PYTHON%"
if not defined PY py -3 -c "import sys" >nul 2>&1 && set "PY=py -3"
if not defined PY python -c "import sys" >nul 2>&1 && set "PY=python"
if not defined PY python3 -c "import sys" >nul 2>&1 && set "PY=python3"
if not defined PY (
    echo [sign] ERROR: Python not found - set PYTHON
    exit /b 3
)

set "WORK=%TEMP%\rza3m_sign_%RANDOM%%RANDOM%"
mkdir "%WORK%" || exit /b 3
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

rem ---- 1. raw binary --------------------------------------------------------
set "BIN=%IN%"
if /i not "%IN_EXT%"==".elf" goto :have_bin
"%OBJCOPY%" -O binary "%IN%" "%WORK%\app.bin"
if errorlevel 1 (
    echo [sign] ERROR: objcopy failed - is OBJCOPY set correctly? "%OBJCOPY%"
    goto :fail
)
set "BIN=%WORK%\app.bin"
:have_bin

rem ---- 2. strip RZ FSP loader header and check the link address -------------
set "HELPER=%WORK%\strip_fsp.py"
 >"%HELPER%" echo import sys,struct
>>"%HELPER%" echo src,dst,mode,entry,limit=sys.argv[1],sys.argv[2],sys.argv[3],int(sys.argv[4],0),int(sys.argv[5],0)
>>"%HELPER%" echo d=open(src,'rb').read()
>>"%HELPER%" echo if len(d) ^> 2*limit: sys.exit('[sign] ERROR: binary is %%d bytes - check the section load addresses' %% len(d))
>>"%HELPER%" echo ok=len(d) ^> 512 and d[0x1E0:0x1F2]==b'RZ FSP Application'
>>"%HELPER%" echo if not ok and mode=='ramload': print('[sign] WARNING: no FSP header - entry address not checked')
>>"%HELPER%" echo if not ok: open(dst,'wb').write(d); sys.exit(0)
>>"%HELPER%" echo start,_,copy,_,size,_=struct.unpack_from('^<6Q',d,0)
>>"%HELPER%" echo print('[sign] FSP header: entry=0x%%X copy=0x%%X size=0x%%X' %% (start,copy,size))
>>"%HELPER%" echo if size==0 or size ^> len(d)-512: sys.exit('[sign] ERROR: FSP header size does not match the file')
>>"%HELPER%" echo if mode=='ramload' and (start-entry or copy-entry): sys.exit('[sign] ERROR: entry 0x%%X copy 0x%%X, expected 0x%%X - link the app at LOAD_ADDR+HDR_SIZE' %% (start,copy,entry))
>>"%HELPER%" echo open(dst,'wb').write(d[512:512+size])

%PY% "%HELPER%" "%BIN%" "%WORK%\payload.bin" %MODE% %ENTRY_ADDR% %SLOT_SIZE%
if errorlevel 1 goto :fail

rem ---- 3. imgtool -----------------------------------------------------------
set "LOAD_ARGS="
if /i "%MODE%"=="ramload" set "LOAD_ARGS=--load-addr %LOAD_ADDR%"
set "KEY_ARGS="
if defined SIGNING_KEY set KEY_ARGS=--key "%SIGNING_KEY%"

echo [sign] %MODE%/%KIND% v%VER% ui_bank=%UI% -^> %OUT%
%PY% "%IMGTOOL%" sign --header-size %HDR_SIZE% --pad-header --align %ALIGN% --slot-size %SLOT_SIZE% --max-sectors %MAX_SECTORS% --version %VER% --custom-tlv 0xA0 %UI% %LOAD_ARGS% %KEY_ARGS% %KIND_ARGS% "%WORK%\payload.bin" "%OUT%"
if errorlevel 1 (
    echo [sign] ERROR: imgtool sign failed
    goto :fail
)

%PY% "%IMGTOOL%" dumpinfo "%OUT%"
if errorlevel 1 (
    echo [sign] ERROR: imgtool dumpinfo failed
    goto :fail
)

rem ---- 4. optional S-record --------------------------------------------------
rem if "%SLOT_ADDR%"=="" goto :done
rem set "SREC=%~dpn6.srec"
rem "%OBJCOPY%" -I binary -O srec --srec-forceS3 --srec-len=32 --change-addresses=%SLOT_ADDR% "%OUT%" "%SREC%"
rem if errorlevel 1 (
rem     echo [sign] ERROR: S-record conversion failed
rem     goto :fail
rem )
rem echo [sign] S-record: %SREC% (at %SLOT_ADDR%)

:done
rmdir /s /q "%WORK%" 2>nul
echo [sign] SUCCESS
exit /b 0

:fail
rmdir /s /q "%WORK%" 2>nul
exit /b 1

:usage
echo usage:
echo   sign_image.cmd ramload^|swap factory^|test^|raw IN.elf^|IN.bin VERSION UI_BANK OUT.bin [SLOT_ADDR]
echo   slot A = 0x20100000, slot B = 0x20200000
exit /b 2
