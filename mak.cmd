@echo off
rem run in "Visual Studio x86 Native Tools Command Prompt"
rem set CWB var to your cygwin\bin directory
set CWB=C:\cygwin\bin

if not %1n == n goto %1

rem
:all
rc /fo out\tt7.res tt7.rc

rem -O1 
:cmp
cl /c /GL /MT /DUNICODE /EHsc stdafx.cpp /Fo:"out\stdafx.obj"
cl /c /GL /MT /DUNICODE /EHsc term.cpp /Fo:"out\term.obj"
cl /c /GL /MT /DUNICODE /EHsc tiny.cpp /Fo:"out\tiny.obj"
cl /c /GL /MT /DUNICODE /EHsc keytable.cpp /Fo:"out\keytable.obj"

rem 
:lnk
cl /GL /MT /DUNICODE /EHsc /LTCG /NXCOMPAT /DYNAMICBASE "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" "out\stdafx.obj" "out\term.obj" "out\tiny.obj" "out\keytable.obj" "out\tt7.res" /Fe:"out\TinyTerm7.exe"

rem gcc -mwindows -static -o out/PTYstub PTYstub.c
:pty
path=%path%;%CWB%
%CWB%/sh -c "gcc -mwindows -static -o out/PTYstub PTYstub.c"