// Copyright (C)2004 Dimax ( http://www.xdimax.com )
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#pragma once

#include "Device.h"
#include "WireHost.h"
#include "DeviceFactory.h"

//////////////////////////////////////////////////////////////////////////
// Registry settings:

const char* const regProgramSection = "Program";
const char* const regCommPort = "CommPort";
const BYTE regCommPortDef = 0x1;
const char* const regAddress = "Address";
const BYTE regAddressDef = 0x10;

class CProgram :public CDevice
{
    static CConcreteDeviceFactory<CProgram> sm_Factory;
public:
    std::string GetType() {return "RB2 BOOTLOADER";};
    TStringList GetSubtypes();
    bool SetSubtype(std::string Subtype);

    bool Read(BYTE *pBuffer, UINT Length, HWND Feedback);
    bool Program(BYTE *pBuffer, UINT Length, HWND Feedback);
    bool Verify(BYTE *pBuffer, UINT Length, HWND Feedback);
    bool Erase(HWND Feedback);
    bool Configure();

    CProgram(void);
    virtual ~CProgram(void);
protected:
    struct EepromTypes
    {
        char* m_Name;
        UINT m_Size;
        WORD m_PageSize;
        DWORD m_AddrOffset;
        WORD m_AddrByteNum;
		DWORD m_Delay;
    };
    static EepromTypes sm_EepromTypes[];
    int m_Subtype;
private:

    struct SDeviceOperation
    {
        BYTE *pBuffer;
        ULONG BufLen;
        HWND Feedback;
        BYTE SlaveAddr;
    } m_DevOperation;

    WPARAM prCheckSlave();

    HANDLE m_hBusy;
    bool prSendFeedback(WPARAM wParam, LPARAM lParam);
    bool prStartOperation(BYTE *pBuffer, UINT Length, HWND Feedback,
        unsigned (CProgram::*pFunc)());
    unsigned prEndOperation(WPARAM RetVal);
	wirehost* m_hWireHost;

    unsigned prReadThread();
    unsigned prProgramThread();
    unsigned prVerifyThread();
    unsigned prEraseThread();

    // Functions expect the device has been already opened. 
    WPARAM prDoRead();
    WPARAM prDoProgram();
    WPARAM prDoVerify(bool bAfterErase);
    WPARAM prDoErase();
};

