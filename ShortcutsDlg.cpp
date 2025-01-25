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

#include "StdAfx.h"
#include "ShortcutsDlg.h"
#include "mainDlg.h"
#include "settings.h"
#include "langpack.h"

static int numberWidth = -1;
static int numberHeight = -1;

static CString defaultActionItems[] = {
	MSIP_SHORTCUT_CALL,
#ifdef _GLOBAL_VIDEO
	MSIP_SHORTCUT_VIDEOCALL,
#endif
	MSIP_SHORTCUT_MESSAGE,
	MSIP_SHORTCUT_DTMF,
	MSIP_SHORTCUT_TRANSFER,
	MSIP_SHORTCUT_ATTENDED_TRANSFER,
	MSIP_SHORTCUT_CONFERENCE,
	MSIP_SHORTCUT_RUNBATCH,
	MSIP_SHORTCUT_CALL_URL,
	MSIP_SHORTCUT_POP_URL,
};
static CString defaultActionValues[] = {
	_T("Call"),
#ifdef _GLOBAL_VIDEO
	_T("Video Call"),
#endif
	_T("Message"),
	_T("DTMF"),
	_T("Call Transfer"),
	_T("Attended Transfer"),
	_T("Conference"),
	_T("Run Batch"),
	_T("Call URL"),
	_T("Pop URL"),

};

ShortcutsDlg::ShortcutsDlg(CWnd* pParent /*=NULL*/)
: CDialog(ShortcutsDlg::IDD, pParent)
{
	Create (IDD, pParent);
}

ShortcutsDlg::~ShortcutsDlg(void)
{
}

int ShortcutsDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (langPack.rtl) {
		ModifyStyleEx(0,WS_EX_LAYOUTRTL);
	}
	return 0;
}

BOOL ShortcutsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	TranslateDialog(this->m_hWnd);

	CString str;
	GetDlgItem(IDC_SHORTCUTS_NUMBER2)->GetWindowText(str);
	str.Format(_T("%s 2"), Translate(str.GetBuffer()));
	GetDlgItem(IDC_SHORTCUTS_NUMBER2)->SetWindowText(str);

	CRect rect;
	GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER)->GetWindowRect(rect);
	numberWidth = rect.Width();
	numberHeight = rect.Height();

	((CButton*)GetDlgItem(IDC_SHORTCUTS_ENABLE))->SetCheck(accountSettings.enableShortcuts);
	((CButton*)GetDlgItem(IDC_SHORTCUTS_BOTTOM))->SetCheck(accountSettings.shortcutsBottom);

	CComboBox *combobox;
	for (int i=0; i<_GLOBAL_SHORTCUTS_QTY; i++) {
		combobox = (CComboBox*)GetDlgItem(IDC_SHORTCUTS_COMBO_SHORTCUT1_TYPE + i * 6);
		Shortcut shortcut;
		if (i < shortcuts.GetCount()) {
			shortcut = shortcuts.GetAt(i);
			GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_LABEL + i * 6)->SetWindowText(shortcut.label);
			GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER + i * 6)->SetWindowText(shortcut.number);
			GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER2 + i * 6)->SetWindowText(shortcut.number2);
		}
		int n = sizeof(defaultActionItems) / sizeof(defaultActionItems[0]);
		int c = 0;
		for (int j = 0; j < n; j++) {
			CString str = defaultActionValues[j];
			if (defaultActionItems[j] == _T("runBatch")
				|| defaultActionItems[j] == _T("callURL")
				|| defaultActionItems[j] == _T("popURL")) {
				str.Append(_T("*"));
			}
			combobox->AddString(Translate(str.GetBuffer()));
			if (shortcut.type == defaultActionItems[j]) {
				c = j;
			}
		}
		combobox->SetCurSel(c);
		if (!shortcut.number2.IsEmpty()) {
			CButton* checkbox = (CButton*)GetDlgItem(IDC_SHORTCUTS_TOGGLE1 + i * 6);
			checkbox->SetCheck(1);
			UpdateToggle(true, i);
		}
		CButton* checkbox = (CButton*)GetDlgItem(IDC_SHORTCUTS_PRESENCE1 + i * 6);
		checkbox->SetCheck(shortcut.presence);
	}

	return TRUE;
}

void ShortcutsDlg::OnDestroy()
{
	mainDlg->shortcutsDlg = NULL;
	CDialog::OnDestroy();
}

void ShortcutsDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	delete this;
}

BEGIN_MESSAGE_MAP(ShortcutsDlg, CDialog)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE1, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE2, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE3, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE4, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE5, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE6, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE7, OnBnClickedToggle)
	ON_BN_CLICKED(IDC_SHORTCUTS_TOGGLE8, OnBnClickedToggle)
	ON_NOTIFY(NM_CLICK, IDC_SHORTCUTS_SYSLINK_TOGGLE, &ShortcutsDlg::OnNMClickSyslinkToggle)
	ON_NOTIFY(NM_CLICK, IDC_SHORTCUTS_SYSLINK_BLF, &ShortcutsDlg::OnNMClickSyslinkBLF)
	ON_BN_CLICKED(IDCANCEL, &ShortcutsDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDOK, &ShortcutsDlg::OnBnClickedOk)
END_MESSAGE_MAP()


void ShortcutsDlg::OnClose() 
{
	DestroyWindow();
}

void ShortcutsDlg::OnBnClickedCancel()
{
	OnClose();
}

void ShortcutsDlg::OnBnClickedOk()
{
	this->ShowWindow(SW_HIDE);
	int oldCount = shortcuts.GetCount();
	mainDlg->ShortcutsRemoveAll();
	for (int i=0; i<_GLOBAL_SHORTCUTS_QTY; i++) {
		Shortcut shortcut;
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_LABEL + i * 6)->GetWindowText(shortcut.label);
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER + i * 6)->GetWindowText(shortcut.number);
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER2 + i * 6)->GetWindowText(shortcut.number2);
		CButton* checkbox = (CButton*)GetDlgItem(IDC_SHORTCUTS_TOGGLE1 + i * 6);
		if (!checkbox->GetCheck()) {
			shortcut.number2.Empty();
		}
		shortcut.presence = ((CButton*)GetDlgItem(IDC_SHORTCUTS_PRESENCE1 + i * 6))->GetState();
		int n = ((CComboBox*)GetDlgItem(IDC_SHORTCUTS_COMBO_SHORTCUT1_TYPE + i * 6))->GetCurSel();
		if (n >= 0 && !shortcut.label.IsEmpty() &&
			(!shortcut.number.IsEmpty() ||
				defaultActionItems[n] == MSIP_SHORTCUT_TRANSFER ||
				defaultActionItems[n] == MSIP_SHORTCUT_ATTENDED_TRANSFER)
			) {
			shortcut.type = defaultActionItems[n];
			shortcuts.Add(shortcut);
		}
	}
	ShortcutsSave();

	bool enabled = ((CButton*)GetDlgItem(IDC_SHORTCUTS_ENABLE))->GetCheck();
	bool bottom = ((CButton*)GetDlgItem(IDC_SHORTCUTS_BOTTOM))->GetCheck();

	if (mainDlg->shortcutsEnabled != enabled
		||
		mainDlg->shortcutsBottom != bottom
		||
		(!bottom && oldCount <= 12 && shortcuts.GetCount() > 12)
		|| (bottom && shortcuts.GetCount() > oldCount)
		) {
			AfxMessageBox(Translate(_T("You need to restart application for the changes to take effect.")));
			accountSettings.enableShortcuts=enabled;
			accountSettings.shortcutsBottom = bottom;
			accountSettings.SettingsSave();
	}
	else {
		mainDlg->pageDialer->RebuildShortcuts();
	}
	OnClose();
}

void ShortcutsDlg::OnBnClickedToggle()
{
	CComboBox* combobox;
	bool hasChecked = false;
	for (int i = 0; i < _GLOBAL_SHORTCUTS_QTY; i++) {
		CButton* checkbox = (CButton*)GetDlgItem(IDC_SHORTCUTS_TOGGLE1 + i * 6);
		if (checkbox == GetFocus()) {
			UpdateToggle(checkbox->GetCheck(), i);
		}
		if (!hasChecked && checkbox->GetCheck()) {
			hasChecked = true;
		}
	}
	if (!hasChecked) {
		GetDlgItem(IDC_SHORTCUTS_NUMBER2)->ShowWindow(SW_HIDE);
	}
}

void ShortcutsDlg::OnNMClickSyslinkToggle(NMHDR *pNMHDR, LRESULT *pResult)
{
	OpenHelp(_T("toggle"));
	*pResult = 0;
}

void ShortcutsDlg::OnNMClickSyslinkBLF(NMHDR *pNMHDR, LRESULT *pResult)
{
	OpenHelp(_T("BLF"));
	*pResult = 0;
}

void ShortcutsDlg::UpdateToggle(bool check, int i)
{
	if (check) {
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER + i * 6)->SetWindowPos(NULL, 0, 0, numberWidth / 2, numberHeight, SWP_NOMOVE | SWP_NOZORDER);
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER2 + i * 6)->ShowWindow(SW_NORMAL);
		GetDlgItem(IDC_SHORTCUTS_NUMBER2)->ShowWindow(SW_SHOW);
	}
	else {
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER + i * 6)->SetWindowPos(NULL, 0, 0, numberWidth, numberHeight, SWP_NOMOVE | SWP_NOZORDER);
		GetDlgItem(IDC_SHORTCUTS_EDIT_SHORTCUT1_NUMBER2 + i * 6)->ShowWindow(SW_HIDE);
	}
}
