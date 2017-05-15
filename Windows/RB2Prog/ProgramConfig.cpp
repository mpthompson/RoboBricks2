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


#include "stdafx.h"
#include "RB2Prog.h"
#include "ProgramConfig.h"
#include "Settings.h"
#include "Program.h"

// CProgramConfig dialog

IMPLEMENT_DYNAMIC(CProgramConfig, CDialog)
CProgramConfig::CProgramConfig(CWnd* pParent /*=NULL*/)
    : CDialog(CProgramConfig::IDD, pParent)
    , m_strAddress(_T(""))
{
}

CProgramConfig::~CProgramConfig()
{
}

void CProgramConfig::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMMPORT, m_cmbCommPort);
    DDX_Text(pDX, IDC_ADDRESS, m_strAddress);
}


BEGIN_MESSAGE_MAP(CProgramConfig, CDialog)
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()


// CProgramConfig message handlers
CProgramConfig::SCommPort CProgramConfig::sm_CommPort[] =
{
    {"COM1", 1},
    {"COM2", 2},
    {"COM3", 3},
    {"COM4", 4},
    {"COM5", 5},
    {"COM6", 6},
    {"COM7", 7},
    {"COM8", 8},
    {"COM9", 9},
    {"COM10", 10},
    {"COM11", 11},
    {"COM12", 12},
    {"COM13", 13},
    {"COM14", 14},
    {"COM15", 15},
    {"COM16", 16},
    {"COM17", 17},
    {"COM18", 18},
    {"COM19", 19},
    {"COM20", 20}
};

BOOL CProgramConfig::OnInitDialog()
{
    CDialog::OnInitDialog();
    CSettings *pSet = CSettings::Instance();
    BYTE CommPort = pSet->GetProfileByte(regProgramSection, regCommPort, regCommPortDef);
    BYTE Address = pSet->GetProfileByte(regProgramSection, regAddress, regAddressDef);
    int i;
    int CommPortNum = sizeof(sm_CommPort) / sizeof(SCommPort);
    for (i = 0; i < CommPortNum; i++)
    {
        m_cmbCommPort.AddString(sm_CommPort[i].m_Text);
    }
    for (i = 0; i < CommPortNum; i++)
    {
        if (CommPort <= sm_CommPort[i].m_Value)
        {
            m_cmbCommPort.SetCurSel(i);
            break;
        }
    }
    m_strAddress.Format("%02X", Address);
    UpdateData(FALSE);
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CProgramConfig::OnBnClickedOk()
{
    UpdateData(TRUE);
    char *endp;
    m_strAddress.TrimLeft();
    UINT Address = strtoul(m_strAddress, &endp, 16);
    if ((*endp) || m_strAddress.IsEmpty() ||  (Address > 0xFF))
    {
        MessageBox("Bad value has been used as Address.\nPlease provide value in range from 0 to FF");
        GetDlgItem(IDC_ADDRESS)->SetFocus();
        return;
    }
    BYTE CommPort = sm_CommPort[m_cmbCommPort.GetCurSel()].m_Value;
    CSettings *pSet = CSettings::Instance();
    pSet->WriteProfileByte(regProgramSection, regCommPort, CommPort);
    pSet->WriteProfileByte(regProgramSection, regAddress, (BYTE) Address);
    OnOK();
}
