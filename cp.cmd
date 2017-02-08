@echo off

:preCleanup()
    cls
    set fileName=ppp
    set libs=shell32.lib
    del %fileName%.exe 2>nul
    del %fileName%.obj 2>nul

:build()
    lcc -c -Ic:\lcc\include -A %fileName%.c
    if ERRORLEVEL 1 goto postCleanup()
    lcclnk -subsystem windows -s %fileName%.obj app.res %libs% -o %fileName%.exe

:postCleanup()
    del %fileName%.obj 2>nul

@echo on
