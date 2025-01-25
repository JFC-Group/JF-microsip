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

#define THIS_FILENAME "MSIP.cpp"

#include "MSIP.h"
using namespace MSIP;

#include <afxsock.h>

#include "langpack.h"
#include "utf.h"
#include "settings.h"

#include <Netlistmgr.h>

CStringA msip_md5sum(CString *str)
{
	CStringA md5sum;
	CStringA utf8 = Utf8EncodeUcs2(str->GetBuffer());
	DWORD cbContent = utf8.GetLength();
	BYTE* pbContent = (BYTE*)utf8.GetBuffer(cbContent);
	pj_md5_context ctx;
	pj_uint8_t digest[16];
	pj_md5_init(&ctx);
	pj_md5_update(&ctx, (pj_uint8_t*)pbContent, cbContent);
	pj_md5_final(&ctx, digest);
	char *p = md5sum.GetBuffer(32);
	for (int i = 0; i < 16; ++i) {
		pj_val_to_hex_digit(digest[i], p);
		p += 2;
	}
	md5sum.ReleaseBuffer();
	return md5sum;
}

//void msip_audio_output_set_volume(int val, bool mute)
//{
//	if (mute) {
//		val = 0;
//	} else {
//		pj_status_t status = 
//			pjsua_snd_set_setting(
//			PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
//			&val, PJ_TRUE);
//		if (status == PJ_SUCCESS) {
//			val = 100;
//		}
//	}
//	pjsua_conf_adjust_tx_level(0, (float)val/100);
//}

void msip_audio_conf_set_volume(int val, bool mute)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	if (mute) {
		val = 0;
	}
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			pjsua_conf_port_id conf_port_id = pjsua_call_get_conf_port(call_ids[i]);
			if (conf_port_id != PJSUA_INVALID_ID) {
				pjsua_conf_adjust_rx_level(conf_port_id, (float)val / 100);
			}
		}
	}
}

void msip_audio_input_set_volume(int val, bool mute)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	if (mute) {
		val = 0;
	}
	else {
		pj_status_t status = -1;
		if (!accountSettings.swLevelAdjustment) {
			int valHW;
			if (accountSettings.micAmplification) {
				valHW = val >= 50 ? 100 : val * 2;
			}
			else {
				valHW = val;
			}
			status =
				pjsua_snd_set_setting(
					PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
					&valHW, PJ_TRUE);
		}
		if (status == PJ_SUCCESS) {
			if (accountSettings.micAmplification && val > 50) {
				val = 100 + pow((float)val - 50, 1.68f);
			}
			else {
				val = 100;
			}
		}
		else {
			if (accountSettings.micAmplification) {
				if (val > 50) {
					val = 50 + pow((float)val - 50, 1.7f);
				}
			}
		}
	}
	pjsua_conf_adjust_rx_level(0, (float)val / 100);
}

pj_status_t msip_verify_sip_url(const char *url)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return PJSIP_ENOTINITIALIZED;
	}
	return strlen(url) > 900 ? PJSIP_EURITOOLONG : pjsua_verify_sip_url(url);
}

int msip_get_duration(pj_time_val *time_val)
{
	int res = time_val->sec;
	if (time_val->msec >= 500) {
		res++;
	}
	return res;
}

void MSIP::GetScreenRect(CRect *rect)
{
	rect->left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	rect->top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	rect->right = GetSystemMetrics(SM_CXVIRTUALSCREEN) - rect->left;
	rect->bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN) - rect->top;
}

CString MSIP::GetErrorMessage(pj_status_t status)
{
	CStringA str;
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		str = "Softphone is not initialized. Check your settings.";
	} else if (status == 171039 || status ==  171042) {
		str = "Invalid Number";
	}
	else if (status == PJSIP_EAUTHACCNOTFOUND || status == PJSIP_EAUTHACCDISABLED) {
		str = "Account or credentials not found.";
	}
	else if (status == 130051) {
		str = "Unable to connect to remote server.";
	}
	else {
		char *buf = str.GetBuffer(PJ_ERR_MSG_SIZE - 1);
		pj_strerror(status, buf, PJ_ERR_MSG_SIZE);
		str.ReleaseBuffer();
		int i = str.ReverseFind('(');
		if (i != -1) {
			str = str.Left(i - 1);
		}
	}
	return Translate(CString(str).GetBuffer());
}

BOOL MSIP::ShowErrorMessage(pj_status_t status)
{
	if (status != PJ_SUCCESS) {
		AfxMessageBox(GetErrorMessage(status));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

CString MSIP::RemovePort(CString domain)
{
	int pos = domain.Find(_T(":"));
	if (pos != -1) {
		return domain.Mid(0, pos);
	}
	else {
		return domain;
	}
}

BOOL MSIP::IsIP(CString host)
{
	CStringA hostA(host);
	char *pHost = hostA.GetBuffer();
	unsigned long ulAddr = inet_addr(pHost);
	if (ulAddr != INADDR_NONE && ulAddr != INADDR_ANY) {
		struct in_addr antelope;
		antelope.S_un.S_addr = ulAddr;
		if (strcmp(inet_ntoa(antelope), pHost) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

void MSIP::ParseSIPURI(CString in, SIPURI* out)
{
	//	tone_gen.toneslot = -1;
	//	tone_gen = NULL;

	// "WEwewew rewewe" <sip:qqweqwe@qwerer.com;rrrr=tttt;qweqwe=rrr?qweqwr=rqwrqwr>
	// sip:qqweqwe@qwerer.com;rrrr=tttt;qweqwe=rrr?qweqwr=rqwrqwr

	int start, end;

	out->name.Empty();
	out->user.Empty();
	out->domain.Empty();
	out->parameters.Empty();
	out->commands.Empty();

	if (in.Right(1) == _T(">")) {
		in = in.Left(in.GetLength() - 1);
	}

	start = in.Find(_T("sip:"));
	if (start > 0)
	{
		out->name = in.Left(start);
		out->name.Trim(_T(" \" <"));
		if (!out->name.CompareNoCase(_T("unknown")))
		{
			out->name = _T("");
		}
	}
	if (start >= 0)
	{
		start += 4;
	}
	else {
		start = 0;
	}
	end = in.Find(_T("@"), start);
	if (end >= 0)
	{
		out->user = in.Mid(start, end - start);
		start = end + 1;
	}
	end = in.Find(_T(";"), start);
	if (end >= 0) {
		out->domain = in.Mid(start, end - start);
		start = end;
		out->parameters = in.Mid(start);
	}
	else {
		end = in.Find(_T("?"), start);
		if (end >= 0) {
			out->domain = in.Mid(start, end - start);
			start = end;
			out->parameters = in.Mid(start);
		}
		else {
			out->domain = in.Mid(start);
			start = out->domain.Find(_T(","));
			if (start >= 0) {
				out->commands = out->domain.Mid(start);
				out->domain = out->domain.Left(start);
			}
		}
	}
}

CString MSIP::BuildSIPURI(const SIPURI* in)
{
	CString res;
	res.Format(_T("%s@%s"), in->user, in->domain);
	if (!in->parameters.IsEmpty()) {
		res.AppendFormat(_T(";%s"), in->parameters);
	}
	res.Format(_T("<sip:%s>"), res);
	if (!in->name.IsEmpty()) {
		res.Format(_T("\"%s\" %s"), in->name, res);
	}
	if (!in->commands.IsEmpty()) {
		res.AppendFormat(_T(",%s"), in->commands);
	}
	return res;
}

CString MSIP::PjToStr(const pj_str_t* str, BOOL utf)
{
	CStringA rab;
	rab.Format("%.*s", str->slen, str->ptr);
	if (utf)
	{
#ifdef _UNICODE
		WCHAR* msg;
		Utf8DecodeCP(rab.GetBuffer(), CP_ACP, &msg);
		return msg;
#else
		return Utf8DecodeCP(rab.GetBuffer(), CP_ACP, NULL);
#endif
	}
	else
	{
		return CString(rab);
	}
}

pj_str_t MSIP::StrToPjStr(CString str)
{
	return pj_str(StrToPj(str));
}

char* MSIP::StrToPj(CString str)
{
#ifdef _UNICODE
	return Utf8EncodeUcs2(str.GetBuffer());
#else
	return Utf8EncodeCP(str.GetBuffer(), CP_ACP);
#endif
}

CString MSIP::Utf8DecodeUni(CStringA str)
{
#ifdef _UNICODE
	LPTSTR msg;
	Utf8DecodeCP(str.GetBuffer(), CP_ACP, &msg);
	return msg;
#else
	return Utf8DecodeCP(str.GetBuffer(), CP_ACP, NULL);
#endif
}

CStringA MSIP::UnicodeToAnsi(CString str)
{
	CStringA res;
	int nCount = str.GetLength();
	for (int nIdx = 0; nIdx < nCount; nIdx++)
	{
		res += str[nIdx];
	}
	return res;
}

CString MSIP::AnsiToUnicode(CStringA str)
{
	CString res;
	int nCount = str.GetLength();
	for (int nIdx = 0; nIdx < nCount; nIdx++)
	{
		res += str[nIdx];
	}
	return res;
}

CString MSIP::AnsiToWideChar(char* str)
{
	CString res;
	int iNeeded = MultiByteToWideChar(CP_ACP, 0, str, -1, 0, 0);
	wchar_t *wlocal = res.GetBuffer((iNeeded + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, str, -1, wlocal, iNeeded);
	res.ReleaseBuffer();
	return res;
}

CStringA MSIP::StringToPjString(CString str)
{
	CStringA res;
	int len = str.GetLength() * 4;
	char *buf = res.GetBuffer(len);
	pj_unicode_to_ansi(str.GetBuffer(), -1, buf, len + 1);
	res.ReleaseBuffer();
	return res;
}

char* MSIP::WideCharToPjStr(CString str)
{
	int len = str.GetLength() * 4;
	char *buf = (char *)malloc(len + 1);
	pj_unicode_to_ansi(str.GetBuffer(), -1, buf, len + 1);
	return buf;
}

CString MSIP::PjStrToWideChar(char *str)
{
	CString res;
	int len = strlen(str) * 2;
	wchar_t *buf = res.GetBuffer(len);
	pj_ansi_to_unicode(str, -1, buf, len + 1);
	return res;
}

void MSIP::OpenURL(CString url)
{
	CString param;
	param.Format(_T("url.dll,FileProtocolHandler %s"), url);
	ShellExecute(NULL, NULL, _T("rundll32.exe"), param, NULL, SW_SHOWNORMAL);
}

CString MSIP::GetDuration(int sec, bool zero)
{
	CString duration;
	if (sec || zero) {
		int h, m, s;
		s = sec;
		h = s / 3600;
		s = s % 3600;
		m = s / 60;
		s = s % 60;
		if (h) {
			duration.Format(_T("%d:%02d:%02d"), h, m, s);
		}
		else {
			duration.Format(_T("%d:%02d"), m, s);
		}
	}
	return duration;
}

bool MSIP::IsPSTNNnmber(CString number)
{
	bool isDigits = true;
	for (int i = 0; i < number.GetLength(); i++)
	{
		if ((number[i] > '9' || number[i] < '0') && number[i] != '*' && number[i] != '#' && number[i] != '.' && number[i] != '-' && number[i] != '(' && number[i] != ')' && number[i] != '/' &&number[i] != ' ' && number[0] != '+')
		{
			isDigits = false;
			break;
		}
	}
	return isDigits;
}

bool MSIP::IniSectionExists(CString section, CString iniFile)
{
	CString str;
	LPTSTR ptr = str.GetBuffer(3);
	int result = GetPrivateProfileString(section, NULL, NULL, ptr, 3, iniFile);
	str.ReleaseBuffer();
	return result;
}

CString MSIP::Bin2String(CByteArray *ca)
{
	CString res;
	int k = ca->GetSize();
	for (int i = 0; i < k; i++) {
		unsigned char ch = ca->GetAt(i);
		res.AppendFormat(_T("%02x"), ca->GetAt(i));
	}
	return res;
}

void MSIP::String2Bin(CString str, CByteArray *res)
{
	res->RemoveAll();
	int k = str.GetLength();
	CStringA rab;
	for (int i = 0; i < str.GetLength(); i += 2) {
		rab = CStringA(str.Mid(i, 2));
		char *p = NULL;
		unsigned long bin = strtoul(rab.GetBuffer(), &p, 16);
		res->Add(bin);
	}
}

void MSIP::CommandLineToShell(CString cmd, CString &command, CString &params)
{
	cmd.Trim();
	command.Empty();
	params.Empty();
	int nArgs;
	LPWSTR *szArglist = CommandLineToArgvW(cmd, &nArgs);
	if (NULL == szArglist) {
		AfxMessageBox(_T("Wrong command: ") + cmd);
	}
	else for (int i = 0; i < nArgs; i++) {
		if (!i) {
			command = szArglist[i];
		}
		else {
			params.AppendFormat(_T("%s "), szArglist[i]);
		}
	}
	params.TrimRight();
	LocalFree(szArglist);
}

void MSIP::RunCmd(CString cmdLine, CString addParams, bool noWait)
{
	CString str, command, params;
	//if (cmdLine.Find('"') == -1) {
	//	cmdLine.Format(_T("\"%s\""), cmdLine);
	//}
	CommandLineToShell(cmdLine, command, params);
	params.AppendFormat(_T(" %s"), addParams);
	params.TrimLeft();

	SHELLEXECUTEINFO ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = command;
	ShExecInfo.lpParameters = params;
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = SW_HIDE;
	ShExecInfo.hInstApp = NULL;
	ShellExecuteEx(&ShExecInfo);
	if (!noWait) {
		DWORD res = WaitForSingleObject(ShExecInfo.hProcess, 10000);
	}
	CloseHandle(ShExecInfo.hProcess);
}

void MSIP::PortKnock()
{
	if (!accountSettings.portKnockerPorts.IsEmpty()) {
		CString host;
		CString ip;
		if (!accountSettings.portKnockerHost.IsEmpty()) {
			host = accountSettings.portKnockerHost;
		}
		else {
			host = MSIP::RemovePort(get_account_server());
		}
		if (!host.IsEmpty()) {
			AfxSocketInit();
			if (MSIP::IsIP(host)) {
				ip = host;
			}
			else {
				hostent *he = gethostbyname(CStringA(host));
				if (he) {
					ip = inet_ntoa(*((struct in_addr *) he->h_addr_list[0]));
				}
			}
			CSocket udpSocket;
			if (!ip.IsEmpty() && udpSocket.Create(0, SOCK_DGRAM, 0)) {
				int pos = 0;
				CString strPort = accountSettings.portKnockerPorts.Tokenize(_T(","), pos);
				while (pos != -1) {
					strPort.Trim();
					if (!strPort.IsEmpty()) {
						int port = StrToInt(strPort);
						if (port > 0 && port <= 65535) {
							char buf[6] = "knock";
							udpSocket.SendToEx(buf, sizeof(buf), port, ip);
							Sleep(200);
						}
					}
					strPort = accountSettings.portKnockerPorts.Tokenize(_T(","), pos);
				}
				udpSocket.Close();
			}
		}
	}
}

bool MSIP::IsConnectedToInternet()
{
	HRESULT hr = S_FALSE;
	try {
		INetworkListManager* pNetworkListManager;
		hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, __uuidof(INetworkListManager), (LPVOID*)&pNetworkListManager);
		if (SUCCEEDED(hr)) {
			VARIANT_BOOL isConnected = VARIANT_FALSE;
			hr = pNetworkListManager->get_IsConnectedToInternet(&isConnected);
			if (SUCCEEDED(hr)) {
				if (isConnected == VARIANT_TRUE) {
					return true;
				}
			}
			pNetworkListManager->Release();
		}
	}
	catch (...)
	{
	}
	return false;
}
