//
// tinyTerm7 -- A minimal Windows terminal emulator
//
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <commdlg.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <wchar.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>

#include <vector>
typedef std::vector<char> Tvector_char;

//#include <stdlib.h>
//#include <malloc.h>
//#include <memory.h>
//#include <tchar.h>

// TODO: reference additional headers your program requires here
