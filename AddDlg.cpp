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
#include "AddDlg.h"
#include "mainDlg.h"
#include "langpack.h"

AddDlg::AddDlg(CWnd* pParent /*=NULL*/)
	: CDialog(AddDlg::IDD, pParent)
{
	Create (IDD, pParent);
}

AddDlg::~AddDlg()
{
}

int AddDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (langPack.rtl) {
		ModifyStyleEx(0,WS_EX_LAYOUTRTL);
	}
	loading = false;
	return 0;
}

BOOL AddDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	TranslateDialog(this->m_hWnd);
	return TRUE;
}

void AddDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	delete this;
}


BEGIN_MESSAGE_MAP(AddDlg, CDialog)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDOK, &AddDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &AddDlg::OnBnClickedCancel)
	ON_EN_CHANGE(IDC_ADD_FIRSTNAME, &AddDlg::OnChangeFirstLastName)
	ON_EN_CHANGE(IDC_ADD_LASTNAME, &AddDlg::OnChangeFirstLastName)
	ON_EN_CHANGE(IDC_ADD_PHONE, &AddDlg::OnChangePhoneNumber)
END_MESSAGE_MAP()


void AddDlg::OnClose() 
{
	this->ShowWindow(SW_HIDE);
}

void AddDlg::Load(Contact* pContact)
{
	loading = true;
	number = pContact->number;
	firstname = pContact->firstname;
	lastname = pContact->lastname;
	phone = pContact->phone;
	GetDlgItem(IDC_ADD_NAME)->SetWindowText(pContact->name);
	CString numberLocal = pContact->number;
	GetDlgItem(IDC_ADD_NUMBER)->SetWindowText(numberLocal);
	GetDlgItem(IDC_ADD_FIRSTNAME)->SetWindowText(pContact->firstname);
	GetDlgItem(IDC_ADD_LASTNAME)->SetWindowText(pContact->lastname);
	GetDlgItem(IDC_ADD_PHONE)->SetWindowText(pContact->phone);
	GetDlgItem(IDC_ADD_MOBILE)->SetWindowText(pContact->mobile);
	GetDlgItem(IDC_ADD_EMAIL)->SetWindowText(pContact->email);
	GetDlgItem(IDC_ADD_ADDRESS)->SetWindowText(pContact->address);
	GetDlgItem(IDC_ADD_CITY)->SetWindowText(pContact->city);
	GetDlgItem(IDC_ADD_STATE)->SetWindowText(pContact->state);
	GetDlgItem(IDC_ADD_ZIP)->SetWindowText(pContact->zip);
	GetDlgItem(IDC_ADD_COMMENT)->SetWindowText(pContact->comment);
	((CButton *)GetDlgItem(IDC_ADD_PRESENCE))->SetCheck(pContact->presence);
	((CButton *)GetDlgItem(IDC_ADD_STARRED))->SetCheck(pContact->starred);
	GetDlgItem(IDC_ADD_NAME)->SetFocus();
	loading = false;
}

void AddDlg::OnBnClickedOk()
{
	CStringList fields;
	Contact contact; 
	fields.AddTail(_T("name"));
	GetDlgItem(IDC_ADD_NAME)->GetWindowText(contact.name);
	contact.name.Trim();
	fields.AddTail(_T("number"));
	GetDlgItem(IDC_ADD_NUMBER)->GetWindowText(contact.number);
	contact.number.Trim();
	fields.AddTail(_T("firstname"));
	GetDlgItem(IDC_ADD_FIRSTNAME)->GetWindowText(contact.firstname);
	contact.firstname.Trim();
	fields.AddTail(_T("lastname"));
	GetDlgItem(IDC_ADD_LASTNAME)->GetWindowText(contact.lastname);
	contact.lastname.Trim();
	fields.AddTail(_T("phone"));
	GetDlgItem(IDC_ADD_PHONE)->GetWindowText(contact.phone);
	contact.phone.Trim();
	fields.AddTail(_T("mobile"));
	GetDlgItem(IDC_ADD_MOBILE)->GetWindowText(contact.mobile);
	contact.mobile.Trim();
	fields.AddTail(_T("email"));
	GetDlgItem(IDC_ADD_EMAIL)->GetWindowText(contact.email);
	contact.email.Trim();
	fields.AddTail(_T("address"));
	GetDlgItem(IDC_ADD_ADDRESS)->GetWindowText(contact.address);
	contact.address.Trim();
	fields.AddTail(_T("city"));
	GetDlgItem(IDC_ADD_CITY)->GetWindowText(contact.city);
	contact.city.Trim();
	fields.AddTail(_T("state"));
	GetDlgItem(IDC_ADD_STATE)->GetWindowText(contact.state);
	contact.state.Trim();
	fields.AddTail(_T("zip"));
	GetDlgItem(IDC_ADD_ZIP)->GetWindowText(contact.zip);
	contact.zip.Trim();
	fields.AddTail(_T("comment"));
	GetDlgItem(IDC_ADD_COMMENT)->GetWindowText(contact.comment);
	contact.comment.Trim();
	fields.AddTail(_T("presence"));
	contact.presence = ((CButton*)GetDlgItem(IDC_ADD_PRESENCE))->GetCheck();
	fields.AddTail(_T("starred"));
	contact.starred = ((CButton*)GetDlgItem(IDC_ADD_STARRED))->GetCheck();
	if (!contact.number.IsEmpty()) {
		if (number.IsEmpty() || number != contact.number) {
			if (mainDlg->pageContacts->FindContact(contact.number)) {
				if (MessageBox(Translate(_T("Contact with the same number already exists. Do you want to overwrite?")), Translate(_T("Contact already exists")), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
					return;
				}
			}
		}
		if (mainDlg->pageContacts->ContactAdd(contact, TRUE, FALSE, &fields, number, true)) {
			OnClose();
		}
	}
	else {
		GetDlgItem(IDC_ADD_NUMBER)->SetFocus();
	}
}

void AddDlg::OnBnClickedCancel()
{
	OnClose();
}

void AddDlg::OnChangeFirstLastName()
{
	if (loading) {
		return;
	}
	CString str;
	GetDlgItem(IDC_ADD_NAME)->GetWindowText(str);
	str.Trim();
	CString fname;
	CString lname;
	int pos = -1;
	if (!firstname.IsEmpty()) {
		if (str.Find(firstname) == 0) {
			pos = firstname.GetLength();
		}
	} else if (!lastname.IsEmpty()) {
		if (str.Find(lastname) == 0) {
			pos = 0;
		}
	}
	if (pos == -1) {
		pos = str.Find(' ');
	}
	if (pos != -1) {
		fname = str.Left(pos);
		lname = str.Mid(pos);
		lname.Trim();
	}
	else {
		fname = str;
	}
	//--
	CString str1;
	CString str2;
	GetDlgItem(IDC_ADD_FIRSTNAME)->GetWindowText(str1);
	str1.Trim();
	GetDlgItem(IDC_ADD_LASTNAME)->GetWindowText(str2);
	str2.Trim();
	if ((fname == firstname && lname == lastname) || (lname.IsEmpty() && fname == lastname)) {
		str.Format(_T("%s %s"), str1, str2);
		str.Trim();
		GetDlgItem(IDC_ADD_NAME)->SetWindowText(str);
	}
	firstname = str1;
	lastname = str2;
}

void AddDlg::OnChangePhoneNumber()
{
	CString str;
	CString str1;
	CString str2;
	GetDlgItem(IDC_ADD_NUMBER)->GetWindowText(str);
	GetDlgItem(IDC_ADD_PHONE)->GetWindowText(str1);
	if (phone == str) {
		GetDlgItem(IDC_ADD_NUMBER)->SetWindowText(str1);
	}
	phone = str1;
}

