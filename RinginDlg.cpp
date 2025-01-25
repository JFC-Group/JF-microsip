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
#include "RinginDlg.h"
#include "langpack.h"
#include "mainDlg.h"
#include "settings.h"
#include "Markup.h"

#include <vector>
#include <algorithm>

RinginDlg::RinginDlg(CWnd* pParent /*=NULL*/)
	: CBaseDialog(RinginDlg::IDD, pParent)
{
	Create (IDD, pParent);
	answered = false;
}

RinginDlg::~RinginDlg(void)
{
}

int RinginDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (langPack.rtl) {
		ModifyStyleEx(0,WS_EX_LAYOUTRTL);
	}
	return 0;
}

BOOL CALLBACK MyInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MONITORINFOEX iMonitor;
    iMonitor.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &iMonitor);
    if (iMonitor.dwFlags == DISPLAY_DEVICE_MIRRORING_DRIVER) {
        return true;
    } else {
        reinterpret_cast< std::vector<HMONITOR>* >(dwData)->push_back(hMonitor);
        return true;
    }
}

void RinginDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDialog::DoDataExchange(pDX);
}

BOOL RinginDlg::OnInitDialog() {
	CBaseDialog::OnInitDialog();

	AutoMove(IDC_ANSWER, 0, 100, 0, 0);
	AutoMove(IDC_DECLINE, 0, 100, 0, 0);
	AutoMove(IDC_IGNORE, 0, 100, 0, 0);

#ifdef _GLOBAL_VIDEO
	if (accountSettings.disableVideo) {
		GetDlgItem(IDC_VIDEO)->ShowWindow(SW_HIDE);
	}
#endif
	
	TranslateDialog(this->m_hWnd);

	CFont* font = this->GetFont();
	LOGFONT lf;
	font->GetLogFont(&lf);

	lf.lfHeight = 12;
	m_font_ignore.CreateFontIndirect(&lf);
	GetDlgItem(IDC_IGNORE)->SetFont(&m_font_ignore);
	GetDlgItem(IDC_IGNORE)->EnableWindow(FALSE);
	if (accountSettings.noIgnoreCall) {
		GetDlgItem(IDC_IGNORE)->ShowWindow(SW_HIDE);
	}
	lf.lfHeight = 24;
	lf.lfWeight = FW_BOLD;
	m_font.CreateFontIndirect(&lf);
	GetDlgItem(IDC_CALLER_NAME)->SetFont(&m_font);
	
	GetDlgItem(IDC_CALLER_NAME)->ModifyStyleEx(WS_EX_CLIENTEDGE,0,SWP_NOSIZE|SWP_FRAMECHANGED);
	GetDlgItem(IDC_CALLER_ADDR)->ModifyStyleEx(WS_EX_CLIENTEDGE,0,SWP_NOSIZE|SWP_FRAMECHANGED);

	m_hIconTransfer = LoadImageIcon(IDI_FORWARD);
	((CButton*)GetDlgItem(IDC_TRANSFER))->SetIcon(m_hIconTransfer);
	int x,y;
	if (accountSettings.randomAnswerBox) {
		CRect ringinRect;
		GetWindowRect(&ringinRect);
		if (accountSettings.multiMonitor) {		
			std::vector<HMONITOR> hMonitorArray;
			EnumDisplayMonitors(NULL, NULL, &MyInfoEnumProc, reinterpret_cast<LPARAM>(&hMonitorArray));
			std::random_shuffle ( hMonitorArray.begin(), hMonitorArray.end() );
			std::vector<HMONITOR>::iterator it = hMonitorArray.begin();
			HMONITOR hMonitor = *it;
			MONITORINFO mi;
			mi.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(hMonitor,&mi);
			x = mi.rcWork.left + ( (mi.rcWork.right-mi.rcWork.left) -ringinRect.Width()) * rand() / RAND_MAX;
			y = mi.rcWork.top + ( (mi.rcWork.bottom-mi.rcWork.top) -ringinRect.Height()) * rand() / RAND_MAX;
		} else {
			CRect primaryScreenRect;
			SystemParametersInfo(SPI_GETWORKAREA,0,&primaryScreenRect,0);
			x = primaryScreenRect.left + ( (primaryScreenRect.right-primaryScreenRect.left) -ringinRect.Width()) * rand() / RAND_MAX;
			y = primaryScreenRect.top + ( (primaryScreenRect.bottom-primaryScreenRect.top) -ringinRect.Height()) * rand() / RAND_MAX;
		}
	} else {
		if (mainDlg->ringinDlgs.GetCount())
		{
			CRect rect;
			mainDlg->ringinDlgs.GetAt(mainDlg->ringinDlgs.GetCount()-1)->GetWindowRect(&rect);
			x=rect.left+22;
			y=rect.top+22;
		} else {
			if (accountSettings.ringinX || accountSettings.ringinY) {
				CRect screenRect;
				MSIP::GetScreenRect(&screenRect);
				if (accountSettings.multiMonitor) {
					MSIP::GetScreenRect(&screenRect);
				}
				else {
					SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
				}
				CRect rect;
				GetWindowRect(&rect);
				int maxLeft = screenRect.right-rect.Width();
				if (accountSettings.ringinX>maxLeft) {
					x = maxLeft;
				} else {
					x = accountSettings.ringinX < screenRect.left ? screenRect.left : accountSettings.ringinX;
				}
				int maxTop = screenRect.bottom-rect.Height();
				if (accountSettings.ringinY>maxTop) {
					y = maxTop;
				} else {
					y = accountSettings.ringinY < screenRect.top ? screenRect.top : accountSettings.ringinY;
				}
			} else {
				CRect ringinRect;
				GetWindowRect(&ringinRect);
				CRect primaryScreenRect;
				SystemParametersInfo(SPI_GETWORKAREA,0,&primaryScreenRect,0);
				x = (primaryScreenRect.Width()-ringinRect.Width())/2;
				y = (primaryScreenRect.Height()-ringinRect.Height())/2;
			}
		}
	}
	SetWindowPos(accountSettings.bringToFrontOnIncoming || accountSettings.alwaysOnTop ? &this->wndTopMost : &this->wndNoTopMost, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
	if (accountSettings.bringToFrontOnIncoming) {
		if (!MACRO_SILENT) {
			if (mainDlg->IsWindowVisible()) {
				if (mainDlg->IsIconic()) {
					mainDlg->ShowWindow(SW_RESTORE);
				}
				else {
					mainDlg->ShowWindow(SW_HIDE);
					mainDlg->ShowWindow(SW_MINIMIZE);
					mainDlg->ShowWindow(SW_RESTORE);
				}
			}
			ShowWindow(SW_SHOWNORMAL);
			SetForegroundWindow();
		}
	}
	else {
		if (mainDlg->IsWindowVisible()) {
			ShowWindow(SW_SHOWNORMAL);
		}
	}
	return 0;
}

void RinginDlg::SetCallId(pjsua_call_id new_call_id)
{
	call_id = new_call_id;

}


BEGIN_MESSAGE_MAP(RinginDlg, CBaseDialog)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_TIMER()
	ON_WM_MOVE()
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDOK, &RinginDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &RinginDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_ANSWER, &RinginDlg::OnBnClickedAudio)
	ON_BN_CLICKED(IDC_DECLINE, &RinginDlg::OnBnClickedDecline)
	ON_BN_CLICKED(IDC_VIDEO, &RinginDlg::OnBnClickedVideo)
	ON_BN_CLICKED(IDC_TRANSFER, OnBnClickedTransfer)
END_MESSAGE_MAP()

void RinginDlg::OnClose() 
{
	if (accountSettings.noIgnoreCall) {
		OnBnClickedDecline();
	}
	else {
		Close();
	}
}

void RinginDlg::OnAnswer()
{
	answered = true;
#ifdef _GLOBAL_VIDEO
	GetDlgItem(IDC_VIDEO)->EnableWindow(FALSE);
#endif
	GetDlgItem(IDC_ANSWER)->EnableWindow(FALSE);
	GetDlgItem(IDC_DECLINE)->EnableWindow(FALSE);
}

void RinginDlg::Close(BOOL accept)
{
	int count = mainDlg->ringinDlgs.GetCount();
	for (int i = 0; i < count; i++)
	{
		if (call_id == mainDlg->ringinDlgs.GetAt(i)->call_id)
		{
			if (!accept) {
				mainDlg->UpdateWindowText(_T("-"));
			}
			if (count == 1) {
				mainDlg->PlayerStop();
			}
			mainDlg->ringinDlgs.RemoveAt(i);
			call_id = -1;
			break;
		}
	}
	if (call_id == -1) {
		DestroyWindow();
	}
}

void RinginDlg::OnBnClickedOk()
{
}

void RinginDlg::OnBnClickedCancel()
{
	Close();
}

void RinginDlg::OnBnClickedDecline()
{
	if (!answered) {
		pjsua_call_info call_info;
		pjsua_call_get_info(call_id, &call_info);
		call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
		if (user_data) {
			user_data->CS.Lock();
			user_data->hangup = true;
			user_data->CS.Unlock();
		}		
		msip_call_busy(call_id);
		mainDlg->callIdIncomingIgnore = MSIP::PjToStr(&call_info.call_id);
	}
	Close();
}

void RinginDlg::OnBnClickedAudio() 
{
	CallAccept();
}

void RinginDlg::OnBnClickedVideo()
{
	CallAccept(TRUE);
}

void RinginDlg::CallAccept(BOOL hasVideo)
{
	if (!answered) {
		mainDlg->onCallAnswer((WPARAM)call_id, (LPARAM)hasVideo);
	}
}

void RinginDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	SetTimer(IDT_TIMER_INIT_RINGIN,1000,NULL);
}

void RinginDlg::OnTimer (UINT_PTR TimerVal)
{
	if (TimerVal == IDT_TIMER_INIT_RINGIN)
	{
		KillTimer(IDT_TIMER_INIT_RINGIN);
	}
}

void RinginDlg::OnMove(int x, int y)
{
	if (IsWindowVisible() && !IsZoomed() && !IsIconic()) {
		CRect cRect;
		GetWindowRect(&cRect);
		accountSettings.ringinX = cRect.left;
		accountSettings.ringinY = cRect.top;
		mainDlg->AccountSettingsPendingSave();
	}
}

void RinginDlg::OnBnClickedTransfer()
{
	mainDlg->OpenTransferDlg(this, MSIP_ACTION_FORWARD, call_id);
}
