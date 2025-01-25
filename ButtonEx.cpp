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
#include "ButtonEx.h"   

// CButtonEx   
IMPLEMENT_DYNAMIC(CButtonEx, CMFCButton)
CButtonEx::CButtonEx()   
{   
	m_nFlatStyle = CMFCButton::BUTTONSTYLE_NOBORDERS;
	m_bTransparent = false;
}
   
CButtonEx::~CButtonEx()   
{   
}

BEGIN_MESSAGE_MAP(CButtonEx, CMFCButton)   
    //{{AFX_MSG_MAP(CButtonEx)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CButtonEx::EnableWindow(BOOL bEnable)
{
	if (bEnable) {
		SetTextColor(m_TextColor);
		SetFaceColor(m_FaceColor, true);
	}
	else {
//		SetTextColor(RGB(123, 123, 123));
		SetTextColor(RGB(0, 0, 0));
		SetFaceColor(RGB(222, 222, 222), true);
	}
	return CMFCButton::EnableWindow(bEnable);
}
