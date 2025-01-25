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
#include "Calls.h"
#include "microsip.h"
#include "global.h"
#include "settings.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include "mainDlg.h"
#include "langpack.h"
#include "utf.h"
#include "CSVFile.h"
#include "Markup.h"

enum {
	MSIP_CALLS_COL_NAME,
	MSIP_CALLS_COL_NUMBER,
	MSIP_CALLS_COL_TIME,
	MSIP_CALLS_COL_DURATION,
	MSIP_CALLS_COL_INFO,
};

Calls::Calls(CWnd* pParent /*=NULL*/)
	: CBaseDialog(Calls::IDD, pParent)
{
	Create(IDD, pParent);
}

Calls::~Calls(void)
{
}

BOOL Calls::OnInitDialog()
{
	CBaseDialog::OnInitDialog();

	AutoMove(IDC_CALLS, 0, 0, 100, 100);
	AutoMove(IDC_SEARCH_PICTURE, 0, 100, 0, 0);
	AutoMove(IDC_FILER_VALUE, 0, 100, 100, 0);

	TranslateDialog(this->m_hWnd);

	lastDay = 0;

	imageList = new CImageList();
	imageList->Create(16, 16, ILC_COLOR32, 3, 3);
	imageList->SetBkColor(RGB(255, 255, 255));
	imageList->Add(theApp.LoadIcon(IDI_CALL_OUT));
	imageList->Add(theApp.LoadIcon(IDI_CALL_IN));
	imageList->Add(theApp.LoadIcon(IDI_CALL_MISS));
	imageList->Add(theApp.LoadIcon(IDI_CALL_MISS_1));

	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	//list->SetExtendedStyle( list->GetExtendedStyle() |  LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS);
	list->SetExtendedStyle(list->GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	list->SetImageList(imageList, LVSIL_SMALL);

	list->InsertColumn(MSIP_CALLS_COL_NAME, Translate(_T("Name")), LVCFMT_LEFT, accountSettings.callsWidth0 > 0 ? accountSettings.callsWidth0 : 160);
	list->InsertColumn(MSIP_CALLS_COL_NUMBER, Translate(_T("Number")), LVCFMT_LEFT, accountSettings.callsWidth1 > 0 ? accountSettings.callsWidth1 : 100);
	list->InsertColumn(MSIP_CALLS_COL_TIME, Translate(_T("Time")), LVCFMT_LEFT, accountSettings.callsWidth2 > 0 ? accountSettings.callsWidth2 : 135);
	list->InsertColumn(MSIP_CALLS_COL_DURATION, Translate(_T("Duration")), LVCFMT_LEFT, accountSettings.callsWidth3 > 0 ? accountSettings.callsWidth3 : 70);
	list->InsertColumn(MSIP_CALLS_COL_INFO, Translate(_T("Info")), LVCFMT_LEFT, accountSettings.callsWidth4 > 0 ? accountSettings.callsWidth4 : 120);

	CallsLoad();

	SetTimer(IDT_TIMER_CALLS, 300 * 1000, NULL);

	return TRUE;
}

void Calls::OnCreated()
{
	m_SortItemsExListCtrl.SetSortColumn(2, false);
}

void Calls::PostNcDestroy()
{
	CBaseDialog::PostNcDestroy();
	mainDlg->pageCalls = NULL;
	delete imageList;
	delete this;
}

void Calls::DoDataExchange(CDataExchange* pDX)
{
	CBaseDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CALLS, m_SortItemsExListCtrl);
}

void Calls::OnTimer(UINT_PTR TimerVal)
{
	ReloadTime();
}


BEGIN_MESSAGE_MAP(Calls, CBaseDialog)
	ON_WM_CREATE()
	ON_WM_TIMER()
	ON_NOTIFY(HDN_ENDTRACK, 0, OnEndtrack)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
	ON_EN_CHANGE(IDC_FILER_VALUE, OnFilterValueChange)
	ON_COMMAND(ID_CALL, OnMenuCall)
	ON_COMMAND(ID_CHAT, OnMenuChat)
	ON_COMMAND(ID_ADD, OnMenuAdd)
	ON_COMMAND(ID_COPY, OnMenuCopy)
	ON_COMMAND(ID_DELETE, OnMenuDelete)
	ON_COMMAND(ID_EXPORT, OnMenuExport)
	ON_NOTIFY(NM_DBLCLK, IDC_CALLS, &Calls::OnNMDblclkCalls)
	ON_MESSAGE(WM_CONTEXTMENU, OnContextMenu)
#ifdef _GLOBAL_VIDEO
	ON_COMMAND(ID_VIDEOCALL, OnMenuCallVideo)
#endif
END_MESSAGE_MAP()

BOOL Calls::PreTranslateMessage(MSG* pMsg)
{
	BOOL catched = FALSE;
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) {
		CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
		if (edit == GetFocus()) {
			catched = TRUE;
			if (isFiltered()) {
				filterReset();
			}
		}
	}
	if (!catched) {
		return CBaseDialog::PreTranslateMessage(pMsg);
	}
	else {
		return TRUE;
	}
}

void Calls::OnEndtrack(NMHDR* pNMHDR, LRESULT* pResult)
{
	HD_NOTIFY* phdn = (HD_NOTIFY*)pNMHDR;
	int width = phdn->pitem->cxy;
	switch (phdn->iItem) {
	case 0:
		accountSettings.callsWidth0 = width;
		break;
	case 1:
		accountSettings.callsWidth1 = width;
		break;
	case 2:
		accountSettings.callsWidth2 = width;
		break;
	case 3:
		accountSettings.callsWidth3 = width;
		break;
	case 4:
		accountSettings.callsWidth4 = width;
		break;
	}
	mainDlg->AccountSettingsPendingSave();
	*pResult = 0;
}


void Calls::OnBnClickedOk()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		DefaultItemAction(list->GetNextSelectedItem(pos));
	}
}

void Calls::DefaultItemAction(int i)
{
	if (accountSettings.defaultAction.IsEmpty()) {
		if (accountSettings.singleMode) {
			OnMenuCall();
		}
		else {
			OnMenuChat();
		}
	}
	else {
		if (accountSettings.defaultAction == _T("call")) {
			OnMenuCall();
		}
#ifdef _GLOBAL_VIDEO
		else if (accountSettings.defaultAction == _T("video")) {
			OnMenuCallVideo();
		}
#endif
		else {
			OnMenuChat();
		}
	}
}

void Calls::OnBnClickedCancel()
{
	mainDlg->ShowWindow(SW_HIDE);
}

void Calls::OnFilterValueChange()
{
	CallsClear();
	CallsLoad();
}

bool Calls::isFiltered(Call* pCall) {
	CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
	CString str;
	edit->GetWindowText(str);
	if (!str.IsEmpty()) {
		if (!pCall) {
			return true;
		}
		str.MakeLower();
		CString name = pCall->name;
		CString number = pCall->number;
		name.MakeLower();
		number.MakeLower();
		if (name.Find(str) == -1 && number.Find(str) == -1) {
			return true;
		}
	}
	return false;
}

void Calls::filterReset()
{
	CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
	edit->SetWindowText(_T(""));
}

LRESULT Calls::OnContextMenu(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	POINT pt = { x, y };
	RECT rc;
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	int selectedItem = -1;
	int selectedItem2 = -1;
	if (pos) {
		selectedItem = list->GetNextSelectedItem(pos);
	}
	if (selectedItem != -1) {
		selectedItem2 = list->GetNextSelectedItem(pos);
	}
	if (x != -1 || y != -1) {
		ScreenToClient(&pt);
		GetClientRect(&rc);
		if (!PtInRect(&rc, pt)) {
			x = y = -1;
		}
	}
	else {
		if (selectedItem != -1) {
			list->GetItemPosition(selectedItem, &pt);
			list->ClientToScreen(&pt);
			x = 40 + pt.x;
			y = 8 + pt.y;
		}
		else {
			::ClientToScreen((HWND)wParam, &pt);
			x = 10 + pt.x;
			y = 26 + pt.y;
		}
	}
	if (x != -1 || y != -1) {
		CMenu menu;
		if (menu.LoadMenu(IDR_MENU_CONTACT)) {
			CMenu* tracker = menu.GetSubMenu(0);
			TranslateMenu(tracker->m_hMenu);
			bool disabled = selectedItem == -1;
			if (!disabled) {
				POSITION pos = list->GetFirstSelectedItemPosition();
				int i = list->GetNextSelectedItem(pos);
				Call* pCall = (Call*)list->GetItemData(i);
				if (mainDlg->pageContacts->FindContact(pCall->number)) {
					disabled = true;
				}
			}
			tracker->ModifyMenu(ID_ADD, MF_BYCOMMAND | (disabled ? MF_DISABLED : 0), ID_ADD, Translate(_T("Add Contact")));
			tracker->RemoveMenu(ID_EDIT, 0);
			if (selectedItem != -1) {
				if (selectedItem2 == -1) {
					tracker->EnableMenuItem(ID_CALL, FALSE);
#ifdef _GLOBAL_VIDEO
					tracker->EnableMenuItem(ID_VIDEOCALL, FALSE);
#endif
					tracker->EnableMenuItem(ID_CHAT, FALSE);
					tracker->EnableMenuItem(ID_COPY, FALSE);
				}
				else {
					tracker->EnableMenuItem(ID_CALL, TRUE);
#ifdef _GLOBAL_VIDEO
					tracker->EnableMenuItem(ID_VIDEOCALL, TRUE);
#endif
					tracker->EnableMenuItem(ID_CHAT, TRUE);
					tracker->EnableMenuItem(ID_COPY, TRUE);
				}
				tracker->EnableMenuItem(ID_DELETE, FALSE);
			}
			else {
				tracker->EnableMenuItem(ID_CALL, TRUE);
#ifdef _GLOBAL_VIDEO
				tracker->EnableMenuItem(ID_VIDEOCALL, TRUE);
#endif
				tracker->EnableMenuItem(ID_CHAT, TRUE);
				tracker->EnableMenuItem(ID_COPY, TRUE);
				tracker->EnableMenuItem(ID_DELETE, TRUE);
			}
			tracker->AppendMenu(0, MF_SEPARATOR);
			tracker->AppendMenu(MF_STRING, ID_EXPORT, Translate(_T("Export")));
#ifdef _GLOBAL_VIDEO
			if (accountSettings.disableVideo) {
				tracker->RemoveMenu(ID_VIDEOCALL, MF_BYCOMMAND);
			}
#endif
			if (accountSettings.disableMessaging) {
				tracker->RemoveMenu(ID_CHAT, MF_BYCOMMAND);
			}
			if (tracker->GetMenuItemCount() == 3) {
				tracker->RemoveMenu(0, MF_BYPOSITION);
			}
			tracker->TrackPopupMenu(0, x, y, this);
		}
		return TRUE;
	}
	return DefWindowProc(WM_CONTEXTMENU, wParam, lParam);
}

void Calls::MessageDlgOpen(BOOL isCall, BOOL hasVideo)
{
	if (accountSettings.singleMode && mainDlg->messagesDlg->GetCallsCount() && isCall) {
		mainDlg->GotoTab(0);
		return;
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		int i = list->GetNextSelectedItem(pos);
		Call* pCall = (Call*)list->GetItemData(i);
		if (isCall) {
			mainDlg->MakeCall(pCall->number, hasVideo, false, pCall->type != MSIP_CALL_OUT);
		}
		else {
			mainDlg->MessagesOpen(pCall->number, false, pCall->type != MSIP_CALL_OUT);
		}
	}
}

void Calls::OnNMDblclkCalls(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	if (pNMItemActivate->iItem != -1) {
		DefaultItemAction(pNMItemActivate->iItem);
	}
	*pResult = 0;
}

void Calls::OnMenuCall()
{
	MessageDlgOpen(TRUE);
}

#ifdef _GLOBAL_VIDEO
void Calls::OnMenuCallVideo()
{
	MessageDlgOpen(TRUE, TRUE);
}
#endif

void Calls::OnMenuChat()
{
	if (!accountSettings.disableMessaging) {
		MessageDlgOpen();
	}
}

void Calls::OnMenuAdd()
{
	mainDlg->pageContacts->OnMenuAdd();
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	int i = list->GetNextSelectedItem(pos);
	Call* pCall = (Call*)list->GetItemData(i);
	Contact contact;
	contact.number = pCall->number;
	contact.name = pCall->name;
	mainDlg->pageContacts->addDlg->Load(&contact);
}

void Calls::OnMenuCopy()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		int i = list->GetNextSelectedItem(pos);
		Call* pCall = (Call*)list->GetItemData(i);
		mainDlg->CopyStringToClipboard(pCall->number);
	}
}

void Calls::OnMenuDelete()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_CALLS);
	POSITION pos = pList->GetFirstSelectedItemPosition();
	while (pos) {
		Delete(pList->GetNextSelectedItem(pos));
		pos = pList->GetFirstSelectedItemPosition();
	}
}

void Calls::OnMenuExport()
{
	TCHAR szFilters[] = _T("CSV Files (*.csv)|*.csv|XML Files (*.xml)|*.xml||");
	CFileDialog dlgFile(FALSE, _T("csv"), _T("Calls"), OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, szFilters, this);
	if (dlgFile.DoModal() == IDOK) {
		if (isFiltered()) {
			filterReset();
		}
		CString filename = dlgFile.GetPathName();
		if (dlgFile.m_ofn.nFilterIndex == 2) {
			if (dlgFile.GetFileExt().IsEmpty()) {
				filename.Append(_T(".xml"));
			}
			CMarkup xml;
			xml.AddElem(_T("calls"));
			xml.IntoElem();
			CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
			int count = list->GetItemCount();
			for (int i = 0; i < count; i++) {
				Call* pCall = (Call*)list->GetItemData(i);
				xml.AddElem(_T("call"));
				xml.AddAttrib(_T("type"), pCall->type == MSIP_CALL_OUT ? _T("out") : (pCall->type == MSIP_CALL_IN ? _T("in") :
					(pCall->type == MSIP_CALL_MISS ? _T("miss") : _T("else"))
					));
				xml.AddAttrib(_T("name"), pCall->name);
				xml.AddAttrib(_T("number"), pCall->number);
				CString str;
				str.Format(_T("%d"), pCall->time);
				xml.AddAttrib(_T("time"), str);
				str.Format(_T("%d"), pCall->duration);
				xml.AddAttrib(_T("duration"), str);
				xml.AddAttrib(_T("info"), pCall->info);
			}
			CFile file;
			CFileException fileException;
			if (file.Open(filename, CFile::modeCreate | CFile::modeWrite, &fileException)) {
				CStringA str = "<?xml version=\"1.0\"?>\r\n";
				str.Append(Utf8EncodeUcs2(xml.GetDoc()));
				file.Write(str.GetBuffer(), str.GetLength());
				file.Close();
			}
		}
		else {
			if (dlgFile.GetFileExt().IsEmpty()) {
				filename.Append(_T(".csv"));
			}
			CCSVFile CSVFile;
			CSVFile.SetCodePage(CP_UTF8);
			if (CSVFile.Open(filename, CCSVFile::modeCreate | CCSVFile::modeWrite | CFile::typeText | CFile::shareExclusive)) {
				CStringArray arr;
				arr.Add(_T("Type"));
				arr.Add(_T("Name"));
				arr.Add(_T("Number"));
				arr.Add(_T("Time"));
				arr.Add(_T("Duration"));
				arr.Add(_T("Info"));
				CSVFile.WriteData(arr);
				CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
				int count = list->GetItemCount();
				for (int i = 0; i < count; i++) {
					Call* pCall = (Call*)list->GetItemData(i);
					arr.RemoveAll();
					arr.Add(pCall->type == MSIP_CALL_OUT ? _T("out") : (pCall->type == MSIP_CALL_IN ? _T("in") :
						(pCall->type == MSIP_CALL_MISS ? _T("miss") : _T("else"))
						));
					arr.Add(pCall->name);
					arr.Add(pCall->number);
					CString str;
					str.Format(_T("%d"), pCall->time);
					arr.Add(str);
					str.Format(_T("%d"), pCall->duration);
					arr.Add(str);
					arr.Add(pCall->info);
					CSVFile.WriteData(arr);
				}
				CSVFile.Close();
			}
		}
	}
}


void Calls::Delete(int i)
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	Call* pCall = (Call*)list->GetItemData(i);
	pCall->number = _T("");
	CallSave(pCall);
	delete pCall;
	list->DeleteItem(i);
}

void Calls::DeleteAll()
{
	CallsClear();
	WritePrivateProfileSection(_T("Calls"), NULL, accountSettings.iniFile);
}

void Calls::Add(pj_str_t id, CString number, CString name, int type, call_user_data *user_data)
{
	bool isPRID = true;
	CString numberLocal;
	if (user_data) {
		user_data->CS.Lock();
		numberLocal = user_data->callerID;
		user_data->CS.Unlock();
	}
	if (numberLocal.IsEmpty()) {
		isPRID = false;
		numberLocal = number;
	}
	SIPURI sipuri;
	MSIP::ParseSIPURI(numberLocal, &sipuri);
	if (isPRID) {
		sipuri.commands.Empty();
		sipuri.parameters.Empty();
		//-- fix wrong domain
		if (!sipuri.user.IsEmpty() && accountSettings.accountId) {
			if (MSIP::IsIP(MSIP::RemovePort(sipuri.domain))) {
				sipuri.domain = get_account_domain();
			}
		}
		//--
	}

	numberLocal.Empty();
	if (sipuri.user.IsEmpty()) {
		numberLocal = sipuri.domain;
		numberLocal.Append(sipuri.parameters);
		numberLocal.Append(sipuri.commands);
	}
	else {
		numberLocal = sipuri.user;
		if (sipuri.parameters.IsEmpty() && (get_account_domain() == sipuri.domain || sipuri.domain.IsEmpty())) {
			numberLocal.Append(sipuri.commands);
		}
		else {
			numberLocal.Append(_T("@"));
			numberLocal.Append(sipuri.domain);
			numberLocal.Append(sipuri.parameters);
			numberLocal.Append(sipuri.commands);
		}
		if (!accountSettings.account.dialingPrefix.IsEmpty()) {
			if (numberLocal.Find(accountSettings.account.dialingPrefix) == 0) {
				numberLocal = numberLocal.Mid(accountSettings.account.dialingPrefix.GetLength());
			}
		}
	}

	CString callId = MSIP::PjToStr(&id);
	int i = Get(callId);
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	if (i == -1) {
		ReloadTime();
			Call* pCall = new Call();
			pCall->id = callId;
			pCall->number = numberLocal;
			pCall->name = name;
			pCall->type = type;
			pCall->time = CTime::GetCurrentTime().GetTime();
			pCall->duration = 0;
			pCall->key = GetNextKey();
			Insert(pCall);
			CallSave(pCall);
			SaveKey();
	}
	else {
		bool changed = false;
		Call* pCall = (Call*)list->GetItemData(i);
		if (pCall->number != numberLocal) {
			pCall->number = numberLocal;
			list->SetItemText(i, MSIP_CALLS_COL_NUMBER, pCall->number);
			changed = true;
		}
		if (pCall->name != name) {
			pCall->name = name;
			list->SetItemText(i, MSIP_CALLS_COL_NAME, pCall->name);
			changed = true;
		}
		if (pCall->type != type) {
			pCall->type = type;
			list->SetItem(i, 0, LVIF_IMAGE, NULL, type, 0, 0, 0);
			changed = true;
		}
		if (changed) {
			CallSave(pCall);
		}
	}
}

void Calls::SetName(pj_str_t id, CString name) {
	CString callId = MSIP::PjToStr(&id);
	int i = Get(callId);
	if (i != -1) {
		CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
		Call* pCall = (Call*)list->GetItemData(i);
		pCall->name = name;
		list->SetItemText(i, MSIP_CALLS_COL_NAME, name);
		CallSave(pCall);
	}
}

void Calls::SetDuration(pj_str_t id, int sec) {
	CString callId = MSIP::PjToStr(&id);
	int i = Get(callId);
	if (i != -1) {
		CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
		Call* pCall = (Call*)list->GetItemData(i);
		pCall->duration = sec;
		list->SetItemText(i, MSIP_CALLS_COL_DURATION, MSIP::GetDuration(pCall->duration));
		CallSave(pCall);
	}
}

void Calls::SetInfo(pj_str_t id, CString str) {
	CString callId = MSIP::PjToStr(&id);
	int i = Get(callId);
	if (i != -1) {
		CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
		Call* pCall = (Call*)list->GetItemData(i);
		pCall->info = str;
		list->SetItemText(i, MSIP_CALLS_COL_INFO, str);
		CallSave(pCall);
	}
}

int Calls::Get(CString id)
{
	if (isFiltered()) {
		filterReset();
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	int count = list->GetItemCount();
	for (int i = 0; i < count; i++)
	{
		Call* pCall = (Call*)list->GetItemData(i);
		if (pCall->id == id) {
			return i;
		}
	}
	return -1;
}


void Calls::Insert(Call* pCall, int pos)
{
	if (isFiltered(pCall)) {
		return;
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	int i = list->InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE, pos, pCall->name, 0, 0, pCall->type, (LPARAM)pCall);
	list->SetItemText(i, MSIP_CALLS_COL_NUMBER, pCall->number);
	list->SetItemText(i, MSIP_CALLS_COL_TIME, FormatTime(pCall->time));
	list->SetItemText(i, MSIP_CALLS_COL_DURATION, MSIP::GetDuration(pCall->duration));
	list->SetItemText(i, MSIP_CALLS_COL_INFO, pCall->info);
}

void Calls::CallsClear()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
	int count = list->GetItemCount();
	for (int i = 0; i < count; i++) {
		Call* pCall = (Call*)list->GetItemData(i);
		delete pCall;
	}
	list->DeleteAllItems();
}

CString Calls::FormatTime(int time, CTime* pTimeNow)
{
	CTime timeNow;
	if (!pTimeNow) {
		timeNow = CTime::GetCurrentTime();
		pTimeNow = &timeNow;
	}
	if (!lastDay) {
		lastDay = pTimeNow->GetDay();
	}
	CTime timeCall(time);
	return timeCall.Format(
		pTimeNow->GetYear() == timeCall.GetYear() &&
		pTimeNow->GetMonth() == timeCall.GetMonth() &&
		pTimeNow->GetDay() == timeCall.GetDay()
		? _T("%X") : _T("%c")
	);
}

void Calls::ReloadTime()
{
	CTime timeNow = CTime::GetCurrentTime();
	if (lastDay && lastDay != timeNow.GetDay()) {
		lastDay = timeNow.GetDay();
		CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CALLS);
		int count = list->GetItemCount();
		for (int i = 0; i < count; i++) {
			Call* pCall = (Call*)list->GetItemData(i);
			CTime timeCall(pCall->time);
			list->SetItemText(i, MSIP_CALLS_COL_TIME, FormatTime(pCall->time, &timeNow));
		}
	}
}


void Calls::CallSave(Call* pCall)
{
	CString key;
	// pCall->number == "" means delete
	CString data = !pCall->number.IsEmpty() ? CallEncode(pCall) : _T("null");
	key.Format(_T("%d"), pCall->key);
	WritePrivateProfileString(_T("Calls"), key, data, accountSettings.iniFile);
}

void Calls::CallsLoad()
{
	CString key;
	CString val;
	LPTSTR ptr = val.GetBuffer(255);
	int prevTime = 0;
	int pos = -1;
	int inserted = 0;
	lastKey = -1;
	int maxTime = 0;
	int currTime = CTime::GetCurrentTime().GetTime();
	int i = 0;
	int callsLastKey = GetNextKey(true);
	while (true) {
		key.Format(_T("%d"), i);
		if (GetPrivateProfileString(_T("Calls"), key, NULL, ptr, 256, accountSettings.iniFile)) {
			if (val != _T("null")) {
				Call* pCall = new Call();
				CallDecode(ptr, pCall);
				if (pCall->time > maxTime) {
					maxTime = pCall->time;
					lastKey = i;
				}
				bool skip = false;
				if (isFiltered(pCall)) {
					skip = true;
					delete pCall;
				}
				if (!skip) {
					pCall->key = i;
					if (pos == -1) {
						if (prevTime > pCall->time) {
							pos = inserted;
						}
					}
					if (pos == -1) {
						Insert(pCall);
						prevTime = pCall->time;
					}
					else {
						Insert(pCall, pos);
					}
					inserted++;
				}
			}
		}
		else {
			if (callsLastKey <= i) {
				break;
			}
		}
		i++;
	}
	m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
}

CString Calls::CallEncode(Call* pCall)
{
	CString data;
	data.Format(_T("%s;%s;%d;%d;%d;%s"), pCall->number, pCall->name, pCall->type, pCall->time, pCall->duration, pCall->info);
	return data;
}

void Calls::CallDecode(CString str, Call* pCall)
{
	pCall->number = str;
	pCall->name = pCall->number;
	pCall->type = 0;
	pCall->time = 0;
	pCall->duration = 0;

	CString rab;
	int begin;
	int end;
	begin = 0;
	end = str.Find(';', begin);

	if (end != -1)
	{
		pCall->number = str.Mid(begin, end - begin);
		begin = end + 1;
		end = str.Find(';', begin);
		if (end != -1)
		{
			pCall->name = str.Mid(begin, end - begin);
			begin = end + 1;
			end = str.Find(';', begin);
			if (end != -1)
			{
				pCall->type = atoi(CStringA(str.Mid(begin, end - begin)));
				if (pCall->type > 3 || pCall->type < 0) {
					pCall->type = 0;
				}
				begin = end + 1;
				end = str.Find(';', begin);
				if (end != -1)
				{
					pCall->time = atoi(CStringA(str.Mid(begin, end - begin)));
					begin = end + 1;
					end = str.Find(';', begin);
					if (end != -1)
					{
						pCall->duration = atoi(CStringA(str.Mid(begin, end - begin)));
						begin = end + 1;
						end = str.Find(';', begin);
						if (end != -1)
						{
							pCall->info = str.Mid(begin, end - begin);
							begin = end + 1;
							end = str.Find(';', begin);
						}
						else {
							pCall->info = str.Mid(begin);
						}
					}
				}
			}
		}
	}
}

int Calls::GetNextKey(bool noInc)
{
	CString str;
	LPTSTR ptr;
	CString section;
	section = _T("Settings");

	ptr = str.GetBuffer(255);
	GetPrivateProfileString(section, _T("callsLastKey"), _T("-1"), ptr, 256, accountSettings.iniFile);
	str.ReleaseBuffer();
	int key = _wtoi(str);
	if (key != -1) {
		lastKey = key;
	}
	if (!noInc) {
		lastKey++;
		if (lastKey >= 1000) {
			lastKey = 0;
		}
	}
	return lastKey;
}

void Calls::SaveKey()
{
	CString str;
	LPTSTR ptr;
	CString section;
	section = _T("Settings");

	str.Format(_T("%d"), lastKey);
	WritePrivateProfileString(section, _T("callsLastKey"), str, accountSettings.iniFile);
}
