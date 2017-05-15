// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif

// Avoid compiler complaints trying to use 64bit time_t.
#define _USE_32BIT_TIME_T

// Some CString constructors will be explicit
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

// Turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes

#include <afxdtctl.h>       // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>         // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <process.h>

//const ULONG MIN_BUFFER_SIZE = 0x80;
//const ULONG MAX_BUFFER_SIZE = 0x08000000;
const int MIN_BUFFER_SIZE_POW = 7;
const int MAX_BUFFER_SIZE_POW = 27;
const ULONG MIN_BUFFER_SIZE = 1 << MIN_BUFFER_SIZE_POW;
const ULONG MAX_BUFFER_SIZE = 1 << MAX_BUFFER_SIZE_POW;

#define BORDER_INDENT 4 

#include "resource.h"
#define ID_FIRST_MODE   ID_NIBBLE_MODE
#define ID_LAST_MODE    ID_WORDB1B0_MODE

enum eFileType
{
    ftUnknown,
    ftBinary,
    ftIntelHex,
};

#pragma warning(disable: 4786)

#pragma warning(push, 3)
#pragma warning(disable: 4702)
#include <string>
#include <list>
#include <algorithm>
typedef std::list<std::string> TStringList;
#pragma warning(pop)

