/* 
 * Copyright (C) 2011-2020 MicroSIP (http://www.microsip.org)
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
#include "HidApi.h"
#include "hidusage.h"

class Hid
{
private:
	static void CALLBACK TimerProc(HWND hWnd,
		UINT nMsg,
		UINT_PTR nIDEvent,
		DWORD dwTime
		);
	static hid_device *m_hDevice;

//	static bool m_stateOnline;
	static bool m_stateOffhook;
	static bool m_stateRing;
	static bool m_stateMute;
	static UCHAR m_reportIdOnline;
	static UCHAR m_reportIdOffhook;
	static UCHAR m_reportIdRing;
	static UCHAR m_reportIdMute;
	static void UpdateLED(USAGE Usage = 0, USAGE Usage2 = 0);
public:
	static void OpenDevice();
	static void CloseDevice(bool exit = false);
	static void SetOnline(bool state);
	static void SetOffhook(bool state);
	static void SetRing(bool state);
	static void SetOffhookRing(bool stateOffhook, bool stateRing);
	static void SetMute(bool state);
};
