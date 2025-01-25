/*
 * Copyright (C) 2011-2024 MicroSIP (http://www.microsip.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stdafx.h"
#include "microsip.h"
#include "AAOptionsDlg.h"
#include "mainDlg.h"
#include "langpack.h"
#include "settings.h"

AAOptionsDlg::AAOptionsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(AAOptionsDlg::IDD, pParent)
{
	Create (IDD, pParent);
}

AAOptionsDlg::~AAOptionsDlg()
{
}

int AAOptionsDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (langPack.rtl) {
		ModifyStyleEx(0,WS_EX_LAYOUTRTL);
	}
	POINT p;
	if (GetCursorPos(&p)) {
		SetWindowPos(NULL, p.x-250, p.y+10, 0, 0, SWP_NOZORDER| SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
	}
	return 0;
}

BOOL AAOptionsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	TranslateDialog(this->m_hWnd);
	CString str;
	str.Format(_T("%d"), accountSettings.autoAnswerDelay);
	GetDlgItem(IDC_AA_OPTIONS_DELAY)->SetWindowText(str);
	GetDlgItem(IDC_AA_OPTIONS_NUMBER)->SetWindowText(accountSettings.autoAnswerNumber);
	return TRUE;
}

void AAOptionsDlg::OnDestroy()
{
	mainDlg->settingsDlg->aaOptionsDlg = NULL;
	CDialog::OnDestroy();
}

void AAOptionsDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	delete this;
}

BEGIN_MESSAGE_MAP(AAOptionsDlg, CDialog)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDOK, &AAOptionsDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &AAOptionsDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


void AAOptionsDlg::OnClose() 
{
	DestroyWindow();
}

void AAOptionsDlg::OnBnClickedOk()
{
	CString str;
	GetDlgItem(IDC_AA_OPTIONS_DELAY)->GetWindowText(str);
	accountSettings.autoAnswerDelay = _wtoi(str);
	GetDlgItem(IDC_AA_OPTIONS_NUMBER)->GetWindowText(accountSettings.autoAnswerNumber);
	accountSettings.autoAnswerNumber.Trim();
	accountSettings.SettingsSave();
	OnClose();
}

void AAOptionsDlg::OnBnClickedCancel()
{
	OnClose();
}

