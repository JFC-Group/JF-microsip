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

#define THIS_FILENAME "global.cpp"

#include "global.h"
#include "settings.h"
#include "utf.h"
#include "langpack.h"
#include <afxinet.h>
#include <Psapi.h>
#include "atlrx.h"
#include "addons.h"

#ifdef UNICODE
#define CF_TEXT_T CF_UNICODETEXT
#else
#define CF_TEXT_T CF_TEXT
#endif

pjsua_transport_id transport_udp_local;
pjsua_transport_id transport_udp;
pjsua_transport_id transport_tcp;
pjsua_transport_id transport_tls;

struct call_tonegen_data *tone_gen = NULL;
pjsua_acc_id account;
CString password;
pjsua_acc_id account_local;
pjsua_conf_port_id msip_conf_port_id;
pjsua_call_id msip_conf_port_call_id;

int msip_audio_input;
int msip_audio_output;
int msip_audio_ring;

CList<pjmedia_port*, pjmedia_port*> DTMFTonegens;


CString customString;

CString FormatNumber(CString number, CString *commands, bool noTransform) {
	int pos = number.Find(',');
	if (pos > 0 && pos < number.GetLength() - 1) {
		if (commands) {
			*commands = number.Mid(pos);
		}
		number = number.Mid(0, pos);
	}
	CString numberFormated = number;
	pjsua_acc_id acc_id;
	pj_str_t pj_uri;
	bool isLocal = SelectSIPAccount(number, acc_id, pj_uri) && acc_id == account_local;
	if (!noTransform) {
		if (number.Find('<') == -1 || number.Find('>') == -1) {
			if (!isLocal) {
				bool addPrefix = false;
				if (MSIP::IsPSTNNnmber(number)) {
					numberFormated.Remove('.');
					numberFormated.Remove('-');
					numberFormated.Remove('(');
					numberFormated.Remove(')');
					numberFormated.Remove('/');
					numberFormated.Remove(' ');
					if (!accountSettings.account.dialingPrefix.IsEmpty() && numberFormated.GetLength() > 3) {
						if (numberFormated.Left(1) == _T("+")) {
							numberFormated = numberFormated.Mid(1);
						}
						addPrefix = true;
					}
				}
				if (addPrefix) {
					numberFormated = accountSettings.account.dialingPrefix + numberFormated;
				}
				if (!accountSettings.account.dialPlan.IsEmpty()) {
					CString dialPlan = accountSettings.account.dialPlan;
					dialPlan.Trim(_T(" ()"));
					pos = 0;
					bool matched = false;
					CString resToken = dialPlan.Tokenize(_T("|"), pos);
					while (!resToken.IsEmpty()) {
						CString newToken;
						CString replaceGroup;
						CStringList delayedReplaces;
						bool group = false;
						for (int i = 0; i < resToken.GetLength(); i++) {
							TCHAR c = resToken.GetAt(i);
							if (!group && c == '<') {
								group = true;
							}
							else if (group) {
								if (c != '>') {
									replaceGroup.AppendChar(c);
								}
								else {
									if (!replaceGroup.IsEmpty()) {
										int p = replaceGroup.Find(':');
										if (p == -1) {
											newToken.Append(replaceGroup);
										}
										else {
											CString match = replaceGroup.Left(p);
											CString replace = replaceGroup.Mid(p + 1, replaceGroup.GetLength() - p - 1);
											newToken.AppendFormat(_T("{%s}"), match);
											delayedReplaces.AddTail(replace);
										}
									}
									replaceGroup.Empty();
									group = false;
								}
							}
							else {
								newToken.AppendChar(c);
							}
						}
						newToken.Replace('.', '*');
						newToken.Replace('x', '.');
						newToken.Replace('X', '.');
						resToken.Format(_T("^%s$"), newToken);
						CAtlRegExp<> regex;
						REParseError parseStatus = regex.Parse(resToken, true);
						if (parseStatus == REPARSE_ERROR_OK) {
							CAtlREMatchContext<> mc;
							if (regex.Match(numberFormated, &mc)) {
								POSITION pos = delayedReplaces.GetHeadPosition();
								if (pos) {
									CString numberFormatedNew;
									int i = 0;
									const CAtlREMatchContext<>::RECHAR *szPrev = mc.m_Match.szStart;
									while (pos) {
										CString replace = delayedReplaces.GetNext(pos);
										const CAtlREMatchContext<>::RECHAR *szStart, *szEnd;
										mc.GetMatch(i, &szStart, &szEnd);
										int m = szPrev - mc.m_Match.szStart;
										int n = szStart - szPrev;
										numberFormatedNew.Append(numberFormated.Mid(m, n));
										numberFormatedNew.Append(replace);
										szPrev = szEnd;
										i++;
									}
									numberFormatedNew.Append(numberFormated.Right(mc.m_Match.szEnd - szPrev - 1));
									numberFormated = numberFormatedNew;
								}
								matched = true;
								break;
							}
						}
						resToken = dialPlan.Tokenize(_T("|"), pos);
					}
					if (!matched) {
						numberFormated.Empty();
					}
				}
			}
		}
	}
	return GetSIPURI(numberFormated, true, isLocal);
}

void AddTransportSuffix(CString &str, Account *account)
{
	if (account) {
		if (account->transport == _T("tcp") && transport_tcp != -1) {
			str.Append(_T(";transport=tcp"));
		}
		else if (account->transport == _T("tls") && transport_tls != -1) {
			str.Append(_T(";transport=tls"));
		}
	}
}

CString GetSIPURI(CString str, bool isSimple, bool isLocal, CString domain)
{
	CString rab = str;
	rab.MakeLower();
	int pos = rab.Find(_T("sip:"));
	if (pos == -1)
	{
		str = _T("sip:") + str;
	}
	pos = str.Find(_T("@"));
	if (!isLocal) {
		if (accountSettings.accountId && pos == -1) {
			str.Append(_T("@") + (!domain.IsEmpty() ? domain : get_account_domain()));
		}
	}
	else {
		if (pos == -1 && !accountSettings.accountLocal.domain.IsEmpty()) {
			str.Append(_T("@") + accountSettings.accountLocal.domain);
		}
	}
	if (str.GetAt(str.GetLength() - 1) == '>')
	{
		str = str.Left(str.GetLength() - 1);
		if (!isSimple) {
			AddTransportSuffix(str, isLocal ? &accountSettings.accountLocal : &accountSettings.account);
		}
		str += _T(">");
	}
	else {
		if (!isSimple) {
			AddTransportSuffix(str, isLocal ? &accountSettings.accountLocal : &accountSettings.account);
		}
		str = _T("<") + str + _T(">");
	}
	return str;
}

bool SelectSIPAccount(CString number, pjsua_acc_id &acc_id, pj_str_t &pj_uri)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return false;
	}
	SIPURI sipuri;
	MSIP::ParseSIPURI(number, &sipuri);
	if (pjsua_acc_is_valid(account) && pjsua_acc_is_valid(account_local)) {
		acc_id = account;
		if (get_account_domain() != sipuri.domain) {
			int pos = sipuri.domain.Find(_T(":"));
			CString domainWithoutPort = MSIP::RemovePort(sipuri.domain);
			if (domainWithoutPort.CompareNoCase(_T("localhost")) == 0 || MSIP::IsIP(domainWithoutPort)) {
				acc_id = account_local;
			}
		}
	}
	else if (pjsua_acc_is_valid(account)) {
		acc_id = account;
	}
	else if (pjsua_acc_is_valid(account_local)) {
		acc_id = account_local;
	}
	else {
		return false;
	}
	pj_uri = MSIP::StrToPjStr(GetSIPURI(number, false, acc_id == account_local));
	return true;
}

void OpenHelp(CString code)
{
	CString url = _T(_GLOBAL_HELP_WEBSITE);
	url.Append(_T("#"));
	MSIP::OpenURL(url + code);
}

struct call_tonegen_data *call_init_tonegen(pjsua_call_id call_id)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return NULL;
	}
	pj_status_t status;
	pj_pool_t *pool;
	struct call_tonegen_data *cd;
	pjsua_call_info ci;

	if (call_id != -1) {
		pjsua_call_get_info(call_id, &ci);

		if (ci.media_status != PJSUA_CALL_MEDIA_ACTIVE)
			return NULL;
	}

	pool = pjsua_pool_create("mycall", 512, 512);
	cd = PJ_POOL_ZALLOC_T(pool, struct call_tonegen_data);
	cd->pool = pool;

	status = pjmedia_tonegen_create(cd->pool, 8000, 1, 64, 16, 0, &cd->tonegen);
	if (status != PJ_SUCCESS) {
		return NULL;
	}
	pjsua_conf_add_port(cd->pool, cd->tonegen, &cd->toneslot);
	if (call_id != -1) {
		pjsua_conf_connect(cd->toneslot, ci.conf_slot);
	}
	else {
		if (accountSettings.localDTMF) {
			pjsua_conf_adjust_rx_level(cd->toneslot, 0.4);
			pjsua_conf_connect(cd->toneslot, 0);
		}
	}
	if (call_id != -1) {
		call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
		if (!user_data) {
			user_data = new call_user_data(call_id);
			pjsua_call_set_user_data(call_id, user_data);
		}
		user_data->CS.Lock();
		user_data->tonegen_data = cd;
		user_data->CS.Unlock();
	}
	return cd;
}


static UINT_PTR destroyDTMFPlayerTimer = NULL;
static UINT_PTR tonegenBusyTimer = NULL;

void destroyDTMFPlayerTimerHandler(
	HWND hwnd,
	UINT uMsg,
	UINT_PTR idEvent,
	DWORD dwTime)
{
	if (!tone_gen || pjsua_var.state != PJSUA_STATE_RUNNING || !pjmedia_tonegen_is_busy(tone_gen->tonegen)) {
		if (destroyDTMFPlayerTimer) {
			KillTimer(NULL, destroyDTMFPlayerTimer);
			destroyDTMFPlayerTimer = NULL;
		}
		call_deinit_tonegen(-1);
	}
}

void DTMFQueueTimerHandler(
	HWND hwnd,
	UINT uMsg,
	UINT_PTR idEvent,
	DWORD dwTime)
{
	KillTimer(hwnd, idEvent);
	pjsua_call_id call_id = (pjsua_call_id)idEvent;
	if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_call_is_active(call_id)) {
		call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
		if (user_data) {
			user_data->CS.Lock();
			if (!user_data->commands.IsEmpty()) {
				CString dtmf;
				int pos = user_data->commands.Find(',');
				if (pos != -1) {
					dtmf = user_data->commands.Mid(0, pos);
					user_data->commands = user_data->commands.Mid(pos + 1);
				}
				else {
					dtmf = user_data->commands;
					user_data->commands.Empty();
				}
				if (!dtmf.IsEmpty()) {
					msip_call_dial_dtmf(call_id, dtmf);
				}
				if (!user_data->commands.IsEmpty()) {
					::SetTimer(hwnd, idEvent, 1000 + 200 * dtmf.GetLength(), (TIMERPROC)DTMFQueueTimerHandler);
				}
			}
			user_data->CS.Unlock();
		}
	}
}

void tonegenBusyHandler(
	HWND hwnd,
	UINT uMsg,
	UINT_PTR idEvent,
	DWORD dwTime)
{
	POSITION pos = DTMFTonegens.GetHeadPosition();
	while (pos) {
		POSITION posKey = pos;
		pjmedia_port *port = DTMFTonegens.GetNext(pos);
		if (pjsua_var.state != PJSUA_STATE_RUNNING || pjmedia_tonegen_is_busy(port) == PJ_FALSE) {
			DTMFTonegens.RemoveAt(posKey);
		}
	};
	if (DTMFTonegens.IsEmpty()) {
		KillTimer(NULL, tonegenBusyTimer);
		tonegenBusyTimer = NULL;
		CWnd *hWnd = AfxGetApp()->m_pMainWnd;
		if (hWnd) {
			hWnd->PostMessage(UM_REFRESH_LEVELS, NULL, NULL);
		}
	}
}

void msip_set_sound_device(int outDev, bool forse) {
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	int in, out;
	if (forse || msip_audio_input == -1 || pjsua_get_snd_dev(&in, &out) != PJ_SUCCESS || msip_audio_input != in || outDev != out) {
		pjsua_snd_dev_param params;
		pjsua_snd_dev_param_default(&params);
		params.capture_dev = msip_audio_input;
		params.playback_dev = outDev;
		if (pjsua_set_snd_dev2(&params) != PJ_SUCCESS) {
			params.mode |= PJSUA_SND_DEV_SPEAKER_ONLY;
			pjsua_set_snd_dev2(&params);
		}
	}
}

bool msip_call_statistics(call_user_data *user_data, float *MOS)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return false;
	}
	if (!pjsua_call_has_media(user_data->call_id)) {
		return false;
	}
	pjsua_stream_stat stat;
	pj_status_t status = pjsua_call_get_stream_stat(user_data->call_id, 0, &stat);
	if (status != PJ_SUCCESS) {
		return false;
	}

	int LOCAL_DELAY = 30;
	float R;
	float a = 0.0f;
	float b = 19.8f;
	float c = 29.7f;
	float rx_loss = 0.0;
	float rx_jit = 0.0;
	float avg_latency = 0.0;

	int pkt_last = stat.rtcp.rx.pkt - user_data->rx_pkt_prev;
	int loss_last = stat.rtcp.rx.loss - user_data->rx_loss_prev;
	rx_loss = (pkt_last == 0) ? 1.0f : ((float)loss_last / (float)(pkt_last + loss_last));
	user_data->rx_pkt_prev = stat.rtcp.rx.pkt;
	user_data->rx_loss_prev = stat.rtcp.rx.loss;

	rx_jit = (float)stat.rtcp.rx.jitter.last / 1000;

	avg_latency = (stat.rtcp.rtt.last / 2000.0f) + LOCAL_DELAY + PJMEDIA_SND_DEFAULT_PLAY_LATENCY +
		PJMEDIA_SND_DEFAULT_REC_LATENCY + rx_jit;

	{
		float d = avg_latency;
		float d2 = d - 177.3f;
		float Id = 0.024f * d + 0.11f * (d - 177.3f) * (d2 < 0 ? 0 : 1);
		float P = rx_loss;
		float Ie = a + b * (float)log(1 + c * P);
		R = 94.2f - Id - Ie;
	}
	if (R < 0) {
		*MOS = 1;
	}
	else if (R > 100) {
		*MOS = 4.5;
	}
	else {
		*MOS = 1 + 0.035f * R + 7.10f / 1000000 * R * (R - 60) * (100 - R);
	}
	return true;
}

void msip_call_dial_dtmf(pjsua_call_id call_id, CString digits)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	bool simulate = true;
	if (call_id != PJSUA_INVALID_ID) {
		pjsua_call_info call_info;
		pjsua_call_get_info(call_id, &call_info);
		if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
			pj_str_t pj_digits = MSIP::StrToPjStr(digits);
			if (accountSettings.DTMFMethod == 1) {
				// in-band
				simulate = !call_play_digit(call_id, MSIP::StrToPj(digits));
			}
			else if (accountSettings.DTMFMethod == 2) {
				// RFC2833
				pjsua_call_dial_dtmf(call_id, &pj_digits);
			}
			else if (accountSettings.DTMFMethod == 3) {
				// sip-info
				msip_call_send_dtmf_info(call_id, pj_digits);
			}
			else {
				// auto
				if (pjsua_call_dial_dtmf(call_id, &pj_digits) != PJ_SUCCESS) {
					simulate = !call_play_digit(call_id, MSIP::StrToPj(digits));
				}
			}
		}
	}
	if (simulate && accountSettings.localDTMF) {
		msip_set_sound_device(msip_audio_output);
		call_play_digit(-1, MSIP::StrToPj(digits));
	}
}

BOOL call_play_digit(pjsua_call_id call_id, const char *digits, int duration)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return FALSE;
	}
	pjmedia_tone_digit d[16];
	unsigned i, count = strlen(digits);
	struct call_tonegen_data *cd;
	call_user_data *user_data = NULL;
	if (call_id != -1) {
		user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
		cd = NULL;
		if (user_data) {
			user_data->CS.Lock();
			if (user_data->tonegen_data) {
				cd = user_data->tonegen_data;
			}
			user_data->CS.Unlock();
		}
	}
	else {
		cd = tone_gen;
	}
	if (!cd)
		cd = call_init_tonegen(call_id);
	if (!cd)
		return FALSE;
	if (call_id == -1) {
		tone_gen = cd;
	}

	if (count > PJ_ARRAY_SIZE(d))
		count = PJ_ARRAY_SIZE(d);

	pj_bzero(d, sizeof(d));
	for (i = 0; i < count; ++i) {
		d[i].digit = digits[i];
		d[i].on_msec = duration;
		d[i].off_msec = 50;
		d[i].volume = call_id == -1 ? (1 + accountSettings.volumeRing * PJMEDIA_TONEGEN_VOLUME / 100) : 0;
	}

	if (call_id != -1) {
		// mute microphone before play in-band tones
		msip_audio_input_set_volume(0, true);
		if (DTMFTonegens.Find(cd->tonegen) == NULL) {
			DTMFTonegens.AddTail(cd->tonegen);
		}
		if (tonegenBusyTimer) {
			KillTimer(NULL, tonegenBusyTimer);
		}
		tonegenBusyTimer = SetTimer(NULL, NULL, 800, (TIMERPROC)tonegenBusyHandler);

	}

	pjmedia_tonegen_play_digits(cd->tonegen, count, d, 0);

	if (call_id == -1) {
		if (destroyDTMFPlayerTimer) {
			KillTimer(NULL, destroyDTMFPlayerTimer);
		}
		destroyDTMFPlayerTimer = SetTimer(NULL, NULL, 5000, (TIMERPROC)destroyDTMFPlayerTimerHandler);
	}
	return TRUE;
}

void call_deinit_tonegen(pjsua_call_id call_id)
{
	struct call_tonegen_data *cd;
	call_user_data *user_data = NULL;

	if (call_id != -1) {
		if (pjsua_var.state == PJSUA_STATE_RUNNING) {
			user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
		}
		cd = NULL;
		if (user_data) {
			user_data->CS.Lock();
			if (user_data->tonegen_data) {
				cd = user_data->tonegen_data;
				POSITION position = DTMFTonegens.Find(cd->tonegen);
				if (position != NULL) {
					DTMFTonegens.RemoveAt(position);
				}
			}
			user_data->CS.Unlock();
		}
	}
	else {
		cd = tone_gen;
	}
	if (!cd)
		return;

	if (pjsua_var.state == PJSUA_STATE_RUNNING) {
		pjsua_conf_remove_port(cd->toneslot);
		pjmedia_port_destroy(cd->tonegen);
		pj_pool_release(cd->pool);
	}

	if (call_id != -1) {
		if (user_data) {
			user_data->CS.Lock();
			user_data->tonegen_data = NULL;
			user_data->CS.Unlock();
		}
	}
	else {
		tone_gen = NULL;
	}
}

bool call_hangup_all_noincoming(bool onHold)
{
	bool res = false;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			pjsua_call_info call_info;
			pjsua_call_get_info(call_ids[i], &call_info);
			if (call_info.role != PJSIP_ROLE_UAS || (call_info.state != PJSIP_INV_STATE_INCOMING && call_info.state != PJSIP_INV_STATE_EARLY)) {
				if (onHold && call_info.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD) {
					continue;
				}
				msip_call_hangup_fast(call_ids[i], &call_info);
				res = true;
			}
		}
	}
	return res;
}

bool call_hangup_incoming()
{
	bool res = false;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			pjsua_call_info call_info;
			pjsua_call_get_info(call_ids[i], &call_info);
			if (call_info.role == PJSIP_ROLE_UAS && (call_info.state == PJSIP_INV_STATE_INCOMING || call_info.state == PJSIP_INV_STATE_EARLY)) {
				msip_call_hangup_fast(call_ids[i], &call_info);
				res = true;
			}
		}
	}
	return res;
}

void call_hangup_calling()
{
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			pjsua_call_info call_info;
			pjsua_call_get_info(call_ids[i], &call_info);
			if (call_info.role == PJSIP_ROLE_UAC && call_info.state != PJSIP_INV_STATE_CONFIRMED) {
				msip_call_hangup_fast(call_ids[i], &call_info);
			}
		}
	}
}

void call_hangup_all()
{
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			msip_call_end(call_ids[i]);
		}
	}
}

static char _x2c(char hex_up, char hex_low)
{
	char digit;

	digit = 16 * (hex_up >= 'A'
		? ((hex_up & 0xdf) - 'A') + 10 : (hex_up - '0'));
	digit += (hex_low >= 'A'
		? ((hex_low & 0xdf) - 'A') + 10 : (hex_low - '0'));
	return (digit);
}

CStringA urldecode(CStringA str)
{
	CStringA res;
	if (str) {
		for (int j = 0; j < str.GetLength(); j++) {
			switch (str.GetAt(j)) {
			case '+':
				res.AppendChar(' ');
				break;
			case '%':
				res.AppendChar(_x2c(str.GetAt(j + 1), str.GetAt(j + 2)));
				j += 2;
				break;
			default:
				res.AppendChar(str.GetAt(j));
				break;
			}
		}
	}
	return res;
}

CStringA urlencode(CStringA str)
{
	CStringA escaped;
	int max = str.GetLength();
	for (int i = 0; i < max; i++)
	{
		const char chr = str.GetAt(i);
		if ((48 <= chr && chr <= 57) ||//0-9
			(65 <= chr && chr <= 90) ||//abc...xyz
			(97 <= chr && chr <= 122) || //ABC...XYZ
			(chr == '~' || chr == '!' || chr == '*' || chr == '(' || chr == ')' || chr == '\'')
			)
		{
			escaped.AppendFormat("%c", chr);
		}
		else
		{
			escaped.Append("%");
			escaped.Append(char2hex(chr));//converts char 255 to string "ff"
		}
	}
	return escaped;
}

CStringA char2hex(char dec)
{
	char dig1 = (dec & 0xF0) >> 4;
	char dig2 = (dec & 0x0F);
	if (0 <= dig1 && dig1 <= 9) dig1 += 48;    //0,48inascii
	if (10 <= dig1 && dig1 <= 15) dig1 += 97 - 10; //a,97inascii
	if (0 <= dig2 && dig2 <= 9) dig2 += 48;
	if (10 <= dig2 && dig2 <= 15) dig2 += 97 - 10;

	CStringA r;
	r.AppendFormat("%c", dig1);
	r.AppendFormat("%c", dig2);
	return r;
}

static DWORD WINAPI URLGetAsyncThread(LPVOID lpParam)
{
	URLGetAsyncData *data = (URLGetAsyncData *)lpParam;
	data->body.Empty();
	data->statusCode = 0;
	if (!data->url.IsEmpty()) {
		try {
			CInternetSession session;
			CHttpConnection* pHttp = NULL;
			CHttpFile* pFile = NULL;
			DWORD dwServiceType;
			CString strServer;
			CString strObject;
			INTERNET_PORT nPort;
			CString strUsername;
			CString strPassword;
			if (AfxParseURLEx(data->url, dwServiceType, strServer, strObject, nPort, strUsername, strPassword)) {
				if (strUsername.IsEmpty()) {
					strUsername = data->username;
					strPassword = data->password;
				}
				pHttp = session.GetHttpConnection(strServer, (dwServiceType == AFX_INET_SERVICE_HTTPS ? INTERNET_FLAG_SECURE : 0), nPort);
				CStringA strFormData;
				CString requestHeaders = data->headers;
				data->headers.Empty();
				if (data->post) {
					if (!data->postData.IsEmpty()) {
						strFormData = data->postData;
					} else {
						int pos = strObject.Find(_T("?"));
						if (pos != -1) {
							strFormData = Utf8EncodeUcs2(strObject.Mid(pos + 1));
							strObject = strObject.Left(pos);
						}
					}
					if (requestHeaders.IsEmpty()) {
						requestHeaders = _T("Content-Type: application/x-www-form-urlencoded");
					}
				}
				pFile = pHttp->OpenRequest(data->post || !data->postData.IsEmpty() ? CHttpConnection::HTTP_VERB_POST : CHttpConnection::HTTP_VERB_GET, strObject, 0, 1, 0, 0,
					INTERNET_FLAG_TRANSFER_BINARY |
					INTERNET_FLAG_RELOAD |
					INTERNET_FLAG_DONT_CACHE |
					(dwServiceType == AFX_INET_SERVICE_HTTPS ? INTERNET_FLAG_SECURE : 0) |
					(!strUsername.IsEmpty() && !strPassword.IsEmpty() ? INTERNET_FLAG_KEEP_CONNECTION : 0)
				);
				if (dwServiceType == AFX_INET_SERVICE_HTTPS) {
					pFile->SetOption(INTERNET_OPTION_SECURITY_FLAGS,
						SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
						SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
						SECURITY_FLAG_IGNORE_UNKNOWN_CA |
						SECURITY_FLAG_IGNORE_WRONG_USAGE
					);
				}
				if (!strUsername.IsEmpty() && !strPassword.IsEmpty()) {
					pFile->SetOption(INTERNET_OPTION_USERNAME, strUsername.GetBuffer(), strUsername.GetLength());
					pFile->SetOption(INTERNET_OPTION_PASSWORD, strPassword.GetBuffer(), strPassword.GetLength());
				}
				pFile->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000);

				bool status = pFile->SendRequest(requestHeaders, (LPVOID)strFormData.GetBuffer(), strFormData.GetLength());
				if (status) {
					pFile->QueryInfoStatusCode(data->statusCode);
					CStringA buf;
					int i;
					UINT len = 0;
					do {
						LPSTR p = data->body.GetBuffer(len + 1024);
						i = pFile->Read(p + len, 1024);
						len += i;
						data->body.ReleaseBuffer(len);
					} while (i > 0);
					//--
					pFile->QueryInfo(
						HTTP_QUERY_RAW_HEADERS_CRLF,
						data->headers
					);
					pFile->Close();
				}
				session.Close();
			}
			else {
				data->statusCode = 0;
			}
		}
		catch (CInternetException *e) {
			data->statusCode = 0;
		}
	}
	if (data->message) {
		if (data->hWnd) {
			PostMessage(data->hWnd, data->message, (WPARAM)data, 0);
		}
	}
	else {
		delete data;
	}
	return 0;
}

void URLGetAsync(CString url, HWND hWnd, UINT message, bool post, CString postData, CString headers, CString username, CString password, void* userData)
{
	HANDLE hThread;
	URLGetAsyncData *data = new URLGetAsyncData();
	data->hWnd = hWnd;
	data->message = message;
	data->statusCode = 0;
	data->url = url;
	data->post = post;
	data->postData = postData;
	data->headers = headers;
	data->username = username;
	data->password = password;
	data->userData = userData;
	if (!CreateThread(NULL, 0, URLGetAsyncThread, data, 0, NULL)) {
		data->url.Empty();
		URLGetAsyncThread(data);
	}
}

URLGetAsyncData URLGetSync(CString url, bool post, CString postData, CString headers, CString username, CString password, void* userData)
{
	URLGetAsyncData data;
	data.hWnd = 0;
	data.message = 1;
	data.statusCode = 0;
	data.url = url;
	data.post = post;
	data.postData = postData;
	data.headers = headers;
	data.username = username;
	data.password = password;
	data.userData = userData;
	URLGetAsyncThread(&data);
	return data;
}

CString get_account_username()
{
	CString res = accountSettings.account.username;
	return res;
}

CString get_account_password()
{
	CString res = accountSettings.account.password;
	if (!password.IsEmpty()) {
		res = password;
	}
		return res;
}

CString get_account_domain()
{
	CString res;
	res.Append(accountSettings.account.domain);
	return res;
}

CString get_account_server()
{
	CString res;
	res.Append(accountSettings.account.server);
	return res;
}

void get_account_proxy(Account *account, CStringList &proxies)
{
	proxies.RemoveAll();
	int pos = 0;
	CString proxy = account->proxy;
	CString resToken = proxy.Tokenize(_T(" "), pos);
	while (!resToken.IsEmpty()) {
		proxies.AddTail(resToken);
		resToken = proxy.Tokenize(_T(" "), pos);
	}
}

CString get_public_addr(Account *account)
{
	CString res = account ? account->publicAddr : accountSettings.account.publicAddr;
	return res;
}

CString URLMask(CString url, SIPURI* sipuri, pjsua_acc_id acc, call_user_data *user_data, pjsua_call_info* call_info)
{
	//-- replace server
	CString str;
	if (accountSettings.accountId) {
		str = get_account_server();
	}
	url.Replace(_T("{server}"), str.IsEmpty() ? _T("localhost") : str);
	url.Replace(_T("{extension}"), accountSettings.accountId ? get_account_username() : _T(""));
	int duration = 0;
	if (user_data) {
		user_data->CS.Lock();
		duration = user_data->duration;
		user_data->CS.Unlock();
	}
	str.Format(_T("%d"), duration);
	url.Replace(_T("{duration}"), str);
	//--
	CTime t = CTime::GetCurrentTime();
	time_t time = t.GetTime();
	str.Format(_T("%d"), time);
	url.Replace(_T("{time}"), str);
	//--
	url.Replace(_T("{callid}"), CString(urlencode(Utf8EncodeUcs2(
		call_info ? MSIP::PjToStr(&call_info->call_id, TRUE) : _T("")
	))));
	url.Replace(_T("{direction}"), CString(urlencode(Utf8EncodeUcs2(
		call_info ? (call_info->role ? _T("out") : _T("in")) : _T("")
	))));
	if (sipuri) {
		//-- replace callerid
		CString num = !sipuri->name.IsEmpty() ? sipuri->name : sipuri->user;
		url.Replace(_T("{callerid}"), CString(urlencode(Utf8EncodeUcs2(num))));
		//-- replace
		url.Replace(_T("{user}"), CString(urlencode(Utf8EncodeUcs2(sipuri->user))));
		url.Replace(_T("{number}"), CString(urlencode(Utf8EncodeUcs2(sipuri->user))));
		url.Replace(_T("{domain}"), CString(urlencode(Utf8EncodeUcs2(sipuri->domain))));
		url.Replace(_T("{name}"), CString(urlencode(Utf8EncodeUcs2(sipuri->name))));
		//--
	}
	return url;
}

HICON LoadImageIcon(int i)
{
	return (HICON)LoadImage(
		AfxGetInstanceHandle(),
		MAKEINTRESOURCE(i),
		IMAGE_ICON, 0, 0, LR_SHARED);
}

void msip_call_send_dtmf_info(pjsua_call_id current_call, pj_str_t digits)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	if (current_call == -1) {
		PJ_LOG(3, (THIS_FILENAME, "No current call"));
	}
	else {
		const pj_str_t SIP_INFO = pj_str("INFO");
		int call = current_call;
		pj_status_t status;
		for (int i = 0; i < digits.slen; ++i) {
			char body[80];
			pjsua_msg_data msg_data_;

			pjsua_msg_data_init(&msg_data_);
			msg_data_.content_type = pj_str("application/dtmf-relay");

			pj_ansi_snprintf(body, sizeof(body),
				"Signal=%c\r\n"
				"Duration=160",
				digits.ptr[i]);
			msg_data_.msg_body = pj_str(body);

			status = pjsua_call_send_request(current_call, &SIP_INFO,
				&msg_data_);
			if (status != PJ_SUCCESS) {
				return;
			}
		}
	}
}

void msip_call_hangup_fast(pjsua_call_id call_id, pjsua_call_info *p_call_info)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	pjsua_call_info call_info;
	if (!p_call_info) {
		if (pjsua_call_get_info(call_id, &call_info) == PJ_SUCCESS) {
			p_call_info = &call_info;
		}
	}
	if (!p_call_info) {
		return;
	}
	if (p_call_info->conf_slot != PJSUA_INVALID_ID) {
		pjsua_conf_disconnect(p_call_info->conf_slot, 0);
		pjsua_conf_disconnect(0, p_call_info->conf_slot);
	}
	call_user_data* user_data = (call_user_data*)pjsua_call_get_user_data(call_id);
	if (pjsua_call_hangup(call_id, 0, NULL, NULL) == PJ_SUCCESS) {
		mainDlg->messagesDlg->OnEndCall(p_call_info, user_data);
	}
}

void msip_call_end(pjsua_call_id call_id)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
	if (user_data) {
		user_data->CS.Lock();
		user_data->hangup = true;
		if (user_data->inConference) {
			pjsua_call_info call_info;
			if (pjsua_call_get_info(call_id, &call_info) == PJ_SUCCESS && call_info.state == PJSIP_INV_STATE_CONFIRMED) {
				pjsua_call_id call_ids[PJSUA_MAX_CALLS];
				unsigned count = PJSUA_MAX_CALLS;
				if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
					for (unsigned i = 0; i < count; ++i) {
						if (call_id == call_ids[i]) {
							continue;
						}
						call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
						bool inConferenceCurr = false;
						if (user_data_curr) {
							user_data_curr->CS.Lock();
							if (user_data_curr->inConference) {
								inConferenceCurr = true;
							}
							user_data_curr->CS.Unlock();
						}
						if (inConferenceCurr) {
							msip_call_hangup_fast(call_ids[i]);
						}
					}
				}
			}
		}
		user_data->CS.Unlock();
	}
	msip_call_hangup_fast(call_id);
}

void msip_conference_join(pjsua_call_info *call_info)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	if (user_data) {
		user_data->CS.Lock();
		if (user_data->inConference) {
			pjsua_call_id call_ids[PJSUA_MAX_CALLS];
			unsigned count = PJSUA_MAX_CALLS;
			if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
				for (unsigned i = 0; i < count; ++i) {
					if (call_info->id == call_ids[i]) {
						continue;
					}
					if (!pjsua_call_has_media(call_ids[i])) {
						continue;
					}
					call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
					bool inConferenceCurr = false;
					bool isRecordingCurr = false;
					if (user_data_curr) {
						user_data_curr->CS.Lock();
						if (user_data_curr->inConference) {
							inConferenceCurr = true;
						}
						if (user_data_curr->recorder_id != PJSUA_INVALID_ID) {
							isRecordingCurr = true;
						}
						user_data_curr->CS.Unlock();
					}
					if (inConferenceCurr) {
						if (call_info->conf_slot != PJSUA_INVALID_ID) {
							pjsua_conf_port_id conf_port_id = pjsua_call_get_conf_port(call_ids[i]);
							if (conf_port_id != PJSUA_INVALID_ID) {
								pjsua_conf_connect(call_info->conf_slot, conf_port_id);
								pjsua_conf_connect(conf_port_id, call_info->conf_slot);
							}
						}
						if (isRecordingCurr) {
							msip_call_recording_start(user_data, call_info);
						}
						else if (user_data->recorder_id != PJSUA_INVALID_ID) {
							msip_call_recording_start(user_data_curr);
						}
						user_data_curr->CS.Unlock();
						CWnd *hWnd = AfxGetApp()->m_pMainWnd;
						if (hWnd) {
							hWnd->PostMessage(UM_TAB_ICON_UPDATE, (WPARAM)call_ids[i], NULL);
						}
					}
				}
			}
			CWnd *hWnd = AfxGetApp()->m_pMainWnd;
			if (hWnd) {
				hWnd->PostMessage(UM_TAB_ICON_UPDATE, (WPARAM)call_info->id, NULL);
			}
		}
		user_data->CS.Unlock();
	}
}

void msip_conference_leave(pjsua_call_info *call_info, call_user_data *user_data, bool hold)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	if (!user_data) {
		user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	}
	if (user_data) {
		user_data->CS.Lock();
		if (user_data->inConference) {
			if (user_data->recorder_id != PJSUA_INVALID_ID) {
				msip_call_recording_stop(user_data);
			}
			pjsua_call_id call_ids[PJSUA_MAX_CALLS];
			unsigned count = PJSUA_MAX_CALLS;
			if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
				int qty = 0;
				call_user_data *last_conf_user_data = NULL;
				for (unsigned i = 0; i < count; ++i) {
					if (call_info->id == call_ids[i]) {
						continue;
					}
					call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
					bool inConferenceCurr = false;
					if (user_data_curr) {
						user_data_curr->CS.Lock();
						if (user_data_curr->inConference) {
							inConferenceCurr = true;
						}
						user_data_curr->CS.Unlock();
					}
					if (inConferenceCurr) {
						last_conf_user_data = user_data_curr;
						qty++;
						if (call_info->conf_slot != PJSUA_INVALID_ID) {
							pjsua_conf_port_id conf_port_id = pjsua_call_get_conf_port(call_ids[i]);
							if (conf_port_id != PJSUA_INVALID_ID) {
								pjsua_conf_disconnect(call_info->conf_slot, conf_port_id);
								pjsua_conf_disconnect(conf_port_id, call_info->conf_slot);
							}
						}
						if (!hold) {
							CWnd *hWnd = AfxGetApp()->m_pMainWnd;
							if (hWnd) {
								hWnd->PostMessage(UM_TAB_ICON_UPDATE, (WPARAM)call_ids[i], NULL);
							}
						}
					}
				}
				if (qty == 1) {
					if (!hold) {
						last_conf_user_data->CS.Lock();
						last_conf_user_data->inConference = false;
						last_conf_user_data->CS.Unlock();
						CWnd *hWnd = AfxGetApp()->m_pMainWnd;
						if (hWnd) {
							hWnd->PostMessage(UM_TAB_ICON_UPDATE, (WPARAM)call_info->id, NULL);
						}
					}
				}
			}
			if (!hold) {
				user_data->inConference = false;
			}
		}
		user_data->CS.Unlock();
	}
}

void msip_call_hold(pjsua_call_info *call_info)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	if (user_data) {
		user_data->CS.Lock();
		if (user_data->inConference) {
			pjsua_call_id call_ids[PJSUA_MAX_CALLS];
			unsigned count = PJSUA_MAX_CALLS;
			if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
				for (unsigned i = 0; i < count; ++i) {
					if (call_ids[i] != call_info->id) {
						call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
						bool inConferenceCurr = false;
						if (user_data_curr) {
							user_data_curr->CS.Lock();
							if (user_data_curr->inConference) {
								inConferenceCurr = true;
							}
							user_data_curr->CS.Unlock();
						}
						if (inConferenceCurr) {
							pjsua_call_info call_info_curr;
							pjsua_call_get_info(call_ids[i], &call_info_curr);
							if (call_info_curr.state == PJSIP_INV_STATE_CONFIRMED) {
								if (call_info_curr.media_status != PJSUA_CALL_MEDIA_LOCAL_HOLD && call_info_curr.media_status != PJSUA_CALL_MEDIA_NONE) {
									pjsua_call_set_hold(call_info_curr.id, NULL);
								}
							}
						}
					}
				}
			}
		}
		user_data->CS.Unlock();
	}
	if (call_info->state == PJSIP_INV_STATE_CONFIRMED) {
		if (call_info->media_status != PJSUA_CALL_MEDIA_LOCAL_HOLD && call_info->media_status != PJSUA_CALL_MEDIA_NONE) {
			pjsua_call_set_hold(call_info->id, NULL);
		}
	}
}

void msip_call_unhold(pjsua_call_info *call_info)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	call_user_data *user_data = NULL;
	if (call_info) {
		user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	}
	bool inConference = false;
	if (user_data) {
		user_data->CS.Lock();
		if (user_data->inConference) {
			inConference = true;
		}
		user_data->CS.Unlock();
	}
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned count = PJSUA_MAX_CALLS;
	if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < count; ++i) {
			if (!call_info || call_ids[i] != call_info->id) {
				pjsua_call_info call_info_curr;
				pjsua_call_get_info(call_ids[i], &call_info_curr);
				if (call_info_curr.state == PJSIP_INV_STATE_CONFIRMED) {
					call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
					bool inConferenceCurr = false;
					if (user_data_curr) {
						user_data_curr->CS.Lock();
						if (user_data_curr->inConference) {
							inConferenceCurr = true;
						}
						user_data_curr->CS.Unlock();
					}
					if (inConference && inConferenceCurr) {
						// unhold
						if (call_info_curr.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD || call_info_curr.media_status == PJSUA_CALL_MEDIA_NONE) {
							pjsua_call_reinvite(call_ids[i], PJSUA_CALL_UNHOLD, NULL);
						}
					}
					else {
						// hold
						if (call_info_curr.media_status != PJSUA_CALL_MEDIA_LOCAL_HOLD && call_info_curr.media_status != PJSUA_CALL_MEDIA_NONE) {
							if (accountSettings.singleMode || !accountSettings.AC) {
								pjsua_call_set_hold(call_ids[i], NULL);
							}
						}
					}
				}
			}
		}
	}
	if (call_info && call_info->state == PJSIP_INV_STATE_CONFIRMED) {
		if (call_info->media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD || call_info->media_status == PJSUA_CALL_MEDIA_NONE) {
			pjsua_call_reinvite(call_info->id, PJSUA_CALL_UNHOLD, NULL);
		}
	}
}

bool msip_call_answer(pjsua_call_id call_id)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return false;
	}
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned calls_count = PJSUA_MAX_CALLS;
	unsigned calls_count_cmp = 0;
	if (pjsua_enum_calls(call_ids, &calls_count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < calls_count; ++i) {
			pjsua_call_info call_info;
			if (pjsua_call_get_info(call_ids[i], &call_info) == PJ_SUCCESS) {
				if (call_info.role == PJSIP_ROLE_UAS && (call_info.state == PJSIP_INV_STATE_INCOMING || call_info.state == PJSIP_INV_STATE_EARLY)) {
					CWnd *hWnd = AfxGetApp()->m_pMainWnd;
					if (hWnd) {
						hWnd->PostMessage(UM_CALL_ANSWER, (WPARAM)call_ids[i], NULL);
						return true;
					}
					break;
				}
			}
		}
	}
	return false;
}

void msip_call_busy(pjsua_call_id call_id, CString reason)
{
	//or Reason: Q.850 ;cause=17 ; text="User busy..."
	if (!reason.IsEmpty()) {
		pj_str_t pj_reason = MSIP::StrToPjStr(reason);
		pjsua_call_hangup(call_id, 486, &pj_reason, NULL);
	}
	else {
		pjsua_call_hangup(call_id, 486, NULL, NULL);
	}
}

void msip_call_recording_start(call_user_data *user_data, pjsua_call_info *call_info, int id)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	if (user_data) {
		user_data->CS.Lock();
		pjsua_recorder_id *recorder_id = &user_data->recorder_id;
		if (*recorder_id == PJSUA_INVALID_ID) {
			pjsua_call_info call_info_loc;
			if (!call_info) {
				if (pjsua_call_get_info(user_data->call_id, &call_info_loc) == PJ_SUCCESS) {
					call_info = &call_info_loc;
				}
			}
			if (call_info && call_info->conf_slot != PJSUA_INVALID_ID) {
				if (user_data->inConference) {
					pjsua_call_id call_ids[PJSUA_MAX_CALLS];
					unsigned count = PJSUA_MAX_CALLS;
					if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
						for (unsigned i = 0; i < count; ++i) {
							if (call_info->id == call_ids[i]) {
								continue;
							}
							call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
							if (user_data_curr) {
								user_data_curr->CS.Lock();
								if (user_data_curr->inConference && user_data_curr->recorder_id != PJSUA_INVALID_ID) {
									pjsua_conf_port_id rec_conf_port_id = pjsua_recorder_get_conf_port(user_data_curr->recorder_id);
									pjsua_conf_connect(call_info->conf_slot, rec_conf_port_id);
									*recorder_id = user_data_curr->recorder_id;
									user_data_curr->CS.Unlock();
									break;
								}
								user_data_curr->CS.Unlock();
							}
						}
					}
				}
				if (*recorder_id == PJSUA_INVALID_ID) {
					CString filename;
					SIPURI remoteURI;
					MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->remote_info, TRUE), &remoteURI);
					CTime tm = CTime::GetCurrentTime();
					CString recordingPath = accountSettings.recordingPath;
					if (!recordingPath.IsEmpty() && recordingPath.Right(1) != _T("\\")) {
						recordingPath.Append(_T("\\"));
					}
					SIPURI localURI;
					MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->local_info, TRUE), &localURI);
					filename.Format(_T("%s-%s-%s-%s"),
						tm.Format(_T("%Y%m%d-%H%M%S")),
						remoteURI.user,
						call_info->role == PJSIP_ROLE_UAC ? _T("outgoing") : _T("incoming"),
						accountSettings.accountId && !accountSettings.account.label.IsEmpty() ? accountSettings.account.label : localURI.user
					);
					if (!recordingPath.IsEmpty()) {
						CreateDirectory(recordingPath, NULL);
					}
					char spec[] = { '/','\\', '?', '%', '*', ':', '|', '"', '<', '>', '.', ' ' };
					for (int i = 0; i < sizeof(spec); i++) {
						filename.Replace(spec[i], '_');
					}
					filename = recordingPath + filename;
					//--
					if (accountSettings.recordingFormat == _T("wav")) {
						filename.Append(_T(".wav"));
					}
					else {
						filename.Append(_T(".mp3"));
					}
					char *buf = MSIP::WideCharToPjStr(filename);
					if (pjsua_recorder_create(&pj_str(buf), 0, NULL, -1, 0, recorder_id) == PJ_SUCCESS) {
						pjsua_conf_port_id rec_conf_port_id = pjsua_recorder_get_conf_port(*recorder_id);
						pjsua_conf_connect(call_info->conf_slot, rec_conf_port_id);
						pjsua_conf_connect(0, rec_conf_port_id);
						if (user_data->inConference) {
							pjsua_call_id call_ids[PJSUA_MAX_CALLS];
							unsigned count = PJSUA_MAX_CALLS;
							if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
								for (unsigned i = 0; i < count; ++i) {
									if (call_info->id == call_ids[i]) {
										continue;
									}
									call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
									if (user_data_curr) {
										user_data_curr->CS.Lock();
										if (user_data_curr->inConference && user_data_curr->recorder_id == PJSUA_INVALID_ID) {
											pjsua_call_info call_info_curr;
											pjsua_call_get_info(call_ids[i], &call_info_curr);
											if (call_info_curr.conf_slot != PJSUA_INVALID_ID) {
												pjsua_conf_connect(call_info_curr.conf_slot, rec_conf_port_id);
												user_data_curr->recorder_id = *recorder_id;
											}
										}
										user_data_curr->CS.Unlock();
									}
								}
							}
						}
					}
					delete buf;
					//--
				}
			}
		}
		user_data->CS.Unlock();
	}
}

void msip_call_recording_stop(call_user_data *user_data, int id, bool force)
{
	if (user_data) {
		user_data->CS.Lock();
		pjsua_recorder_id *recorder_id = &user_data->recorder_id;
		if (*recorder_id != PJSUA_INVALID_ID) {
			if (pjsua_var.state == PJSUA_STATE_RUNNING) {
				bool block = false;
				pjsua_call_id call_ids[PJSUA_MAX_CALLS];
				unsigned count = PJSUA_MAX_CALLS;
				if (pjsua_enum_calls(call_ids, &count) == PJ_SUCCESS) {
					for (unsigned i = 0; i < count; ++i) {
						if (user_data->call_id == call_ids[i]) {
							continue;
						}
						call_user_data *user_data_curr = (call_user_data *)pjsua_call_get_user_data(call_ids[i]);
						if (user_data_curr) {
							user_data_curr->CS.Lock();
							if (user_data_curr->recorder_id == *recorder_id) {
								if (force) {
									pjsua_conf_port_id rec_conf_port_id = pjsua_recorder_get_conf_port(user_data_curr->recorder_id);
									pjsua_call_info call_info_curr;
									pjsua_call_get_info(call_ids[i], &call_info_curr);
									if (call_info_curr.conf_slot != PJSUA_INVALID_ID) {
										pjsua_conf_disconnect(call_info_curr.conf_slot, rec_conf_port_id);
									}
									user_data_curr->recorder_id = PJSUA_INVALID_ID;
								}
								else {
									block = true;
									user_data_curr->CS.Unlock();
									break;
								}
							}
							user_data_curr->CS.Unlock();
						}
					}
				}
				pjsua_conf_port_id rec_conf_port_id = pjsua_recorder_get_conf_port(*recorder_id);
				pjsua_call_info call_info;
				pjsua_call_get_info(user_data->call_id, &call_info);
				if (call_info.conf_slot != PJSUA_INVALID_ID) {
					pjsua_conf_disconnect(call_info.conf_slot, rec_conf_port_id);
				}
				if (!block) {
					pjsua_recorder_destroy(*recorder_id);
				}
			}
			*recorder_id = PJSUA_INVALID_ID;
		}
		user_data->CS.Unlock();
	}
}

CString msip_url_mask(CString url)
{
	CTime t = CTime::GetCurrentTime();
	time_t time = t.GetTime();
	CString str;
	str.Format(_T("%d"), time);
	url.Replace(_T("{time}"), str);
	url.Replace(_T("{version}"), _T(_GLOBAL_VERSION));
	if (accountSettings.accountId) {
		url.Replace(_T("{label}"), CString(urlencode(Utf8EncodeUcs2(accountSettings.account.label))));
		url.Replace(_T("{server}"), MSIP::RemovePort(get_account_server()));
		url.Replace(_T("{domain}"), CString(urlencode(Utf8EncodeUcs2(accountSettings.account.domain))));
		url.Replace(_T("{username}"), CString(urlencode(Utf8EncodeUcs2(accountSettings.account.username))));
		url.Replace(_T("{password}"), CString(urlencode(Utf8EncodeUcs2(accountSettings.account.password))));
		url.Replace(_T("{md5_password}"), CString(msip_md5sum(&accountSettings.account.password)));
	}
	else {
		url.Replace(_T("{label}"), _T(""));
		url.Replace(_T("{server}"), _T("localhost"));
		url.Replace(_T("{domain}"), _T(""));
		url.Replace(_T("{username}"), _T(""));
		url.Replace(_T("{password}"), _T(""));
		url.Replace(_T("{md5_password}"), _T(""));
	}
	return url;
}

void msip_startup_set(bool enable)
{
	CRegKey regKey;
	CString rab;
	rab = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
	if (regKey.Create(HKEY_CURRENT_USER, rab) == ERROR_SUCCESS) {
		if (enable) {
			CString str;
			str.Format(_T("\"%s\" /minimized"), accountSettings.exeFile);
			regKey.SetStringValue(_T(_GLOBAL_NAME_NICE), str);
		}
		else {
			regKey.DeleteValue(_T(_GLOBAL_NAME_NICE));
		}
		regKey.Close();
	}
}
