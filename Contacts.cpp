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

#include "Contacts.h"
#include "microsip.h"
#include "settings.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include "mainDlg.h"
#include "utf.h"
#include "langpack.h"
#include "CSVFile.h"
#include "Markup.h"
#include "Transfer.h"
#include "afxinet.h"
#include "MessageBoxX.h"

static UINT_PTR blinkTimer = NULL;
static bool blinkState = false;

Contacts::Contacts(CWnd* pParent /*=NULL*/)
	: CBaseDialog(Contacts::IDD, pParent)
{
	Create(IDD, pParent);
}

Contacts::~Contacts(void)
{
}

BOOL Contacts::OnInitDialog()
{
	CBaseDialog::OnInitDialog();

	AutoMove(IDC_CONTACTS, 0, 0, 100, 100);
	AutoMove(IDC_SEARCH_PICTURE, 0, 100, 0, 0);
	AutoMove(IDC_FILER_VALUE, 0, 100, 100, 0);

	TranslateDialog(this->m_hWnd);

	addDlg = new AddDlg(this);

	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	//list->SetExtendedStyle(list->GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS);
	list->SetExtendedStyle(list->GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	list->SetImageList(mainDlg->imageListStatus, LVSIL_SMALL);

	list->InsertColumn(0, Translate(_T("Name")), LVCFMT_LEFT, accountSettings.contactsWidth0 > 0 ? accountSettings.contactsWidth0 : 160);
	list->InsertColumn(1, Translate(_T("Number")), LVCFMT_LEFT, accountSettings.contactsWidth1 > 0 ? accountSettings.contactsWidth1 : 100);
	list->InsertColumn(2, Translate(_T("Info")), LVCFMT_LEFT, accountSettings.contactsWidth2 > 0 ? accountSettings.contactsWidth2 : 120);
	ContactsLoad();

	return TRUE;
}

void Contacts::OnCreated()
{
	m_SortItemsExListCtrl.SetSortColumn(0, true);
}

void Contacts::PostNcDestroy()
{
	CBaseDialog::PostNcDestroy();
	mainDlg->pageContacts = NULL;
	delete this;
}

void Contacts::DoDataExchange(CDataExchange* pDX)
{
	CBaseDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CONTACTS, m_SortItemsExListCtrl);
}

BEGIN_MESSAGE_MAP(Contacts, CBaseDialog)
	ON_WM_TIMER()
	ON_NOTIFY(HDN_ENDTRACK, 0, OnEndtrack)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
	ON_EN_CHANGE(IDC_FILER_VALUE, OnFilterValueChange)
	ON_COMMAND(ID_CALL_PICKUP, OnMenuCallPickup)
	ON_COMMAND(ID_CALL, OnMenuCall)
	ON_COMMAND(ID_CALL_PHONE, OnMenuCallPhone)
	ON_COMMAND(ID_CALL_MOBILE, OnMenuCallMobile)
	ON_COMMAND(ID_CHAT, OnMenuChat)
	ON_COMMAND(ID_ADD, OnMenuAdd)
	ON_COMMAND(ID_EDIT, OnMenuEdit)
	ON_COMMAND(ID_COPY, OnMenuCopy)
	ON_COMMAND(ID_DELETE, OnMenuDelete)
	ON_COMMAND(ID_IMPORT, OnMenuImport)
	ON_COMMAND(ID_EXPORT, OnMenuExport)
	ON_MESSAGE(WM_CONTEXTMENU, OnContextMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CONTACTS, &Contacts::OnNMDblclkContacts)
#ifdef _GLOBAL_VIDEO
	ON_COMMAND(ID_VIDEOCALL, OnMenuCallVideo)
#endif
END_MESSAGE_MAP()

void Contacts::OnTimer(UINT_PTR TimerVal)
{
	if (TimerVal == IDT_TIMER_CONTACTS_BLINK) {
		OnTimerContactsBlink();
	}
}

BOOL Contacts::PreTranslateMessage(MSG* pMsg)
{
	BOOL catched = FALSE;
	if (pMsg->message == WM_KEYDOWN) {
		if (pMsg->wParam == VK_ESCAPE) {
			CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
			if (edit == GetFocus()) {
				catched = TRUE;
				if (isFiltered()) {
					filterReset();
				}
			}
		}
		if (pMsg->wParam == VK_DELETE) {
			if ((CListCtrl*)GetDlgItem(IDC_CONTACTS) == GetFocus()) {
				catched = TRUE;
				OnMenuDelete();
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

void Contacts::OnEndtrack(NMHDR* pNMHDR, LRESULT* pResult)
{
	HD_NOTIFY* phdn = (HD_NOTIFY*)pNMHDR;
	int width = phdn->pitem->cxy;
	switch (phdn->iItem) {
	case 0:
		accountSettings.contactsWidth0 = width;
		break;
	case 1:
		accountSettings.contactsWidth1 = width;
		break;
	case 2:
		accountSettings.contactsWidth2 = width;
		break;
	}
	mainDlg->AccountSettingsPendingSave();
	*pResult = 0;
}

void Contacts::OnBnClickedOk()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		DefaultItemAction(list->GetNextSelectedItem(pos));
	}
}

void Contacts::DefaultItemAction(int i)
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	Contact* contact = (Contact*)list->GetItemData(i);
	bool pickup = contact->ringing;
	pickup = false;
	if (pickup) {
		OnMenuCallPickup();
	}
	else {
		MessagesContact* messagesContact = mainDlg->messagesDlg->GetMessageContact();
		if (messagesContact && messagesContact->callId != -1) {
			mainDlg->OpenTransferDlg(mainDlg, MSIP_ACTION_TRANSFER, PJSUA_INVALID_ID, contact);
		}
		else {
			if (accountSettings.defaultAction.IsEmpty()) {
				MessageDlgOpen(accountSettings.singleMode);
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
	}
}

void Contacts::OnBnClickedCancel()
{
	mainDlg->ShowWindow(SW_HIDE);
}

bool Contacts::isFiltered(Contact* contact) {
	CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
	CString str;
	edit->GetWindowText(str);
	if (!str.IsEmpty()) {
		if (!contact) {
			return true;
		}
		str.MakeLower();
		CString name = contact->name;
		CString number = contact->number;
		name.MakeLower();
		number.MakeLower();
		if (name.Find(str) == -1 && number.Find(str) == -1) {
			return true;
		}
	}
	return false;
}

void Contacts::filterReset()
{
	CEdit* edit = (CEdit*)GetDlgItem(IDC_FILER_VALUE);
	edit->SetWindowText(_T(""));
}

void Contacts::OnFilterValueChange()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	list->DeleteAllItems();
	POSITION pos = contacts.GetHeadPosition();
	while (pos) {
		Contact* contact = contacts.GetNext(pos);
		if (!isFiltered(contact)) {
			ListAppend(list, contact);
		}
	}
	m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
}

LRESULT Contacts::OnContextMenu(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	POINT pt = { x, y };
	RECT rc;
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	int selectedItem = -1;
	if (pos) {
		selectedItem = list->GetNextSelectedItem(pos);
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
			y = 10 + pt.y;
		}
	}
	if (x != -1 || y != -1) {
		CMenu menu;
		menu.LoadMenu(IDR_MENU_CONTACT);
		CMenu* tracker = menu.GetSubMenu(0);
		TranslateMenu(tracker->m_hMenu);
		if (selectedItem != -1) {
			Contact* pContact = (Contact*)list->GetItemData(selectedItem);
			if (pContact->ringing) {
				tracker->InsertMenu(ID_CALL, 0, ID_CALL_PICKUP, Translate(_T("Call Pickup")));
				tracker->InsertMenu(ID_CALL, MF_SEPARATOR);
			}
			//--
			CMenu numbersMenu;
			numbersMenu.CreatePopupMenu();
			numbersMenu.AppendMenu(MF_STRING, ID_CALL, pContact->number);
			if (!pContact->phone.IsEmpty() && pContact->phone != pContact->number) {
				numbersMenu.AppendMenu(MF_STRING, ID_CALL_PHONE, pContact->phone);
			}
			if (!pContact->mobile.IsEmpty() && pContact->mobile != pContact->number) {
				numbersMenu.AppendMenu(MF_STRING, ID_CALL_MOBILE, pContact->mobile);
			}
			if (numbersMenu.GetMenuItemCount() > 1) {
				tracker->ModifyMenu(ID_CALL, MF_BYCOMMAND | MF_POPUP, (UINT_PTR)numbersMenu.m_hMenu, Translate(_T("Call")));
			}
			//--
			tracker->EnableMenuItem(ID_CALL, FALSE);
#ifdef _GLOBAL_VIDEO
			tracker->EnableMenuItem(ID_VIDEOCALL, FALSE);
#endif
			tracker->EnableMenuItem(ID_CHAT, FALSE);
			tracker->EnableMenuItem(ID_COPY, FALSE);
			tracker->EnableMenuItem(ID_EDIT, FALSE);
			tracker->EnableMenuItem(ID_DELETE, FALSE);
		}
		else {
			tracker->EnableMenuItem(ID_CALL, TRUE);
#ifdef _GLOBAL_VIDEO
			tracker->EnableMenuItem(ID_VIDEOCALL, TRUE);
#endif
			tracker->EnableMenuItem(ID_CHAT, TRUE);
			tracker->EnableMenuItem(ID_COPY, TRUE);
			tracker->EnableMenuItem(ID_EDIT, TRUE);
			tracker->EnableMenuItem(ID_DELETE, TRUE);
		}
		tracker->AppendMenu(0, MF_SEPARATOR);
		tracker->AppendMenu(MF_STRING, ID_IMPORT, Translate(_T("Import")));
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
		return TRUE;
	}
	return DefWindowProc(WM_CONTEXTMENU, wParam, lParam);
}

void Contacts::MessageDlgOpen(BOOL isCall, BOOL hasVideo, BYTE index)
{
	if (accountSettings.singleMode && mainDlg->messagesDlg->GetCallsCount() && isCall) {
		mainDlg->GotoTab(0);
		return;
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		int i = list->GetNextSelectedItem(pos);
		Contact* pContact = (Contact*)list->GetItemData(i);
		CString number = pContact->number;
		if (index == 1 && !pContact->phone.IsEmpty()) {
			number = pContact->phone;
		}
		if (index == 2 && !pContact->mobile.IsEmpty()) {
			number = pContact->mobile;
		}
		if (isCall) {
			mainDlg->MakeCall(number, hasVideo);
		}
		else {
			mainDlg->MessagesOpen(number);
		}
	}
}

void Contacts::OnNMDblclkContacts(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	if (pNMItemActivate->iItem != -1) {
		DefaultItemAction(pNMItemActivate->iItem);
	}
	*pResult = 0;
}

void Contacts::OnMenuCallPickup()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		int i = list->GetNextSelectedItem(pos);
		Contact* pContact = (Contact*)list->GetItemData(i);
		CString commands;
		CString numberFormated = FormatNumber(pContact->number, &commands);
		SIPURI sipuri;
		MSIP::ParseSIPURI(numberFormated, &sipuri);
		CString str;
		str = _T(_GLOBAL_CALL_PICKUP);
		str.Append(sipuri.user);
		sipuri.user = str;
		numberFormated = MSIP::BuildSIPURI(&sipuri);
		mainDlg->messagesDlg->CallMake(numberFormated);
	}
}

void Contacts::OnMenuCall()
{
	MessageDlgOpen(TRUE);
}

void Contacts::OnMenuCallPhone()
{
	MessageDlgOpen(TRUE, 0, 1);
}

void Contacts::OnMenuCallMobile()
{
	MessageDlgOpen(TRUE, 0, 2);
}

#ifdef _GLOBAL_VIDEO
void Contacts::OnMenuCallVideo()
{
	MessageDlgOpen(TRUE, TRUE);
}
#endif

void Contacts::OnMenuChat()
{
	if (!accountSettings.disableMessaging) {
		MessageDlgOpen();
	}
}

void Contacts::OnMenuAdd()
{
	if (!addDlg->IsWindowVisible()) {
		addDlg->ShowWindow(SW_SHOW);
	}
	else {
		addDlg->SetForegroundWindow();
	}
	Contact contact;
	addDlg->Load(&contact);
}

void Contacts::OnMenuEdit()
{
	OnMenuAdd();
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	int i = list->GetNextSelectedItem(pos);
	Contact* pContact = (Contact*)list->GetItemData(i);
	addDlg->Load(pContact);
}

void Contacts::OnMenuCopy()
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		int i = list->GetNextSelectedItem(pos);
		Contact* pContact = (Contact*)list->GetItemData(i);
		mainDlg->CopyStringToClipboard(pContact->number);
	}
}

void Contacts::OnMenuDelete()
{
	CList<CString, CString> contactsSelected;
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = list->GetFirstSelectedItemPosition();
	if (pos) {
		if (MessageBox(Translate(_T("Are you sure you want to delete?")), Translate(_T("Delete contact")), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
			return;
		}
		while (pos) {
			Contact* pContact = (Contact*)list->GetItemData(list->GetNextSelectedItem(pos));
			contactsSelected.AddTail(pContact->number);
		}
		if (isFiltered()) {
			filterReset();
		}
		int count = list->GetItemCount();
		bool deleted = false;
		for (int i = 0; i < count; i++) {
			Contact* pContact = (Contact*)list->GetItemData(i);
			if (contactsSelected.Find(pContact->number)) {
				bool allow = true;
				if (allow) {
					ContactDelete(i);
					count--;
					i--;
					deleted = true;
				}
			}
		}
		if (deleted) {
			ContactsSave();
		}
	}
}

bool Contacts::Import(CString filename, CArray<ContactWithFields*>& contactsWithFields, bool directory)
{
	CCSVFile CSVFile;
	CSVFile.SetCodePage(CP_UTF8);
	if (CSVFile.Open(filename, CCSVFile::modeRead | CFile::typeText | CFile::shareDenyWrite)) {
		CStringArray arr;
		int nameIndex = -1;
		int numberIndex = -1;
		int firstnameIndex = -1;
		int lastnameIndex = -1;
		int phoneIndex = -1;
		int mobileIndex = -1;
		int emailIndex = -1;
		int addressIndex = -1;
		int cityIndex = -1;
		int stateIndex = -1;
		int zipIndex = -1;
		int commentIndex = -1;
		int idIndex = -1;
		int infoIndex = -1;
		int presenceIndex = -1;
		int directoryIndex = -1;
		int starredIndex = -1;
		bool header = true;
		ContactWithFields* contactWithFields;
		while (CSVFile.ReadData(arr)) {
			if (header) {
				for (int i = 0; i < arr.GetCount(); i++) {
					CString s = arr.GetAt(i);
					if (nameIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Name")) == 0) {
						nameIndex = i;
					}
					if (numberIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Number")) == 0 || arr.GetAt(i).CompareNoCase(_T("Primary Phone")) == 0 || arr.GetAt(i).CompareNoCase(_T("phone")) == 0)) {
						numberIndex = i;
					}
					if (firstnameIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("First Name")) == 0 || arr.GetAt(i).CompareNoCase(_T("Given Name")) == 0 || arr.GetAt(i).CompareNoCase(_T("first_name")) == 0)) {
						firstnameIndex = i;
					}
					if (lastnameIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Last Name")) == 0 || arr.GetAt(i).CompareNoCase(_T("Family Name")) == 0 || arr.GetAt(i).CompareNoCase(_T("last_name")) == 0)) {
						lastnameIndex = i;
					}
					if (phoneIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Phone Number")) == 0 || arr.GetAt(i).CompareNoCase(_T("Home Phone")) == 0 || arr.GetAt(i).CompareNoCase(_T("Phone 2 - Value")) == 0 || arr.GetAt(i).CompareNoCase(_T("home_number")) == 0)) {
						phoneIndex = i;
					}
					if (mobileIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Mobile Number")) == 0 || arr.GetAt(i).CompareNoCase(_T("Mobile Phone")) == 0 || arr.GetAt(i).CompareNoCase(_T("Phone 1 - Value")) == 0 || arr.GetAt(i).CompareNoCase(_T("mobile_number")) == 0)) {
						mobileIndex = i;
					}
					if (emailIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("E-mail Address")) == 0 || arr.GetAt(i).CompareNoCase(_T("E-mail 1 - Value")) == 0 || arr.GetAt(i).CompareNoCase(_T("email")) == 0)) {
						emailIndex = i;
					}
					if (addressIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Address")) == 0 || arr.GetAt(i).CompareNoCase(_T("Home Address")) == 0)) {
						addressIndex = i;
					}
					if (cityIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("City")) == 0 || arr.GetAt(i).CompareNoCase(_T("Home City")) == 0)) {
						cityIndex = i;
					}
					if (stateIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("State")) == 0 || arr.GetAt(i).CompareNoCase(_T("Home State")) == 0)) {
						stateIndex = i;
					}
					if (zipIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Postal Code")) == 0 || arr.GetAt(i).CompareNoCase(_T("Home Postal Code")) == 0)) {
						zipIndex = i;
					}
					if (commentIndex == -1 && (arr.GetAt(i).CompareNoCase(_T("Comment")) == 0 || arr.GetAt(i).CompareNoCase(_T("Notes")) == 0)) {
						commentIndex = i;
					}
					if (idIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Id")) == 0) {
						idIndex = i;
					}
					if (infoIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Info")) == 0) {
						infoIndex = i;
					}
					if (presenceIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Presence")) == 0) {
						presenceIndex = i;
					}
					if (directoryIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Directory")) == 0) {
						directoryIndex = i;
					}
					if (starredIndex == -1 && arr.GetAt(i).CompareNoCase(_T("Starred")) == 0) {
						starredIndex = i;
					}
				}
				if (numberIndex == -1 && phoneIndex == -1 && mobileIndex == -1) {
					AfxMessageBox(Translate(_T("Unknown format")));
					break;
				}
				header = false;
			}
			else {
				contactWithFields = new ContactWithFields();
				contactWithFields->contact.directory = directory;
				if (nameIndex != -1 && arr.GetCount() > nameIndex) {
					contactWithFields->fields.AddTail(_T("name"));
					contactWithFields->contact.name = arr.GetAt(nameIndex);
				}
				if (numberIndex != -1 && arr.GetCount() > numberIndex) {
					contactWithFields->fields.AddTail(_T("number"));
					contactWithFields->contact.number = arr.GetAt(numberIndex);
				}
				if (firstnameIndex != -1 && arr.GetCount() > firstnameIndex) {
					contactWithFields->fields.AddTail(_T("firstname"));
					contactWithFields->contact.firstname = arr.GetAt(firstnameIndex);
				}
				if (lastnameIndex != -1 && arr.GetCount() > lastnameIndex) {
					contactWithFields->fields.AddTail(_T("lastname"));
					contactWithFields->contact.lastname = arr.GetAt(lastnameIndex);
				}
				if (phoneIndex != -1 && arr.GetCount() > phoneIndex) {
					contactWithFields->fields.AddTail(_T("phone"));
					contactWithFields->contact.phone = arr.GetAt(phoneIndex);
				}
				if (mobileIndex != -1 && arr.GetCount() > mobileIndex) {
					contactWithFields->fields.AddTail(_T("mobile"));
					contactWithFields->contact.mobile = arr.GetAt(mobileIndex);
				}
				if (emailIndex != -1 && arr.GetCount() > emailIndex) {
					contactWithFields->fields.AddTail(_T("email"));
					contactWithFields->contact.email = arr.GetAt(emailIndex);
				}
				if (addressIndex != -1 && arr.GetCount() > addressIndex) {
					contactWithFields->fields.AddTail(_T("address"));
					contactWithFields->contact.address = arr.GetAt(addressIndex);
				}
				if (cityIndex != -1 && arr.GetCount() > cityIndex) {
					contactWithFields->fields.AddTail(_T("city"));
					contactWithFields->contact.city = arr.GetAt(cityIndex);
				}
				if (stateIndex != -1 && arr.GetCount() > stateIndex) {
					contactWithFields->fields.AddTail(_T("state"));
					contactWithFields->contact.state = arr.GetAt(stateIndex);
				}
				if (zipIndex != -1 && arr.GetCount() > zipIndex) {
					contactWithFields->fields.AddTail(_T("zip"));
					contactWithFields->contact.zip = arr.GetAt(zipIndex);
				}
				if (commentIndex != -1 && arr.GetCount() > commentIndex) {
					contactWithFields->fields.AddTail(_T("comment"));
					contactWithFields->contact.comment = arr.GetAt(commentIndex);
				}
				if (idIndex != -1 && arr.GetCount() > idIndex) {
					contactWithFields->fields.AddTail(_T("id"));
					contactWithFields->contact.id = arr.GetAt(idIndex);
				}
				if (infoIndex != -1 && arr.GetCount() > infoIndex) {
					contactWithFields->fields.AddTail(_T("info"));
					contactWithFields->contact.info = arr.GetAt(infoIndex);
				}
				if (presenceIndex != -1 && arr.GetCount() > presenceIndex) {
					contactWithFields->fields.AddTail(_T("presence"));
					contactWithFields->contact.presence = arr.GetAt(presenceIndex) == _T("1");
				}
				if (directoryIndex != -1 && arr.GetCount() > directoryIndex) {
					contactWithFields->fields.AddTail(_T("directory"));
					contactWithFields->contact.directory = arr.GetAt(directoryIndex) == _T("1");
				}
				if (starredIndex != -1 && arr.GetCount() > starredIndex) {
					contactWithFields->fields.AddTail(_T("starred"));
					contactWithFields->contact.starred = arr.GetAt(starredIndex) == _T("1");
				}
				if (ContactPrepare(&contactWithFields->contact)) {
					contactsWithFields.Add(contactWithFields);
				}
				else {
					delete contactWithFields;
				}
			}
		}
		CSVFile.Close();
		return true;
	}
	return false;
}

void Contacts::OnMenuImport()
{
	CFileDialog dlgFile(TRUE, _T("cvs"), 0, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("CSV Files (*.csv)|*.csv|"), this);
	if (dlgFile.DoModal() == IDOK) {
		if (isFiltered()) {
			filterReset();
		}
		CArray<ContactWithFields*> contactsWithFields;
		if (Import(dlgFile.GetPathName(), contactsWithFields)) {
			if (contactsWithFields.GetCount()) {
				ContactsAdd(&contactsWithFields);
				ContactWithFields* contactWithFields;
				for (int i = 0; i < contactsWithFields.GetCount(); i++) {
					contactWithFields = contactsWithFields.GetAt(i);
					delete contactWithFields;
				}
				m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
			}
		}
	}
}

void Contacts::OnMenuExport()
{
	TCHAR szFilters[] = _T("CSV Files (*.csv)|*.csv|XML Files (*.xml)|*.xml||");
	CFileDialog dlgFile(FALSE, _T("csv"), _T("Contacts"), OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, szFilters, this);
	if (dlgFile.DoModal() == IDOK) {
		CString filename = dlgFile.GetPathName();
		if (dlgFile.m_ofn.nFilterIndex == 2) {
			if (dlgFile.GetFileExt().IsEmpty()) {
				filename.Append(_T(".xml"));
			}
			CString source = accountSettings.pathRoaming;
			source.Append(_T("Contacts.xml"));
			CopyFile(source, filename, FALSE);
		}
		else {
			if (isFiltered()) {
				filterReset();
			}
			if (dlgFile.GetFileExt().IsEmpty()) {
				filename.Append(_T(".csv"));
			}
			CCSVFile CSVFile;
			CSVFile.SetCodePage(CP_UTF8);
			if (CSVFile.Open(filename, CCSVFile::modeCreate | CCSVFile::modeWrite | CFile::typeText | CFile::shareExclusive)) {
				CStringArray arr;
				arr.Add(_T("Name"));
				arr.Add(_T("Number"));
				arr.Add(_T("First Name"));
				arr.Add(_T("Last Name"));
				arr.Add(_T("Phone Number"));
				arr.Add(_T("Mobile Number"));
				arr.Add(_T("E-mail Address"));
				arr.Add(_T("Address"));
				arr.Add(_T("City"));
				arr.Add(_T("State"));
				arr.Add(_T("Postal Code"));
				arr.Add(_T("Comment"));
				arr.Add(_T("Id"));
				arr.Add(_T("Info"));
				arr.Add(_T("Presence"));
				arr.Add(_T("Directory"));
				arr.Add(_T("Starred"));
				CSVFile.WriteData(arr);
				CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
				int count = list->GetItemCount();
				for (int i = 0; i < count; i++) {
					Contact* contact = (Contact*)list->GetItemData(i);
					arr.RemoveAll();
					arr.Add(contact->name);
					arr.Add(contact->number);
					arr.Add(contact->firstname);
					arr.Add(contact->lastname);
					arr.Add(contact->phone);
					arr.Add(contact->mobile);
					arr.Add(contact->email);
					arr.Add(contact->address);
					arr.Add(contact->city);
					arr.Add(contact->state);
					arr.Add(contact->zip);
					arr.Add(contact->comment);
					arr.Add(contact->id);
					arr.Add(contact->info);
					arr.Add(contact->presence ? _T("1") : _T("0"));
					arr.Add(contact->directory ? _T("1") : _T("0"));
					arr.Add(contact->starred ? _T("1") : _T("0"));
					CSVFile.WriteData(arr);
				}
				CSVFile.Close();
			}
		}
	}
}

bool Contacts::ContactPrepare(Contact* contact)
{
	if (contact->number.IsEmpty()) {
		contact->number = contact->phone;
	}
	if (contact->number.IsEmpty()) {
		contact->number = contact->mobile;
	}
	if (contact->number.IsEmpty()) {
		return false;
	}
	if (contact->name.IsEmpty()) {
		if (contact->firstname != contact->lastname) {
			contact->name.Format(_T("%s %s"), contact->firstname, contact->lastname);
		}
		else {
			contact->name = contact->firstname;
		}
		contact->name.Trim();
	}
	if (contact->name.IsEmpty()) {
		contact->name = contact->number;
	}
	return true;
}

void Contacts::ContactCreate(CListCtrl* list, Contact* pContact, bool subscribe)
{
	Contact* contact = new Contact();
	contacts.AddTail(contact);
	contact->image = MSIP_CONTACT_ICON_DEFAULT;
	contact->name = pContact->name;
	contact->number = pContact->number;
	contact->firstname = pContact->firstname;
	contact->lastname = pContact->lastname;
	contact->phone = pContact->phone;
	contact->mobile = pContact->mobile;
	contact->email = pContact->email;
	contact->address = pContact->address;
	contact->city = pContact->city;
	contact->state = pContact->state;
	contact->zip = pContact->zip;
	contact->comment = pContact->comment;
	contact->id = pContact->id;
	if (!contact->presence || contact->info.IsEmpty()) {
		contact->info = pContact->info;
	}
	contact->presence = pContact->presence;
	contact->directory = pContact->directory;
	contact->starred = pContact->starred;
	ListAppend(list, contact, subscribe);
}

void Contacts::ListAppend(CListCtrl* list, Contact* contact, bool subscribe)
{
	int i = list->InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE, 0, contact->name, 0, 0, contact->image + (contact->starred ? 7 : 0), (LPARAM)contact);
	CString number = contact->number;
	list->SetItemText(i, 1, number);
	list->SetItemText(i, 2, Translate(contact->info.GetBuffer()));
	if (subscribe) {
		if (contact->presence) {
			mainDlg->SubsribeNumber(&contact->number);
		}
	}
}


bool Contacts::ContactUpdate(CListCtrl* list, int i, Contact* contact, Contact* newContact, CStringList* fields)
{
	bool changed = false;
	if (!fields || fields->Find(_T("name"))) {
		if (contact->name != newContact->name) {
			list->SetItemText(i, 0, newContact->name);
			contact->name = newContact->name;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("number"))) {
		if (contact->number != newContact->number) {
			bool presenceOrig = contact->presence;
			if (contact->presence) {
				contact->presence = false;
				PresenceUnsubsribeOne(contact);
			}
			list->SetItemText(i, 1, newContact->number);
			contact->number = newContact->number;
			if ((!fields || fields->Find(_T("presence")))) {
				contact->presence = newContact->presence;
			}
			else {
				contact->presence = presenceOrig;
			}
			if (contact->presence) {
				mainDlg->SubsribeNumber(&contact->number);
			}
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("firstname"))) {
		if (contact->firstname != newContact->firstname) {
			contact->firstname = newContact->firstname;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("lastname"))) {
		if (contact->lastname != newContact->lastname) {
			contact->lastname = newContact->lastname;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("phone"))) {
		if (contact->phone != newContact->phone) {
			contact->phone = newContact->phone;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("mobile"))) {
		if (contact->mobile != newContact->mobile) {
			contact->mobile = newContact->mobile;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("email"))) {
		if (contact->email != newContact->email) {
			contact->email = newContact->email;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("address"))) {
		if (contact->address != newContact->address) {
			contact->address = newContact->address;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("city"))) {
		if (contact->city != newContact->city) {
			contact->city = newContact->city;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("state"))) {
		if (contact->state != newContact->state) {
			contact->state = newContact->state;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("zip"))) {
		if (contact->zip != newContact->zip) {
			contact->zip = newContact->zip;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("comment"))) {
		if (contact->comment != newContact->comment) {
			contact->comment = newContact->comment;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("id"))) {
		if (contact->id != newContact->id) {
			contact->id = newContact->id;
			changed = true;
		}
	}

	if (!fields || fields->Find(_T("info"))) {
		if ((!contact->presence || contact->info.IsEmpty()) && contact->info != newContact->info) {
			list->SetItemText(i, 2, Translate(newContact->info.GetBuffer()));
			contact->info = newContact->info;
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("starred"))) {
		if (newContact->starred != contact->starred) {
			contact->starred = newContact->starred;
			list->SetItem(i, 0, LVIF_IMAGE, 0, contact->image + (contact->starred ? 7 : 0), 0, 0, 0);
			changed = true;
		}
	}
	if (!fields || fields->Find(_T("presence"))) {
		if (newContact->presence != contact->presence) {
			contact->presence = newContact->presence;
			if (contact->presence) {
				mainDlg->SubsribeNumber(&contact->number);
			}
			else {
				PresenceUnsubsribeOne(contact);
			}
			changed = true;
		}
	}
	return changed;
}

void Contacts::ContactsAdd(CArray<ContactWithFields*>* contactsWithFields, bool directory)
{
	if (isFiltered()) {
		filterReset();
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	bool changedAny = false;
	int count = list->GetItemCount();
	int countNew = contactsWithFields->GetCount();
	for (int i = 0; i < count; i++) {
		Contact* contact = (Contact*)list->GetItemData(i);
		bool found = false;
		for (int j = 0; j < countNew; j++) {
			ContactWithFields* contactWithFields = contactsWithFields->GetAt(j);
			if (contact->number == contactWithFields->contact.number) {
				contactWithFields->processed = true;
				found = true;
				if (ContactUpdate(list, i, contact, &contactWithFields->contact, &contactWithFields->fields)) {
					changedAny = true;
				}
			}
		}
		if (directory && contact->directory && !found) {
			ContactDelete(i);
			changedAny = true;
			count--;
			i--;
		}
	}
	for (int j = 0; j < countNew; j++) {
		ContactWithFields* contactWithFields = contactsWithFields->GetAt(j);
		if (!contactWithFields->processed) {
			ContactCreate(list, &contactWithFields->contact);
			changedAny = true;
		}
	}
	if (changedAny) {
		ContactsSave();
	}
}

bool Contacts::ContactAdd(Contact contact, BOOL save, BOOL load, CStringList* fields, CString oldNumber, bool manual)
{
	if (!ContactPrepare(&contact)) {
		return false;
	}
	if (save) {
		if (isFiltered()) {
			filterReset();
		}
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	if (!load) {
		bool found = false;
		bool changedAny = false;
		int count = list->GetItemCount();
		for (int i = 0; i < count; i++) {
			Contact* pContact = (Contact*)list->GetItemData(i);
			CString compareNumber = !oldNumber.IsEmpty() ? oldNumber : contact.number;
			if (pContact->number == compareNumber) {
				found = true;
				pContact->candidate = false;
				bool changed = ContactUpdate(list, i, pContact, &contact, fields);
				if (changed) {
					changedAny = true;
				}
			}
		}
		if (found) {
			if (save && changedAny) {
				ContactsSave();
			}
			if (manual && changedAny) {
				m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
			}
			return true;
		}
	}
	ContactCreate(list, &contact, !load);
	if (save) {
		ContactsSave();
	}
	if (manual) {
		m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
	}
	return true;
}

void Contacts::ContactDelete(int i)
{
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	Contact* contact = (Contact*)list->GetItemData(i);
	list->DeleteItem(i);
	ContactDeleteRaw(contact);
}

void Contacts::ContactDeleteRaw(Contact* contact)
{
	if (contact->presence) {
		contact->presence = false;
		PresenceUnsubsribeOne(contact);
	}
	POSITION pos = contacts.Find(contact);
	contacts.RemoveAt(pos);
	delete contact;
}

void Contacts::ContactsSave()
{
	if (isFiltered()) {
		filterReset();
	}
	CMarkup xml;
	xml.AddElem(_T("contacts"));
	xml.IntoElem();

	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	int count = list->GetItemCount();
	for (int i = 0; i < count; i++) {
		Contact* pContact = (Contact*)list->GetItemData(i);
		xml.AddElem(_T("contact"));
		xml.AddAttrib(_T("name"), pContact->name);
		xml.AddAttrib(_T("number"), pContact->number);
		xml.AddAttrib(_T("firstname"), pContact->firstname);
		xml.AddAttrib(_T("lastname"), pContact->lastname);
		xml.AddAttrib(_T("phone"), pContact->phone);
		xml.AddAttrib(_T("mobile"), pContact->mobile);
		xml.AddAttrib(_T("email"), pContact->email);
		xml.AddAttrib(_T("address"), pContact->address);
		xml.AddAttrib(_T("city"), pContact->city);
		xml.AddAttrib(_T("state"), pContact->state);
		xml.AddAttrib(_T("zip"), pContact->zip);
		xml.AddAttrib(_T("comment"), pContact->comment);
		xml.AddAttrib(_T("id"), pContact->id);
		xml.AddAttrib(_T("info"), pContact->info);
		xml.AddAttrib(_T("presence"), pContact->presence ? _T("1") : _T("0"));
		xml.AddAttrib(_T("starred"), pContact->starred ? _T("1") : _T("0"));
		xml.AddAttrib(_T("directory"), pContact->directory ? _T("1") : _T("0"));
	}

	CString filename = accountSettings.pathRoaming;
	filename.Append(_T("Contacts.xml"));
	CFile file;
	CFileException fileException;
	if (file.Open(filename, CFile::modeCreate | CFile::modeWrite, &fileException)) {
		CStringA str = "<?xml version=\"1.0\"?>\r\n";
		str.Append(Utf8EncodeUcs2(xml.GetDoc()));
		file.Write(str.GetBuffer(), str.GetLength());
		file.Close();
	}
}

void Contacts::ContactsLoad()
{
	CString filename = accountSettings.pathRoaming;
	filename.Append(_T("Contacts.xml"));
	CFile file;
	CFileException fileException;
	if (file.Open(filename, CFile::modeRead, &fileException)) {
		CStringA data;
		char buf[256];
		int count;
		while (true) {
			count = file.Read(buf, sizeof(buf));
			data.Append(buf, count);
			if (count < sizeof(buf)) {
				break;
			}
		}
		file.Close();
		CMarkup xml;
		BOOL bResult = xml.SetDoc(MSIP::Utf8DecodeUni(data));
		if (bResult) {
			if (xml.FindElem(_T("contacts"))) {
				while (xml.FindChildElem(_T("contact"))) {
					xml.IntoElem();
					Contact contact;
					contact.name = xml.GetAttrib(_T("name"));
					contact.number = xml.GetAttrib(_T("number"));
					contact.firstname = xml.GetAttrib(_T("firstname"));
					contact.lastname = xml.GetAttrib(_T("lastname"));
					contact.phone = xml.GetAttrib(_T("phone"));
					contact.mobile = xml.GetAttrib(_T("mobile"));
					contact.email = xml.GetAttrib(_T("email"));
					contact.address = xml.GetAttrib(_T("address"));
					contact.city = xml.GetAttrib(_T("city"));
					contact.state = xml.GetAttrib(_T("state"));
					contact.zip = xml.GetAttrib(_T("zip"));
					contact.comment = xml.GetAttrib(_T("comment"));
					contact.id = xml.GetAttrib(_T("id"));
					contact.info = xml.GetAttrib(_T("info"));
					CString rab;
					rab = xml.GetAttrib(_T("presence"));
					contact.presence = rab == _T("1");
					rab = xml.GetAttrib(_T("starred"));
					contact.starred = rab == _T("1");
					rab = xml.GetAttrib(_T("directory"));
					contact.directory = rab == _T("1");
					if (!contact.number.IsEmpty()) {
						if (!isFiltered(&contact)) {
							ContactAdd(contact, FALSE, TRUE);
						}
					}
					xml.OutOfElem();
				}
			}
		}
	}
	else {
		// old
		CString key;
		CString val;
		LPTSTR ptr = val.GetBuffer(255);
		int i = 0;
		while (TRUE) {
			key.Format(_T("%d"), i);
			if (GetPrivateProfileString(_T("Contacts"), key, NULL, ptr, 256, accountSettings.iniFile)) {
				Contact contact;
				ContactDecode(ptr, contact);
				ContactAdd(contact, FALSE, TRUE);
			}
			else {
				break;
			}
			i++;
		}
		WritePrivateProfileSection(_T("Contacts"), NULL, accountSettings.iniFile);
		ContactsSave();
	}
	m_SortItemsExListCtrl.SortColumn(m_SortItemsExListCtrl.GetSortColumn(), m_SortItemsExListCtrl.IsAscending());
}

void Contacts::ContactDecode(CString str, Contact& contact)
{
	CString rab;
	int begin;
	int end;
	begin = 0;
	end = str.Find(';', begin);
	if (end != -1) {
		contact.number = str.Mid(begin, end - begin);
		begin = end + 1;
		end = str.Find(';', begin);
		if (end != -1) {
			contact.name = str.Mid(begin, end - begin);
			begin = end + 1;
			end = str.Find(';', begin);
			if (end != -1) {
				rab = str.Mid(begin, end - begin);
				contact.presence = rab == _T("1");
				begin = end + 1;
				end = str.Find(';', begin);
				if (end != -1) {
					rab = str.Mid(begin, end - begin);
				}
				else {
					rab = str.Mid(begin);
				}
				contact.directory = rab == _T("1");
			}
			else {
				rab = str.Mid(begin);
				contact.presence = rab == _T("1");
			}
		}
		else {
			contact.name = str.Mid(begin);
		}
	}
	else {
		contact.number = str;
		contact.name = contact.number;
	}
}

Contact* Contacts::FindContact(CString number, bool subscribed)
{
	POSITION pos = contacts.GetHeadPosition();
	while (pos) {
		Contact* contact = contacts.GetNext(pos);
		if (subscribed) {
			if (contact->presence) {
				CString commands;
				CString numberFormated = FormatNumber(contact->number, &commands, true);
				if (number == numberFormated) {
					return contact;
				}
			}
		}
		else {
			if (number == contact->number) {
				return contact;
			}
		}
	};
	return NULL;
}

CString Contacts::GetNameByNumber(CString number)
{
	CString name;
	CString nameAlt;
	POSITION pos = contacts.GetHeadPosition();
	while (pos) {
		Contact* contact = contacts.GetNext(pos);
		CString commands;
		CString numberContact = FormatNumber(contact->number, &commands);
		SIPURI sipuri;
		MSIP::ParseSIPURI(numberContact, &sipuri);
		numberContact = !sipuri.user.IsEmpty() ? sipuri.user : sipuri.domain;
		if (number == numberContact) {
			name = contact->name;
			break;
		}
		if (numberContact.GetLength() > 3) {
			int pos = number.Find(numberContact);
			if (pos >= 0 && pos <= 3 && number.GetLength() == numberContact.GetLength() + pos) {
				nameAlt = contact->name;
			}
		}
	};
	return !name.IsEmpty() ? name : nameAlt;
}

void Contacts::PresenceUnsubsribeOne(Contact* pContact)
{
	mainDlg->UnsubscribeNumber(&pContact->number);
	PresenceReset(pContact);
}

void Contacts::PresenceSubscribe()
{
	POSITION pos = contacts.GetHeadPosition();
	while (pos) {
		Contact* contact = contacts.GetNext(pos);
		if (contact->presence) {
			mainDlg->SubsribeNumber(&contact->number);
		}
	}
}

void Contacts::PresenceReset(Contact* pContact)
{
	if (!::IsWindow(this->m_hWnd)) {
		return;
	}
	if (isFiltered()) {
		filterReset();
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	int n = list->GetItemCount();
	for (int i = 0; i < n; i++) {
		Contact* contact = (Contact*)list->GetItemData(i);
		if (!pContact || pContact == contact) {
			if (contact->image != MSIP_CONTACT_ICON_DEFAULT) {
				contact->info.Empty();
				list->SetItemText(i, 2, _T(""));
			}
			contact->image = MSIP_CONTACT_ICON_DEFAULT;
			contact->ringing = false;
			list->SetItem(i, 0, LVIF_IMAGE, 0, contact->image + (contact->starred ? 7 : 0), 0, 0, 0);
		}
	}
}

void Contacts::PresenceReceived(CString* buddyNumber, int image, bool ringing, CString* info)
{
	bool blink = false;
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	POSITION pos = contacts.GetHeadPosition();
	while (pos) {
		Contact* contact = contacts.GetNext(pos);
		if (contact->presence) {
			CString commands;
			CString numberFormated = FormatNumber(contact->number, &commands, true);
			if (*buddyNumber == numberFormated) {
				if (ringing) {
					blink = true;
				}
				contact->image = image;
				contact->ringing = ringing;
				contact->info = *info;
				LVFINDINFO findInfo;
				int i;
				findInfo.flags = LVFI_PARAM;
				findInfo.lParam = (LPARAM)contact;
				if ((i = list->FindItem(&findInfo)) != -1) {
					list->SetItem(i, 0, LVIF_IMAGE, 0, contact->image + (contact->starred ? 7 : 0), 0, 0, 0);
					list->SetItemText(i, 2, Translate(contact->info.GetBuffer()));
				}
			}
		}
	};
	if (blink) {
		if (!blinkTimer) {
			blinkTimer = SetTimer(IDT_TIMER_CONTACTS_BLINK, 500, NULL);
			OnTimerContactsBlink();
		}
	}
}

void Contacts::OnTimerContactsBlink()
{
	if (!blinkTimer) {
		return;
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	int n = list->GetItemCount();
	bool ringing = false;
	for (int i = 0; i < n; i++) {
		Contact* contact = (Contact*)list->GetItemData(i);
		if (contact->ringing) {
			list->SetItem(i, 0, LVIF_IMAGE, 0, blinkState ? contact->image + (contact->starred ? 7 : 0) : MSIP_CONTACT_ICON_BLANK, 0, 0, 0);
			ringing = true;
		}
	}
	if (!ringing) {
		blinkTimer = NULL;
		KillTimer(IDT_TIMER_CONTACTS_BLINK);
		blinkState = false;
	}
	else {
		blinkState = !blinkState;
	}
}

void Contacts::SetCanditates()
{
	if (isFiltered()) {
		filterReset();
	}
	GetDlgItem(IDC_FILER_VALUE)->EnableWindow(FALSE);
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	int count = list->GetItemCount();
	for (int i = 0; i < count; i++)
	{
		Contact* pContact = (Contact*)list->GetItemData(i);
		if (pContact->directory) {
			pContact->candidate = true;
		}
	}
}
int Contacts::DeleteCanditates()
{
	if (isFiltered()) {
		filterReset();
	}
	CListCtrl* list = (CListCtrl*)GetDlgItem(IDC_CONTACTS);
	int count = list->GetItemCount();
	int deleted = 0;
	for (int i = 0; i < count; i++)
	{
		Contact* pContact = (Contact*)list->GetItemData(i);
		if (pContact->candidate) {
			ContactDelete(i);
			count--;
			i--;
			deleted++;
		}
	}
	GetDlgItem(IDC_FILER_VALUE)->EnableWindow(TRUE);
	return deleted;
}

