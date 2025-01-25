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
#include "stdafx.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

struct SIPURI {
	CString name;
	CString user;
	CString domain;
	CString parameters;
	CString commands;
};

CStringA msip_md5sum(CString *str);
void msip_audio_conf_set_volume(int val, bool mute);
void msip_audio_input_set_volume(int val, bool mute = false);
pj_status_t msip_verify_sip_url(const char *url);
int msip_get_duration(pj_time_val *time_val);

namespace MSIP
{
void GetScreenRect(CRect *rect);
CString GetErrorMessage(pj_status_t status);
BOOL ShowErrorMessage(pj_status_t status);
BOOL IsIP(CString host);
CString RemovePort(CString domain);
void ParseSIPURI(CString in, SIPURI* out);
CString BuildSIPURI(const SIPURI* in);
CString PjToStr(const pj_str_t* str, BOOL utf = FALSE);
pj_str_t StrToPjStr(CString str);
char* StrToPj(CString str);
CString Utf8DecodeUni(CStringA str);
CStringA UnicodeToAnsi(CString str);
CString AnsiToUnicode(CStringA str);
CString AnsiToWideChar(char* str);
CStringA StringToPjString(CString str);
char *WideCharToPjStr(CString str);
CString PjStrToWideChar(char *str);
void OpenURL(CString url);
CString GetDuration(int sec, bool zero = false);
bool IsPSTNNnmber(CString number);
bool IniSectionExists(CString section, CString iniFile);
CString Bin2String(CByteArray *ca);
void String2Bin(CString str, CByteArray *res);
void CommandLineToShell(CString cmd, CString &command, CString &params);
void RunCmd(CString cmdLine, CString addParams=_T(""), bool noWait = false);
void PortKnock();
bool IsConnectedToInternet();
}
