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

#pragma once

#include "resource.h"
#include "global.h"
#include "AddDlg.h"
#include "BaseDialog.h"
#include "CListCtrl_SortItemsEx.h"

class Contacts :
	public CBaseDialog
{
public:
	Contacts(CWnd* pParent = NULL);	// standard constructor
	~Contacts();
	enum { IDD = IDD_CONTACTS };
	void TabFocusSet() override {};
	bool GotoTab(int i, CTabCtrl* tab) { return false; };

	CListCtrl_SortItemsEx m_SortItemsExListCtrl;

	AddDlg* addDlg;

	CList<Contact*> contacts;

	bool ContactPrepare(Contact* contact);
	void ContactCreate(CListCtrl* list, Contact* pContact, bool subscribe = true);
	void ListAppend(CListCtrl* list, Contact* contact, bool subscribe = true);
	bool ContactUpdate(CListCtrl* list, int i, Contact* contact, Contact* newContact, CStringList* fields);
	void ContactsAdd(CArray<ContactWithFields*> *contacts, bool directory = false);
	bool ContactAdd(Contact contact, BOOL save = FALSE, BOOL load = FALSE, CStringList* fields = NULL, CString oldNumber = _T(""), bool manual = false);

	void ContactDelete(int i);
	void ContactDeleteRaw(Contact* contact);
	void ContactsSave();
	void ContactsLoad();
	bool isFiltered(Contact *pContact = NULL);
	void filterReset();

	void SetCanditates();
	int DeleteCanditates();

	void UpdateCallButton();
	Contact* FindContact(CString number, bool subscribed = false);
	CString GetNameByNumber(CString number);
	void PresenceSubsribeOne(Contact *pContact);
	void PresenceUnsubsribeOne(Contact *pContact);
	void PresenceSubscribe();
	void PresenceReset(Contact* pContact = NULL);
	void PresenceReceived(CString *buddyNumber, int image, bool ringing, CString* info);
	void OnTimerContactsBlink();
	void OnCreated();
	bool Import(CString filename, CArray<ContactWithFields*> &contacts, bool directory = false);

private:
	void ContactDecode(CString str, Contact &contact);
	void MessageDlgOpen(BOOL isCall = FALSE, BOOL hasVideo = FALSE, BYTE index = 0);
	void DefaultItemAction(int i);

protected:
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR TimerVal);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnFilterValueChange();
	afx_msg void OnMenuCallPickup();
	afx_msg void OnMenuCall(); 
	afx_msg void OnMenuCallPhone(); 
	afx_msg void OnMenuCallMobile(); 
	afx_msg void OnMenuChat();
	afx_msg void OnMenuAdd(); 
	afx_msg void OnMenuEdit(); 
	afx_msg void OnMenuCopy(); 
	afx_msg void OnMenuDelete(); 
	afx_msg void OnMenuImport();
	afx_msg void OnMenuExport();
	afx_msg LRESULT OnContextMenu(WPARAM wParam,LPARAM lParam);
	afx_msg void OnNMDblclkContacts(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEndtrack(NMHDR* pNMHDR, LRESULT* pResult);
#ifdef _GLOBAL_VIDEO
	afx_msg void OnMenuCallVideo(); 
#endif
};

