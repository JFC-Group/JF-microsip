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
#include "const.h"
#include "Contacts.h"

enum msip_action {
	MSIP_ACTION_TRANSFER,
	MSIP_ACTION_ATTENDED_TRANSFER,
	MSIP_ACTION_INVITE,
	MSIP_ACTION_FORWARD,
};

class Transfer :
	public CDialog
{
public:
	Transfer(CWnd* pParent = NULL);	// standard constructor
	~Transfer();
	enum { IDD = IDD_TRANSFER };
	msip_action action;
	pjsua_call_id callId;
	void SetAction(msip_action action, pjsua_call_id call_id = PJSUA_INVALID_ID);
	void LoadFromContacts(Contact *selectedContact = NULL);
protected:
	CFont m_font;

	void ClearDropdown();
	afx_msg void OnDestroy();
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnClose();
	void Input(CString digits);
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedKey1();
	afx_msg void OnBnClickedKey2();
	afx_msg void OnBnClickedKey3();
	afx_msg void OnBnClickedKey4();
	afx_msg void OnBnClickedKey5();
	afx_msg void OnBnClickedKey6();
	afx_msg void OnBnClickedKey7();
	afx_msg void OnBnClickedKey8();
	afx_msg void OnBnClickedKey9();
	afx_msg void OnBnClickedKeyStar();
	afx_msg void OnBnClickedKey0();
	afx_msg void OnBnClickedKeyGrate();
};
