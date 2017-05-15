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

#include "StdAfx.h"
#include "Program.h"
#include "ProgramConfig.h"
#include <assert.h>
#include "ThreadUtils.h"
#include "Settings.h"
#include "resource.h"

CConcreteDeviceFactory<CProgram> CProgram::sm_Factory;
const int erU2CError = 1;
CProgram::EepromTypes CProgram::sm_EepromTypes[] = {
    {"ATMega128 Flash", 131072, 256, 0, 3, 400},
    {"ATMega128 EEPROM", 4096, 256, 131072, 3, 600},
    {"ATMega168 Flash", 16384, 128, 0, 2, 400},
    {"ATMega168 EEPROM", 512, 128, 16384, 2, 600},
    {"ATMega328 Flash", 32768, 128, 0, 2, 400},
    {"ATMega328 EEPROM", 1024, 128, 32768, 2, 600},
    {"ATMega8 Flash", 8192, 64, 0, 2, 400},
    {"ATMega8 EEPROM", 512, 64, 8192, 2, 800},
};

CProgram::CProgram(void)
: m_Subtype(0), m_hWireHost(NULL)
{
    m_hBusy = CreateMutex(NULL, FALSE, NULL);
}

CProgram::~CProgram(void)
{
    CloseHandle(m_hBusy);
}

TStringList CProgram::GetSubtypes()
{
    TStringList Res;
    int ArrSize = sizeof(sm_EepromTypes)/sizeof(EepromTypes);
    for (int i=0; i<ArrSize; i++)
    {
        Res.push_back(sm_EepromTypes[i].m_Name);
    }
    return Res;
}

bool CProgram::SetSubtype(std::string DevSubType)
{
    int ArrSize = sizeof(sm_EepromTypes)/sizeof(EepromTypes);
    for (int i=0; i<ArrSize; i++)
    {
        if (DevSubType == sm_EepromTypes[i].m_Name)
        {
            m_Subtype = i;
            return true;
        }
    }
    return false;
}

bool CProgram::Read(BYTE* pBuffer, UINT Length, HWND Feedback)
{
    return prStartOperation(pBuffer, Length, Feedback, &CProgram::prReadThread);
}

bool CProgram::Program(BYTE* pBuffer, UINT Length, HWND Feedback)
{
    return prStartOperation(pBuffer, Length, Feedback, &CProgram::prProgramThread);
}

bool CProgram::Verify(BYTE* pBuffer, UINT Length, HWND Feedback)
{
    return prStartOperation(pBuffer, Length, Feedback, &CProgram::prVerifyThread);
}

bool CProgram::Erase(HWND Feedback)
{
    return prStartOperation(NULL, sm_EepromTypes[m_Subtype].m_Size, Feedback, &CProgram::prEraseThread);
}

bool CProgram::Configure()
{
    CProgramConfig dlg;
    dlg.DoModal();
    return true;
}

bool CProgram::prStartOperation(BYTE *pBuffer, UINT Length, HWND Feedback, unsigned (CProgram::*pFunc)())
{
    if (WaitForSingleObject(m_hBusy, 0) == WAIT_TIMEOUT)
    {
        prSendFeedback(DF_U2C_BUSY, 0);
        return false;
    }
    if (Length == 0)
    {
        assert(false);
        prEndOperation(DF_INVALID_PARAMETER);
        return false;
    }

	// Get the settings.
	CSettings *pSet = CSettings::Instance();

	// Get the communications port to open.
	BYTE CommPort = pSet->GetProfileByte(regProgramSection, regCommPort, regCommPortDef);
	BYTE Address = pSet->GetProfileByte(regProgramSection, regAddress, regAddressDef);

	// Fill the operation buffer.
    m_DevOperation.pBuffer = pBuffer;
    m_DevOperation.BufLen = Length;
    m_DevOperation.Feedback = Feedback;
    m_DevOperation.SlaveAddr = Address;

	// Create the wirehost object using the specified commport.
	assert(m_hWireHost == NULL);
	m_hWireHost = wirehost_new(CommPort);
    if (m_hWireHost == NULL)
    {
        prEndOperation(DF_U2C_OPEN_FAILED);
        return false;
    }

    WPARAM Res;
    if (DF_CHECK_SLAVE_SUCCEEDED != (Res = prCheckSlave()))
    {
        prEndOperation(Res);
        return false;
    }

    HANDLE hThread = BeginThread(this, pFunc);
    if (hThread == 0)
    {
        assert(false);
        prEndOperation(DF_U2C_OPEN_FAILED);
        return false;
    }
    CloseHandle(hThread);
    return true;
}

WPARAM CProgram::prCheckSlave()
{
    if (m_DevOperation.BufLen > sm_EepromTypes[m_Subtype].m_Size)
    {
        if (IDCANCEL == AfxMessageBox(IDS_LARGE_BUFFER, MB_OKCANCEL))
            return DF_OPERATION_CANCELLED;
        else
            m_DevOperation.BufLen = sm_EepromTypes[m_Subtype].m_Size;
    }
	if (!wirehost_check_slave(m_hWireHost, m_DevOperation.SlaveAddr))
	{
        return DF_CHIP_NOT_FOUND;
	}
    return DF_CHECK_SLAVE_SUCCEEDED;
}

bool CProgram::prSendFeedback(WPARAM wParam, LPARAM lParam)
{
    if (PostMessage(m_DevOperation.Feedback, WM_DEVICE_FEEDBACK, wParam, lParam))
        return true;
    else
        return false;
}

unsigned CProgram::prEndOperation(WPARAM RetVal)
{
    if(m_hWireHost != NULL)
    {
        wirehost_free(&m_hWireHost);
        m_hWireHost = NULL;
    }
    prSendFeedback(RetVal, 0);
    ReleaseMutex(m_hBusy);
    return (unsigned) RetVal;
}

unsigned CProgram::prReadThread()
{
    assert(m_hWireHost != NULL);
    return prEndOperation(prDoRead());
}

unsigned CProgram::prProgramThread()
{
    WPARAM Res;
    assert(m_hWireHost != NULL);
    if (DF_PROGRAM_FINISHED != (Res = prDoProgram()))
        return prEndOperation(Res);
    if (DF_VERIFY_FINISHED != (Res = prDoVerify(false)))
        return prEndOperation(Res);
    return prEndOperation(DF_PROGRAM_FINISHED);
}
unsigned CProgram::prVerifyThread()
{
    assert(m_hWireHost != NULL);
    return prEndOperation(prDoVerify(false));
}

unsigned CProgram::prEraseThread()
{
    WPARAM Res;
    assert(m_hWireHost != NULL);
    if (DF_ERASE_FINISHED != (Res = prDoErase()))
        return prEndOperation(Res);
    if (DF_VERIFY_FINISHED != (Res = prDoVerify(true)))
        return prEndOperation(Res);
    return prEndOperation(DF_ERASE_FINISHED);
}

WPARAM CProgram::prDoRead()
{
    if ((m_DevOperation.BufLen == 0) || (m_DevOperation.pBuffer == NULL))
    {
        assert(false);
        return DF_INVALID_PARAMETER;
    }
    DWORD CurrAddr;
    USHORT CurrBufferLen;
    assert(m_DevOperation.BufLen != 0);
    for(CurrAddr = 0; CurrAddr < m_DevOperation.BufLen; CurrAddr += CurrBufferLen)
    {
        prSendFeedback(DF_READ_PROCEED, CurrAddr * 100 / m_DevOperation.BufLen);
        CurrBufferLen = sm_EepromTypes[m_Subtype].m_PageSize;
        if (CurrAddr + CurrBufferLen > m_DevOperation.BufLen)
            CurrBufferLen = USHORT(m_DevOperation.BufLen - CurrAddr);
		if (!wirehost_read_buffer(m_hWireHost, m_DevOperation.SlaveAddr, CurrAddr + sm_EepromTypes[m_Subtype].m_AddrOffset, sm_EepromTypes[m_Subtype].m_AddrByteNum, CurrBufferLen, m_DevOperation.pBuffer+CurrAddr))
            return DF_READ_FAILED;
    }
    return DF_READ_FINISHED;
}

WPARAM CProgram::prDoProgram()
{
    if ((m_DevOperation.BufLen == 0) || (m_DevOperation.pBuffer == NULL))
    {
        assert(false);
        return DF_INVALID_PARAMETER;
    }
    DWORD CurrAddr;
    USHORT CurrBufferLen;
    assert(m_DevOperation.BufLen != 0);
    for(CurrAddr = 0; CurrAddr < m_DevOperation.BufLen; CurrAddr += CurrBufferLen)
    {
        prSendFeedback(DF_PROGRAM_PROCEED, CurrAddr * 100 / m_DevOperation.BufLen);
        CurrBufferLen = sm_EepromTypes[m_Subtype].m_PageSize;
        if (CurrAddr + CurrBufferLen > m_DevOperation.BufLen)
            CurrBufferLen = USHORT(m_DevOperation.BufLen - CurrAddr);
		if (!wirehost_write_buffer(m_hWireHost, m_DevOperation.SlaveAddr, CurrAddr + sm_EepromTypes[m_Subtype].m_AddrOffset, sm_EepromTypes[m_Subtype].m_AddrByteNum, CurrBufferLen, m_DevOperation.pBuffer+CurrAddr))
            return DF_PROGRAM_FAILED;
        Sleep(sm_EepromTypes[m_Subtype].m_Delay);
    }
    return DF_PROGRAM_FINISHED;
}

WPARAM CProgram::prDoVerify(bool bAfterErase)
{
    if (m_DevOperation.BufLen == 0)
    {
        assert(false);
        return DF_INVALID_PARAMETER;
    }
    if ((!bAfterErase) && (m_DevOperation.pBuffer == NULL))
    {
        assert(false);
        return DF_INVALID_PARAMETER;
    }
    DWORD CurrAddr;
    USHORT CurrBufferLen;
    BYTE ffBuffer[256];
    FillMemory(ffBuffer, 256, 0xFF);
    assert(m_DevOperation.BufLen != 0);
    for(CurrAddr = 0; CurrAddr < m_DevOperation.BufLen; CurrAddr += CurrBufferLen)
    {
		BYTE buffer[256];
        prSendFeedback(DF_VERIFY_PROCEED, CurrAddr * 100 / m_DevOperation.BufLen);
        CurrBufferLen = sm_EepromTypes[m_Subtype].m_PageSize;
        if (CurrAddr + CurrBufferLen > m_DevOperation.BufLen)
            CurrBufferLen = USHORT(m_DevOperation.BufLen - CurrAddr);
		if (!wirehost_read_buffer(m_hWireHost, m_DevOperation.SlaveAddr, CurrAddr + sm_EepromTypes[m_Subtype].m_AddrOffset, sm_EepromTypes[m_Subtype].m_AddrByteNum, CurrBufferLen, buffer))
            return DF_VERIFY_FAILED;
        if (bAfterErase)
        {
            // Compare the transaction buffer with the erased buffer values.
            if (memcmp(ffBuffer, buffer, CurrBufferLen) != 0)
                return DF_VERIFY_FAILED;
        }
        else
        {
            // Compare the transaction buffer with the erased buffer values.
            if (memcmp(m_DevOperation.pBuffer + CurrAddr, buffer, CurrBufferLen) != 0)
                return DF_VERIFY_FAILED;
        }
    }
    return DF_VERIFY_FINISHED;
}

WPARAM CProgram::prDoErase()
{
    if (m_DevOperation.BufLen == 0)
    {
        assert(false);
        return DF_INVALID_PARAMETER;
    }
	BYTE buffer[256];
    DWORD CurrAddr;
    USHORT CurrBufferLen;
    FillMemory(buffer, 256, 0xFF);
    assert(m_DevOperation.BufLen != 0);
    for(CurrAddr = 0; CurrAddr < m_DevOperation.BufLen; CurrAddr += CurrBufferLen)
    {
        prSendFeedback(DF_ERASE_PROCEED, CurrAddr* 100 / m_DevOperation.BufLen);
        CurrBufferLen = sm_EepromTypes[m_Subtype].m_PageSize;
        if (CurrAddr + CurrBufferLen > m_DevOperation.BufLen)
            CurrBufferLen = USHORT(m_DevOperation.BufLen - CurrAddr);
		if (!wirehost_write_buffer(m_hWireHost, m_DevOperation.SlaveAddr, CurrAddr + sm_EepromTypes[m_Subtype].m_AddrOffset, sm_EepromTypes[m_Subtype].m_AddrByteNum, CurrBufferLen, buffer))
            return DF_ERASE_FAILED;
        Sleep(sm_EepromTypes[m_Subtype].m_Delay);
    }
    return DF_ERASE_FINISHED;
}
