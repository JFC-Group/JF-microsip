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

#define THIS_FILENAME "mainDlg.cpp"

#include "mainDlg.h"
#include "microsip.h"

#include "Mmsystem.h"
#include "settings.h"
#include "global.h"
#include "ModelessMessageBox.h"
#include "utf.h"
#include "json.h"
#include "Markup.h"
#include "langpack.h"
#include "jumplist.h"
#include "atlenc.h"
#include "Hid.h"
#include "CMask.h"

#include <winuser.h>
#include <windows.h>
#include <io.h>
#include <afxmt.h>
#include <afxinet.h>
#include <ws2tcpip.h>
#include <Dbt.h>
#include <Strsafe.h>
#include <locale.h> 
#include <Wtsapi32.h>
#include "atlrx.h"

#include "afxvisualmanager.h"
#include "afxvisualmanagerwindows.h"

#include "iphlpapi.h"
#include "wininet.h"
#pragma comment(lib, "iphlpapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CmainDlg *mainDlg;

static UINT WM_SHELLHOOKMESSAGE;
static UINT WM_TASKBARRESTARTMESSAGE;

static bool updateCheckerShow;

static UINT BASED_CODE indicators[] =
{
	IDS_STATUSBAR,
	IDS_STATUSBAR2,
};

static bool ipChangeBusy;
static bool ipChangeDelayed;

CCriticalSection gethostbyaddrThreadCS;
static CString gethostbyaddrThreadResult;
static DWORD WINAPI gethostbyaddrThread(LPVOID lpParam)
{
	CString *addr = (CString *)lpParam;
	CString res = *addr;
	delete addr;
	struct hostent *he = NULL;
	struct in_addr inaddr;
	inaddr.S_un.S_addr = inet_addr(CStringA(res));
	if (inaddr.S_un.S_addr != INADDR_NONE && inaddr.S_un.S_addr != INADDR_ANY) {
		he = gethostbyaddr((char *)&inaddr, 4, AF_INET);
		if (he) {
			res = he->h_name;
		}
	}
	gethostbyaddrThreadCS.Lock();
	gethostbyaddrThreadResult = res;
	gethostbyaddrThreadCS.Unlock();
	return 0;
}

static void on_reg_started2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
	if (info->renew) {
		PostMessage(mainDlg->m_hWnd, UM_UPDATEWINDOWTEXT, 1, 0);
	}
}

static void on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
	if (!IsWindow(mainDlg->m_hWnd)) {
		return;
	}
	CString *str = NULL;
	if (info->cbparam->code >= 400 && info->cbparam->rdata) {
		pjsip_generic_string_hdr *hsr;
		const pj_str_t headerError = { "P-Registrar-Error",17 };
		hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(info->cbparam->rdata->msg_info.msg, &headerError, NULL);
		if (hsr) {
			str = new CString();
			str->SetString(MSIP::PjToStr(&hsr->hvalue, true));
		}
	}
	PostMessage(mainDlg->m_hWnd, UM_ON_REG_STATE2, (WPARAM)info->cbparam->code, (LPARAM)str);
}

LRESULT CmainDlg::onRegState2(WPARAM wParam, LPARAM lParam)
{
	int code = wParam;
	CString headerError;
	if (lParam) {
		CString *str = (CString *)lParam;
		headerError = *str;
		delete str;
	}

	if (code == 200) {
		Subscribe();
		if (accountSettings.usersDirectory.Find(_T("%s")) != -1 || accountSettings.usersDirectory.Find(_T("{")) != -1) {
			UsersDirectoryLoad();
		}
	}
	else {
	}

	UpdateWindowText(headerError, IDI_DEFAULT, true);

	return 0;
}

/* Callback from timer when the maximum call duration has been
 * exceeded.
 */
static void call_timeout_callback(pj_timer_heap_t *timer_heap,
	struct pj_timer_entry *entry)
{
	pjsua_call_id call_id = entry->id;
	pjsua_msg_data msg_data_;
	pjsip_generic_string_hdr warn;
	pj_str_t hname = pj_str("Warning");
	pj_str_t hvalue = pj_str("399 localhost \"Call duration exceeded\"");

	PJ_UNUSED_ARG(timer_heap);

	if (call_id == PJSUA_INVALID_ID) {
		PJ_LOG(1, (THIS_FILENAME, "Invalid call ID in timer callback"));
		return;
	}

	/* Add warning header */
	pjsua_msg_data_init(&msg_data_);
	pjsip_generic_string_hdr_init2(&warn, &hname, &hvalue);
	pj_list_push_back(&msg_data_.hdr_list, &warn);

	/* Call duration has been exceeded; disconnect the call */
	PJ_LOG(3, (THIS_FILENAME, "Duration (%d seconds) has been exceeded "
		"for call %d, disconnecting the call",
		accountSettings.autoHangUpTime, call_id));
	entry->id = PJSUA_INVALID_ID;
	pjsua_call_hangup(call_id, 200, NULL, &msg_data_);
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	if (!IsWindow(mainDlg->m_hWnd)) {
		return;
	}
	pjsua_call_info *call_info = new pjsua_call_info();
	if (pjsua_call_get_info(call_id, call_info) != PJ_SUCCESS || call_info->state == PJSIP_INV_STATE_NULL) {
		return;
	}

	if (call_info->state == PJSIP_INV_STATE_DISCONNECTED && call_info->last_status == 481) {
		return;
	}
	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	// reset user_data after call transfer
	if (user_data) {
		user_data->CS.Lock();
		bool callIdMissmatch = user_data->call_id != PJSUA_INVALID_ID && user_data->call_id != call_info->id;
		bool hidden = user_data->hidden;
		user_data->CS.Unlock();
		if (callIdMissmatch) {
			user_data = new call_user_data(call_info->id);
			pjsua_call_set_user_data(call_info->id, user_data);
		}
		else {
			if (hidden) {
				if (call_info->state == PJSIP_INV_STATE_DISCONNECTED) {
					delete user_data;
				}
				return;
			}
		}
	}
	if (!user_data) {
		user_data = new call_user_data(call_info->id);
		pjsua_call_set_user_data(call_info->id, user_data);
	}

	user_data->CS.Lock();

	switch (call_info->state) {
	case PJSIP_INV_STATE_CALLING:
		msip_call_unhold(call_info);
		break;
	case PJSIP_INV_STATE_CONNECTING:
		msip_call_unhold(call_info);
		break;
	case PJSIP_INV_STATE_CONFIRMED:
		if (accountSettings.autoRecording) {
			msip_call_recording_start(user_data, call_info);
		}
		if (accountSettings.autoHangUpTime > 0) {
			/* Schedule timer to hangup call after the specified duration */
			pj_time_val delay;
			user_data->auto_hangup_timer.id = call_info->id;
			user_data->auto_hangup_timer.cb = &call_timeout_callback;
			delay.sec = accountSettings.autoHangUpTime;
			delay.msec = 0;
			pjsua_schedule_timer(&user_data->auto_hangup_timer, &delay);
		}
		break;
	case PJSIP_INV_STATE_DISCONNECTED:
		pjsua_call_set_user_data(call_info->id, NULL);
		break;
	}

	user_data->CS.Unlock();

	PostMessage(mainDlg->m_hWnd, UM_ON_CALL_STATE, (WPARAM)call_info, (LPARAM)user_data);
}

LRESULT CmainDlg::onCallState(WPARAM wParam, LPARAM lParam)
{
	pjsua_call_info *call_info = (pjsua_call_info *)wParam;
	call_user_data *user_data = (call_user_data *)lParam;

	CString number = MSIP::PjToStr(&call_info->remote_info, TRUE);
	SIPURI sipuri;
	MSIP::ParseSIPURI(number, &sipuri);
	
	user_data->CS.Lock();

	CString *str = new CString();
	CString adder;

	if (call_info->state != PJSIP_INV_STATE_DISCONNECTED && call_info->state != PJSIP_INV_STATE_CONNECTING && call_info->remote_contact.slen > 0) {
		SIPURI contactURI;
		MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->remote_contact, TRUE), &contactURI);
		CString contactDomain = MSIP::RemovePort(contactURI.domain);
		struct hostent *he = NULL;
		if (MSIP::IsIP(contactDomain)) {
			HANDLE hThread;
			CString *addr = new CString(contactDomain);
			if (addr) {
				hThread = CreateThread(NULL, 0, gethostbyaddrThread, addr, 0, NULL);
				if (WaitForSingleObject(hThread, 500) == 0) {
					gethostbyaddrThreadCS.Lock();
					contactDomain = gethostbyaddrThreadResult;
					gethostbyaddrThreadCS.Unlock();
				}
			}
		}
		adder.AppendFormat(_T("%s, "), contactDomain);
	}

	if (call_info->state == PJSIP_INV_STATE_CONFIRMED
		|| call_info->state == PJSIP_INV_STATE_CONNECTING) {
		if (autoAnswerTimerCallId != PJSUA_INVALID_ID) {
			KillTimer(IDT_TIMER_AUTOANSWER);
			autoAnswerTimerCallId = PJSUA_INVALID_ID;
		}
		if (forwardingTimerCallId != PJSUA_INVALID_ID) {
			KillTimer(IDT_TIMER_FORWARDING);
			forwardingTimerCallId = PJSUA_INVALID_ID;
		}
	}

	unsigned cnt = 0;
	unsigned cnt_srtp = 0;

	switch (call_info->state) {
	case PJSIP_INV_STATE_CALLING:
		*str = Translate(_T("Calling"));
		str->AppendFormat(_T(" %s "), !sipuri.user.IsEmpty() ? sipuri.user : sipuri.domain);
		str->Append(_T("..."));
		break;
	case PJSIP_INV_STATE_INCOMING:
		str->SetString(Translate(_T("Incoming Call")));
		break;
	case PJSIP_INV_STATE_EARLY:
		str->SetString(Translate(MSIP::PjToStr(&call_info->last_status_text).GetBuffer()));
		break;
	case PJSIP_INV_STATE_CONNECTING:
		str->Format(_T("%s..."), Translate(_T("Connecting")));
		break;
	case PJSIP_INV_STATE_CONFIRMED:
		str->SetString(Translate(_T("Connected")));
		for (unsigned i = 0; i < call_info->media_cnt; i++) {
			if (call_info->media[i].dir != PJMEDIA_DIR_NONE &&
				(call_info->media[i].type == PJMEDIA_TYPE_AUDIO || call_info->media[i].type == PJMEDIA_TYPE_VIDEO)) {
				cnt++;
				pjsua_call_info call_info_stub;
				if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_call_get_info(call_info->id, &call_info_stub) == PJ_SUCCESS) {
					pjsua_stream_info psi;
					if (pjsua_call_get_stream_info(call_info->id, call_info->media[i].index, &psi) == PJ_SUCCESS) {
						if (call_info->media[i].type == PJMEDIA_TYPE_AUDIO) {
							adder.AppendFormat(_T("%s@%dkHz %dkbit/s%s, "),
								MSIP::PjToStr(&psi.info.aud.fmt.encoding_name), psi.info.aud.fmt.clock_rate / 1000,
								psi.info.aud.param->info.avg_bps / 1000,
								psi.info.aud.fmt.channel_cnt == 2 ? _T(" Stereo") : _T("")
							);
						}
						else {
							adder.AppendFormat(_T("%s %dkbit/s, "),
								MSIP::PjToStr(&psi.info.vid.codec_info.encoding_name),
								psi.info.vid.codec_param->enc_fmt.det.vid.max_bps / 1000
							);
						}
					}
					
					pjmedia_transport_info t;
					if (pjsua_call_get_med_transport_info(call_info->id, call_info->media[i].index, &t) == PJ_SUCCESS) {
						for (unsigned j = 0; j < t.specific_info_cnt; j++) {
							if (t.spc_info[j].buffer[0]) {
								switch (t.spc_info[j].type) {
								case PJMEDIA_TRANSPORT_TYPE_SRTP:
									adder.Append(_T("SRTP, "));
									cnt_srtp++;
									break;
								case PJMEDIA_TRANSPORT_TYPE_ICE:
									adder.Append(_T("ICE, "));
									break;
								}
							}
						}
					}
				}
			}
		}
		if (cnt_srtp && cnt == cnt_srtp) {
			user_data->srtp = MSIP_SRTP;
		}
		else {
			user_data->srtp = MSIP_SRTP_DISABLED;
		}
		break;
	}

	if (!str->IsEmpty() && !adder.IsEmpty()) {
		str->AppendFormat(_T(" (%s)"), adder.Left(adder.GetLength() - 2));
	}

	if (call_info->state == PJSIP_INV_STATE_CALLING) {
		//--
		if (!accountSettings.cmdOutgoingCall.IsEmpty()) {
			CString params = sipuri.user;
			MSIP::RunCmd(URLMask(accountSettings.cmdOutgoingCall, &sipuri, call_info->acc_id, user_data), params);
		}
		//--
	}

	if (call_info->state == PJSIP_INV_STATE_CONFIRMED) {
		PostMessage(WM_TIMER, IDT_TIMER_CALL, NULL);
		SetTimer(IDT_TIMER_CALL, 1000, NULL);
		if (call_info->role == PJSIP_ROLE_UAS) {
			//--
			if (!accountSettings.cmdCallAnswer.IsEmpty()
				) {
				CString params = sipuri.user;
				MSIP::RunCmd(accountSettings.cmdCallAnswer, params);
			}
			if (call_info->rem_vid_cnt && !accountSettings.cmdCallAnswerVideo.IsEmpty()) {
				CString params = sipuri.user;
				MSIP::RunCmd(accountSettings.cmdCallAnswerVideo, params);
		}
			//--
		}
		//--
		if (!accountSettings.cmdCallStart.IsEmpty()) {
			CString params = sipuri.user;
			MSIP::RunCmd(accountSettings.cmdCallStart, params);
		}
		//--
		if (!user_data->commands.IsEmpty()) {
			SetTimer((UINT_PTR)call_info->id, 1000, (TIMERPROC)DTMFQueueTimerHandler);
		}
	}

	if (!accountSettings.singleMode) {
		if (call_info->state != PJSIP_INV_STATE_CONFIRMED) {
			if (call_info->state != PJSIP_INV_STATE_DISCONNECTED) {
				UpdateWindowText(*str, call_info->role == PJSIP_ROLE_UAS ? IDI_CALL_IN : IDI_CALL_OUT);
			}
		}
	}

	if (call_info->role == PJSIP_ROLE_UAC) {
		if (call_info->last_status == 180 && !call_info->media_cnt) {
			if (toneCalls.IsEmpty()) {
				PostMessage(WM_TIMER, IDT_TIMER_TONE, NULL);
				SetTimer(IDT_TIMER_TONE, 4500, NULL);
				toneCalls.AddTail(call_info->id);
			}
			else if (toneCalls.Find(call_info->id) == NULL) {
				toneCalls.AddTail(call_info->id);
			}
		}
		else {
			POSITION position = toneCalls.Find(call_info->id);
			if (position != NULL) {
				toneCalls.RemoveAt(position);
				if (toneCalls.IsEmpty()) {
					KillTimer(IDT_TIMER_TONE);
					PostMessage(UM_ON_PLAYER_STOP, 0, 0);
				}
			}
		}
	}

	bool doNotShowMessagesWindow =
		call_info->state == PJSIP_INV_STATE_INCOMING ||
		call_info->state == PJSIP_INV_STATE_EARLY ||
		call_info->state == PJSIP_INV_STATE_DISCONNECTED ||
		accountSettings.singleMode ||
		(MACRO_SILENT && !mainDlg->IsWindowVisible());

	if (user_data->autoAnswer) {
		if (!accountSettings.bringToFrontOnIncoming) {
			doNotShowMessagesWindow = true;
		}
	}
	MessagesContact* messagesContact = messagesDlg->AddTab(number,
		(!accountSettings.singleMode &&
		(call_info->state == PJSIP_INV_STATE_CONFIRMED
			|| call_info->state == PJSIP_INV_STATE_CONNECTING)
			)
		||
		(accountSettings.singleMode
			&&
			(
			(call_info->role == PJSIP_ROLE_UAC && call_info->state != PJSIP_INV_STATE_DISCONNECTED)
				||
				(call_info->role == PJSIP_ROLE_UAS &&
				(call_info->state == PJSIP_INV_STATE_CONFIRMED
					|| call_info->state == PJSIP_INV_STATE_CONNECTING)
					)
				))
		? TRUE : FALSE,
		call_info, user_data, doNotShowMessagesWindow, call_info->state == PJSIP_INV_STATE_DISCONNECTED
	);
	
	if (call_info->state == PJSIP_INV_STATE_CONFIRMED) {
		if (!accountSettings.singleMode && accountSettings.AC) {
			messagesDlg->OnMergeAll();
		}
	}

	if (call_info->state == PJSIP_INV_STATE_DISCONNECTED) {
		if (call_info->role == PJSIP_ROLE_UAS && call_info->connect_duration.sec == 0 && call_info->connect_duration.msec == 0 && call_info->last_status != 486) {
			//-- missed call
			missed = true;
		}
	}

	if (messagesContact) {
		if (call_info->state == PJSIP_INV_STATE_DISCONNECTED) {
			messagesContact->mediaStatus = PJSUA_CALL_MEDIA_ERROR;
		}
		else {
			CString name = messagesContact->name;
			CString number = messagesContact->number + messagesContact->numberParameters + messagesContact->commands;
			if (call_info->role == PJSIP_ROLE_UAS) {
				pageCalls->Add(call_info->call_id, number, name, MSIP_CALL_IN, user_data);
			}
			else {
				pageCalls->Add(call_info->call_id, number, name, MSIP_CALL_OUT, user_data);
			}
		}
	}
	if (accountSettings.singleMode) {
		if (call_info->state != PJSIP_INV_STATE_DISCONNECTED) {
			if (call_info->state != PJSIP_INV_STATE_CONFIRMED) {
				UpdateWindowText(*str, call_info->role == PJSIP_ROLE_UAS ? IDI_CALL_IN : IDI_CALL_OUT);
			}
			int tabN = 0;
			GotoTab(tabN);
			messagesDlg->OnChangeTab(call_info, user_data);
		}
	}

	if (call_info->state == PJSIP_INV_STATE_DISCONNECTED) {
		messagesDlg->OnEndCall(call_info, user_data);
	}
	else {
		if (messagesContact && !str->IsEmpty()) {
			messagesDlg->AddMessage(messagesContact, *str, MSIP_MESSAGE_TYPE_SYSTEM,
				call_info->state == PJSIP_INV_STATE_INCOMING || call_info->state == PJSIP_INV_STATE_EARLY
			);
		}
	}

	bool hasCalls = messagesDlg->GetCallsCount();

	if (call_info->role == PJSIP_ROLE_UAS) {
		if (call_info->state != PJSIP_INV_STATE_INCOMING && call_info->state != PJSIP_INV_STATE_EARLY) {
			int count = ringinDlgs.GetCount();
			if (!count) {
				if (call_info->state != PJSIP_INV_STATE_DISCONNECTED || (call_info->state == PJSIP_INV_STATE_DISCONNECTED && call_info->connect_duration.sec == 0 && call_info->connect_duration.msec == 0)) {
					PlayerStop();
				}
			}
			else {
				for (int i = 0; i < count; i++) {
					RinginDlg *ringinDlg = ringinDlgs.GetAt(i);
					if (call_info->id == ringinDlg->call_id) {
						if (count == 1) {
							PlayerStop();
						}
						ringinDlgs.RemoveAt(i);
						ringinDlg->DestroyWindow();
						break;
					}
				}
			}
		}
	}

	if (call_info->state != PJSIP_INV_STATE_INCOMING &&
		call_info->state != PJSIP_INV_STATE_EARLY
		) {
		if (call_info->state != PJSIP_INV_STATE_DISCONNECTED) {
			if (messagesContact) {
				CString name = messagesContact->name;
				pageDialer->SetName(name);
			}
		}
	}

	user_data->CS.Unlock();

	// --delete user data
	if (call_info->state == PJSIP_INV_STATE_DISCONNECTED) {
		if (user_data) {
			delete user_data;
		}
	}
	// --
	delete call_info;
	delete str;

	if (pageDialer->IsChild(&pageDialer->m_ButtonRec)) {
		pageDialer->m_ButtonRec.EnableWindow(hasCalls);
	}
	if (accountSettings.headsetSupport) {
		Hid::SetOffhookRing(hasCalls, ringinDlgs.GetCount());
	}
	if (hasCalls) {
#ifdef _GLOBAL_VIDEO
		SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED | (mainDlg->previewWin ? ES_DISPLAY_REQUIRED : 0));
#else
		SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#endif
	}
	else {
		SetThreadExecutionState(ES_CONTINUOUS);
	}
	return 0;
}

static void on_call_media_state(pjsua_call_id call_id)
{
	pjsua_call_info *call_info = new pjsua_call_info();
	if (pjsua_call_get_info(call_id, call_info) != PJ_SUCCESS || call_info->state == PJSIP_INV_STATE_NULL) {
		return;
	}

	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	if (!user_data) {
		user_data = new call_user_data(call_info->id);
		pjsua_call_set_user_data(call_info->id, user_data);
	}

	if (call_info->media_status == PJSUA_CALL_MEDIA_ACTIVE
		|| call_info->media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD
		) {
		msip_conference_join(call_info);
		pjsua_conf_connect(call_info->conf_slot, 0);
		pjsua_conf_connect(0, call_info->conf_slot);
		//--
		user_data->CS.Lock();
		user_data->holdFrom = -1;
		if (user_data->recorder_id != PJSUA_INVALID_ID) {
			pjsua_conf_port_id rec_conf_port_id = pjsua_recorder_get_conf_port(user_data->recorder_id);
			pjsua_conf_connect(call_info->conf_slot, rec_conf_port_id);
		}
		user_data->CS.Unlock();

		//--
		::SetTimer(mainDlg->pageDialer->m_hWnd, IDT_TIMER_VU_METER, 100, NULL);
		//--
	}
	else {
		msip_conference_leave(call_info, user_data, true);
		pjsua_conf_disconnect(call_info->conf_slot, 0);
		pjsua_conf_disconnect(0, call_info->conf_slot);
		call_deinit_tonegen(call_info->id);
		//--
		user_data->CS.Lock();
		user_data->holdFrom = msip_get_duration(&call_info->connect_duration);
		user_data->CS.Unlock();
		//--
	}

	PostMessage(mainDlg->m_hWnd, UM_ON_CALL_MEDIA_STATE, (WPARAM)call_info, (LPARAM)user_data);
}

LRESULT CmainDlg::onCallMediaState(WPARAM wParam, LPARAM lParam)
{
	pjsua_call_info *call_info = (pjsua_call_info *)wParam;
	call_user_data *user_data = (call_user_data *)lParam;

	messagesDlg->UpdateHoldButton(call_info);

	CString message;
	CString number = MSIP::PjToStr(&call_info->remote_info, TRUE);

	MessagesContact* messagesContact = messagesDlg->AddTab(number, FALSE, call_info, user_data, TRUE, TRUE);

	if (messagesContact) {
		if (call_info->media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD) {
			message = _T("Call on Remote Hold");
		}
		if (call_info->media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD) {
			message = _T("Call on Local Hold");
		}
		if (call_info->media_status == PJSUA_CALL_MEDIA_NONE) {
			message = _T("Call on Hold");
		}
		if (messagesContact->mediaStatus != PJSUA_CALL_MEDIA_ERROR && messagesContact->mediaStatus != call_info->media_status && call_info->media_status == PJSUA_CALL_MEDIA_ACTIVE) {
			message = _T("Call is Active");
		}
		if (!message.IsEmpty()) {
			messagesDlg->AddMessage(messagesContact, Translate(message.GetBuffer()), MSIP_MESSAGE_TYPE_SYSTEM, TRUE);
		}
		messagesContact->mediaStatus = call_info->media_status;
		pageDialer->SetName();
	}
	if (call_info->media_status == PJSUA_CALL_MEDIA_ACTIVE
		|| call_info->media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD
		) {
		onRefreshLevels(0, 0);
	}

	delete call_info;

	return 0;
}

static void on_call_media_event(pjsua_call_id call_id,
	unsigned med_idx,
	pjmedia_event *event)
{
	char event_name[5];

	PJ_LOG(5, (THIS_FILENAME, "Event %s",
		pjmedia_fourcc_name(event->type, event_name)));

	//#if PJSUA_HAS_VIDEO
		//if (event->type == PJMEDIA_EVENT_FMT_CHANGED) {
		//	pjsua_call_info ci;
		//	pjsua_call_get_info(call_id, &ci);
		//	if ((ci.media[med_idx].type == PJMEDIA_TYPE_VIDEO) &&
		//		(ci.media[med_idx].dir & PJMEDIA_DIR_DECODING)) {
		//		pjsua_vid_win_id wid;
		//		pjmedia_rect_size size;
		//		pjsua_vid_win_info win_info;

		//		wid = ci.media[med_idx].stream.vid.win_in;
		//		pjsua_vid_win_get_info(wid, &win_info);

		//		size = event->data.fmt_changed.new_fmt.det.vid.size;
		//		if (size.w != win_info.size.w || size.h != win_info.size.h) {
		//			pjsua_vid_win_set_size(wid, &size);
		//			/* Re-arrange video windows */
		//			arrange_window(PJSUA_INVALID_ID);
		//		}
		//	}
		//}
	//#else
	//	PJ_UNUSED_ARG(call_id);
	//	PJ_UNUSED_ARG(med_idx);
	//	PJ_UNUSED_ARG(event);
	//#endif
}

static void on_incoming_call(pjsua_acc_id acc, pjsua_call_id call_id,
	pjsip_rx_data *rdata)
{
	pjsua_call_info *call_info = new pjsua_call_info();
	if (pjsua_call_get_info(call_id, call_info) != PJ_SUCCESS) {
		return;
	}

	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	if (!user_data) {
		user_data = new call_user_data(call_info->id);
		pjsua_call_set_user_data(call_info->id, user_data);
	}

	user_data->CS.Lock();

	SIPURI sipuri;
	MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->remote_info, TRUE), &sipuri);
	user_data->name = sipuri.name;
	if (accountSettings.forceCodec) {
		pjsua_call *call;
		pjsip_dialog *dlg;
		pj_status_t status;
		status = acquire_call("on_incoming_call()", call_id, &call, &dlg);
		if (status == PJ_SUCCESS) {
			pjmedia_sdp_neg_set_prefer_remote_codec_order(call->inv->neg, PJ_FALSE);
			pjsip_dlg_dec_lock(dlg);
		}
	}
	//--
	if (!accountSettings.cmdIncomingCall.IsEmpty()) {
		CString params = sipuri.user;
		MSIP::RunCmd(accountSettings.cmdIncomingCall, params);
	}
	//--
	//--
	bool busy = false;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned calls_count = PJSUA_MAX_CALLS;
	unsigned calls_count_cmp = 0;
	if (pjsua_enum_calls(call_ids, &calls_count) == PJ_SUCCESS) {
		for (unsigned i = 0; i < calls_count; ++i) {
			pjsua_call_info call_info_curr;
			if (pjsua_call_get_info(call_ids[i], &call_info_curr) == PJ_SUCCESS) {
				SIPURI sipuri_curr;
				MSIP::ParseSIPURI(MSIP::PjToStr(&call_info_curr.remote_info, TRUE), &sipuri_curr);
				if (call_info_curr.id != call_info->id &&
					sipuri.user + _T("@") + sipuri.domain == sipuri_curr.user + _T("@") + sipuri_curr.domain
					) {
					busy = true;
					break;
				}
				if (call_info_curr.state != PJSIP_INV_STATE_DISCONNECTED) {
					calls_count_cmp++;
				}
			}
		}
	}
	if (busy) {
		// 486 Busy Here
		msip_call_busy(call_info->id, _T("Call already exists"));
		user_data->hidden = true;
	} 
	else if ((!accountSettings.callWaiting && calls_count_cmp > 1) || (accountSettings.maxConcurrentCalls > 0 && calls_count_cmp > accountSettings.maxConcurrentCalls)) {
		// 486 Busy Here
		msip_call_busy(call_info->id, _T("Active calls limit"));
		user_data->hidden = true;
	}
	else if (!mainDlg->callIdIncomingIgnore.IsEmpty() && mainDlg->callIdIncomingIgnore == MSIP::PjToStr(&call_info->call_id)) {
		pjsua_call_answer(call_info->id, 487, NULL, NULL);
		user_data->hidden = true;
	}
	else {
		bool reject = false;
		CString reason;
		if (accountSettings.denyIncoming == _T("all")) {
			reject = true;
		}
		else if (accountSettings.denyIncoming == _T("button")) {
			reject = accountSettings.DND;
			reason = _T("Do Not Disturb");
		}
		else if (accountSettings.denyIncoming == _T("user")) {
			SIPURI sipuri_curr;
			MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->local_info, TRUE), &sipuri_curr);
			if (sipuri_curr.user != get_account_username()) {
				reject = true;
			}
		}
		else if (accountSettings.denyIncoming == _T("domain")) {
			SIPURI sipuri_curr;
			MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->local_info, TRUE), &sipuri_curr);
			if (accountSettings.accountId) {
				if (sipuri_curr.domain != get_account_domain()) {
					reject = true;
				}
			}
		}
		else if (accountSettings.denyIncoming == _T("remotedomain")) {
			if (accountSettings.accountId) {
				if (sipuri.domain != get_account_domain()) {
					reject = true;
				}
			}
		}
		else if (accountSettings.denyIncoming == _T("userdomain")) {
			SIPURI sipuri_curr;
			MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->local_info, TRUE), &sipuri_curr);
			if (sipuri_curr.user != get_account_username()) {
				reject = true;
			}
			else {
				CString domain = get_account_domain();
				if (domain != _T("") && sipuri_curr.domain != domain) {
					reject = true;
				}
			}
		}
		if (reject) {
			if (reason.IsEmpty()) {
				reason = _T("Denied");
			}
			msip_call_busy(call_info->id, reason);
			user_data->hidden = true;
		}
		else {
			pjsip_generic_string_hdr *hsr;
			// -- diversion
			const pj_str_t headerDiversion = { "Diversion",9 };
			hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerDiversion, NULL);
			if (hsr) {
				CString str = MSIP::PjToStr(&hsr->hvalue, true);
				SIPURI sipuriDiversion;
				MSIP::ParseSIPURI(str, &sipuriDiversion);
				user_data->diversion = !sipuriDiversion.user.IsEmpty() ? sipuriDiversion.user : sipuriDiversion.domain;
			}
			// -- end diversion
			// -- caller id
			const pj_str_t headerCallerID = { "P-Asserted-Identity",19 };
			hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerCallerID, NULL);
			if (!hsr) {
				const pj_str_t headerCallerID = { "Remote-Party-Id",15 };
				hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerCallerID, NULL);
			}
			if (hsr) {
				user_data->callerID = MSIP::PjToStr(&hsr->hvalue, true);
				int pos = user_data->callerID.Find(';');
				if (pos != -1) {
					user_data->callerID = user_data->callerID.Left(pos);
				}
			}
			// -- end caller id
			// -- user agent
			const pj_str_t headerUserAgent = { "User-Agent",10 };
			hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerUserAgent, NULL);
			if (hsr) {
				user_data->userAgent = MSIP::PjToStr(&hsr->hvalue, true);
			}
			// -- end user agent
			bool autoAnswer = false;
			int autoAnswerDelay = accountSettings.autoAnswerDelay;
			if (accountSettings.autoAnswer == _T("all")) {
				autoAnswer = true;
			}
			else if (accountSettings.autoAnswer == _T("button")) {
				autoAnswer = accountSettings.AA;
			}
			else if (accountSettings.autoAnswer == _T("header")) {
				//--
				pjsip_generic_string_hdr *hsr = NULL;
				const pj_str_t header = pj_str("X-AUTOANSWER");
				hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &header, NULL);
				if (hsr) {
					CString autoAnswerValue = MSIP::PjToStr(&hsr->hvalue, TRUE);
					autoAnswerValue.MakeLower();
					if (autoAnswerValue == _T("true") || autoAnswerValue == _T("1")) {
						autoAnswer = true;
					}
				}
				//--
				if (!autoAnswer) {
					pjsip_generic_string_hdr *hsr = NULL;
					const pj_str_t header = pj_str("Call-Info");
					hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &header, NULL);
					if (hsr) {
						CString callInfoValue = MSIP::PjToStr(&hsr->hvalue, TRUE);
						callInfoValue.MakeLower();
						if (callInfoValue.Find(_T("auto answer")) != -1) {
							autoAnswer = true;
						}
						else {
							CAtlRegExp<> regex;
							REParseError parseStatus = regex.Parse(_T("answer-after={[0-9]+}"), true);
							if (parseStatus == REPARSE_ERROR_OK) {
								CAtlREMatchContext<> mc;
								if (regex.Match(callInfoValue, &mc) && mc.m_uNumGroups == 1) {
									const CAtlREMatchContext<>::RECHAR* szStart = 0;
									const CAtlREMatchContext<>::RECHAR* szEnd = 0;
									mc.GetMatch(0, &szStart, &szEnd);
									ptrdiff_t nLength = szEnd - szStart;
									CStringA text(szStart, nLength);
									autoAnswerDelay = atoi(text);
									autoAnswer = true;
								}
							}
						}
					}
				}
			}

			if (autoAnswer && !accountSettings.autoAnswerNumber.IsEmpty()) {
				bool found = false;
				int pos = 0;
				CString resToken = accountSettings.autoAnswerNumber.Tokenize(_T(";|"), pos);
				while (!resToken.IsEmpty()) {
					resToken.Trim();
					if (!resToken.IsEmpty()) {
						CMask mask;
						if (mask.WildMatch(resToken,sipuri.user,_T(""))) {
							found = true;
							break;
						}
					}
					resToken = accountSettings.autoAnswerNumber.Tokenize(_T(";|"), pos);
				}
				if (!found) {
					autoAnswer = false;
				}
			}
			bool forwarding = false;
			if (!accountSettings.forwardingNumber.IsEmpty()) {
				if (accountSettings.forwarding == _T("all") || 
					(accountSettings.forwarding == _T("button") && accountSettings.FWD)
					) {
					forwarding = true;
				}
			}
			if (forwarding) {
				if (accountSettings.forwardingDelay > 0) {
					if (autoAnswer && autoAnswerDelay > 0 && mainDlg->autoAnswerTimerCallId == PJSUA_INVALID_ID && autoAnswerDelay < accountSettings.forwardingDelay) {
						//
					} else {
						if (mainDlg->forwardingTimerCallId == PJSUA_INVALID_ID) {
							mainDlg->forwardingTimerCallId = call_info->id;
							mainDlg->SetTimer(IDT_TIMER_FORWARDING, accountSettings.forwardingDelay * 1000, NULL);
						}
					}
				}
				else {
					user_data->forwarding = true;
				}
			}
			if (autoAnswer) {
				if (autoAnswerDelay > 0) {
					if (mainDlg->autoAnswerTimerCallId == PJSUA_INVALID_ID) {
						mainDlg->autoAnswerTimerCallId = call_info->id;
						mainDlg->SetTimer(IDT_TIMER_AUTOANSWER, autoAnswerDelay * 1000, NULL);
					}
				}
				else {
					user_data->autoAnswer = true;
				}
			}
			PostMessage(mainDlg->m_hWnd, UM_ON_INCOMING_CALL, (WPARAM)call_info, (LPARAM)user_data);
		}
	}
	user_data->CS.Unlock();
}

LRESULT CmainDlg::onIncomingCall(WPARAM wParam, LPARAM lParam)
{
	pjsua_call_info *call_info = (pjsua_call_info *)wParam;
	call_user_data *user_data = (call_user_data *)lParam;

	user_data->CS.Lock();

	SIPURI sipuri;
	MSIP::ParseSIPURI(MSIP::PjToStr(&call_info->remote_info, TRUE), &sipuri);

	accountSettings.lastCallNumber = sipuri.user;
	accountSettings.lastCallHasVideo = false;

	bool autoAnswer = user_data->autoAnswer;
	user_data->autoAnswer = false;
	bool playBeep = false;

	if (user_data->forwarding && messagesDlg->CallAction(MSIP_ACTION_FORWARD, _T(""), call_info->id)) {
	} else
	if (autoAnswer && AutoAnswer(call_info->id)) {
	}
	else {
		if (!accountSettings.hidden
			) {
			PostMessage(UM_CREATE_RINGING, (WPARAM)call_info->id, NULL);
		}
		pjsua_call_answer(call_info->id, 180, NULL, NULL);
		if (messagesDlg->GetCallsCount()) {

			playBeep = true;
		}
		else {
			if (!accountSettings.ringtone.GetLength()) {
				onPlayerPlay(MSIP_SOUND_RINGTONE, 0);
			}
			else {
				onPlayerPlay(MSIP_SOUND_CUSTOM, (LPARAM)&accountSettings.ringtone);
			}
		}
		if (accountSettings.headsetSupport) {
			Hid::SetRing(true);
		}
		//--
		if (!accountSettings.cmdCallRing.IsEmpty()) {
			CString params = sipuri.user;
			MSIP::RunCmd(accountSettings.cmdCallRing, params);
		}
		//--
	}
	if (accountSettings.localDTMF && playBeep) {
		onPlayerPlay(MSIP_SOUND_RINGIN2, 0);
	}

	user_data->CS.Unlock();
	delete call_info;
	return 0;
}

static void on_nat_detect(const pj_stun_nat_detect_result *res)
{
	if (res->status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILENAME, "NAT detection failed", res->status);
	}
	else {
		if (res->nat_type == PJ_STUN_NAT_TYPE_SYMMETRIC) {
			if (IsWindow(mainDlg->m_hWnd)) {
				CString message;
//				pjsua_acc_config acc_cfg;
//				pj_pool_t *pool;
//				pool = pjsua_pool_create("acc_cfg-pjsua", 1000, 1000);
//				if (pool) {
//					pjsua_acc_id ids[PJSUA_MAX_ACC];
//					unsigned count = PJSUA_MAX_ACC;
//					if (pjsua_enum_accs(ids, &count) == PJ_SUCCESS) {
//						for (unsigned i = 0; i < count; i++) {
//							if (pjsua_acc_get_config(ids[i], pool, &acc_cfg) == PJ_SUCCESS) {
//								acc_cfg.sip_stun_use = PJSUA_STUN_USE_DISABLED;
//								acc_cfg.media_stun_use = PJSUA_STUN_USE_DISABLED;
//								if (pjsua_acc_modify(ids[i], &acc_cfg) == PJ_SUCCESS) {
//									message = _T("STUN was automatically disabled.");
//									message.Append(_T(" For more info visit MicroSIP website, help page."));
//
//								}
//							}
//						}
//					}
//					pj_pool_release(pool);
//				}
				message = _T("The softphpne may not work properly with enabled STUN and your internet connection.");
				mainDlg->BaloonPopup(Translate(_T("Symmetric NAT detected!")), Translate(message.GetBuffer()));
			}
		}
		PJ_LOG(3, (THIS_FILENAME, "NAT detected as %s", res->nat_type_name));
	}
}

void on_buddy_state(pjsua_buddy_id buddy_id)
{
	if (!IsWindow(mainDlg->m_hWnd)) {
		return;
	}
	mainDlg->PostMessage(UM_ON_BUDDY_STATE, (WPARAM)buddy_id);
}

LRESULT CmainDlg::onBuddyState(WPARAM wParam, LPARAM lParam)
{
	if (isSubscribed) {
		pjsua_buddy_id buddy_id = wParam;
		pjsua_buddy_info buddy_info;
		if (pjsua_buddy_is_valid(buddy_id) && pjsua_buddy_get_info(buddy_id, &buddy_info) == PJ_SUCCESS) {
			int image;
			bool ringing = false;
			CString info;
			switch (buddy_info.status)
			{
			case PJSUA_BUDDY_STATUS_OFFLINE:
				image = MSIP_CONTACT_ICON_OFFLINE;
				break;
			case PJSUA_BUDDY_STATUS_ONLINE:
				if (PJRPID_ACTIVITY_UNKNOWN && !buddy_info.rpid.activity) {
					image = MSIP_CONTACT_ICON_ON_THE_PHONE;
				}
				else
					if (buddy_info.rpid.activity == PJRPID_ACTIVITY_AWAY)
					{
						image = MSIP_CONTACT_ICON_AWAY;
					}
					else if (buddy_info.rpid.activity == PJRPID_ACTIVITY_BUSY) {
						image = MSIP_CONTACT_ICON_BUSY;
					}
					else {
						image = MSIP_CONTACT_ICON_ONLINE;
					}
				break;
			default:
				image = MSIP_CONTACT_ICON_UNKNOWN;
			}
			info = MSIP::PjToStr(&buddy_info.status_text);
			if (buddy_info.status == PJSUA_BUDDY_STATUS_ONLINE) {
				if (info == _T("On the phone")) {
					image = MSIP_CONTACT_ICON_ON_THE_PHONE;
				}
				else if (MSIP::PjToStr(&buddy_info.status_text).Left(4) == _T("Ring")) {
					image = MSIP_CONTACT_ICON_ON_THE_PHONE;
					ringing = true;
				}
			}
			CString* buddyNumber = (CString*)pjsua_buddy_get_user_data(buddy_id);
			//--
			pageContacts->PresenceReceived(buddyNumber, image, ringing, &info);
			pageDialer->PresenceReceived(buddyNumber, image, ringing);
		}
	}
	return 0;
}

static void on_pager2(pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, const pj_str_t *mime_type, const pj_str_t *body, pjsip_rx_data *rdata, pjsua_acc_id acc_id)
{
	if (pj_strcmp2(mime_type, "text/plain") != 0 || accountSettings.disableMessaging) {
		return;
	}
	if (IsWindow(mainDlg->m_hWnd)) {
		CString *number = new CString();
		CString *message = new CString();
		number->SetString(MSIP::PjToStr(from, TRUE));
		message->SetString(MSIP::PjToStr(body, TRUE));
		message->Trim();
		//-- fix wrong domain
		SIPURI sipuri;
		MSIP::ParseSIPURI(*number, &sipuri);
		if (accountSettings.accountId) {
			if (MSIP::IsIP(sipuri.domain)) {
				sipuri.domain = get_account_domain();
			}
			if (!sipuri.user.IsEmpty()) {
				number->Format(_T("%s@%s"), sipuri.user, sipuri.domain);
			}
			else {
				number->SetString(sipuri.domain);
			}
		}
		//--
		mainDlg->PostMessage(UM_ON_PAGER, (WPARAM)number, (LPARAM)message);
	}
}

static void on_pager_status2(pjsua_call_id call_id, const pj_str_t *to, const pj_str_t *body, void *user_data, pjsip_status_code status, const pj_str_t *reason, pjsip_tx_data *tdata, pjsip_rx_data *rdata, pjsua_acc_id acc_id)
{
	if (status != 200) {
		if (IsWindow(mainDlg->m_hWnd)) {
			CString *number = new CString();
			CString *message = new CString();
			number->SetString(MSIP::PjToStr(to, TRUE));
			message->SetString(MSIP::PjToStr(reason, TRUE));
			message->Trim();
			//-- fix wrong domain
			SIPURI sipuri;
			MSIP::ParseSIPURI(*number, &sipuri);
			if (accountSettings.accountId) {
				if (MSIP::IsIP(sipuri.domain)) {
					sipuri.domain = get_account_domain();
				}
				if (!sipuri.user.IsEmpty()) {
					number->Format(_T("%s@%s"), sipuri.user, sipuri.domain);
				}
				else {
					number->SetString(sipuri.domain);
				}
			}
			//--
			mainDlg->PostMessage(UM_ON_PAGER_STATUS, (WPARAM)number, (LPARAM)message);
		}
	}
}

static void on_call_transfer_status(pjsua_call_id call_id,
	int status_code,
	const pj_str_t *status_text,
	pj_bool_t final,
	pj_bool_t *p_cont)
{
	pjsua_call_info *call_info = new pjsua_call_info();
	if (pjsua_call_get_info(call_id, call_info) != PJ_SUCCESS || call_info->state == PJSIP_INV_STATE_NULL) {
		return;
	}

	call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_info->id);
	if (!user_data) {
		user_data = new call_user_data(call_info->id);
		pjsua_call_set_user_data(call_info->id, user_data);
	}

	CString *str = new CString();
	str->Format(_T("%s: %s"),
		Translate(_T("Call Transfer")),
		MSIP::PjToStr(status_text, TRUE)
	);
	if (final) {
		str->AppendFormat(_T(" [%s]"), Translate(_T("Final")));
	}

	if (status_code / 100 == 2) {
		*p_cont = PJ_FALSE;
	}

	call_info->last_status = (pjsip_status_code)status_code;

	call_info->call_id.ptr = (char *)user_data;
	call_info->call_id.slen = 0;

	PostMessage(mainDlg->m_hWnd, UM_ON_CALL_TRANSFER_STATUS, (WPARAM)call_info, (LPARAM)str);
}

LRESULT CmainDlg::onCallTransferStatus(WPARAM wParam, LPARAM lParam)
{
	pjsua_call_info *call_info = (pjsua_call_info *)wParam;
	call_user_data *user_data = (call_user_data *)call_info->call_id.ptr;
	CString *str = (CString *)lParam;


	MessagesContact* messagesContact = NULL;
	CString number = MSIP::PjToStr(&call_info->remote_info, TRUE);
	messagesContact = mainDlg->messagesDlg->AddTab(number, FALSE, call_info, user_data, TRUE, TRUE);
	if (messagesContact) {
		mainDlg->messagesDlg->AddMessage(messagesContact, *str);
	}
	if (call_info->last_status / 100 == 2) {
		if (messagesContact) {
			messagesDlg->AddMessage(messagesContact, Translate(_T("Call transfered successfully, disconnecting call")));
		}
		msip_call_hangup_fast(call_info->id);
	}
	delete call_info;
	delete str;
	return 0;
}

static void on_call_transfer_request2(pjsua_call_id call_id, const pj_str_t *dst, pjsip_status_code *code, pjsua_call_setting *opt)
{
	SIPURI sipuri;
	MSIP::ParseSIPURI(MSIP::PjToStr(dst, TRUE), &sipuri);
	pj_bool_t cont;
	CString number = sipuri.user;
	if (number.IsEmpty()) {
		number = sipuri.domain;
	}
	else if (!accountSettings.accountId || sipuri.domain != get_account_domain()) {
		number.Append(_T("@") + sipuri.domain);
	}
	pj_str_t status_text = MSIP::StrToPjStr(number);
	on_call_transfer_status(call_id,
		0,
		&status_text,
		PJ_FALSE,
		&cont);
	//--
	if (!code) {
		// if our function call
		return;
	}
	pjsua_call_info call_info;
	if (pjsua_call_get_info(call_id, &call_info) != PJ_SUCCESS || call_info.state != PJSIP_INV_STATE_CONFIRMED) {
		*code = PJSIP_SC_DECLINE;
	}
	if (*code != PJSIP_SC_DECLINE) {
		// deny transfer if we already have a call with same dest address
		pjsua_call_id call_ids[PJSUA_MAX_CALLS];
		unsigned calls_count = PJSUA_MAX_CALLS;
		if (pjsua_enum_calls(call_ids, &calls_count) == PJ_SUCCESS) {
			for (unsigned i = 0; i < calls_count; ++i) {
				pjsua_call_info call_info_curr;
				if (pjsua_call_get_info(call_ids[i], &call_info_curr) == PJ_SUCCESS) {
					SIPURI sipuri_curr;
					MSIP::ParseSIPURI(MSIP::PjToStr(&call_info_curr.remote_info, TRUE), &sipuri_curr);
					if (sipuri.user + _T("@") + sipuri.domain == sipuri_curr.user + _T("@") + sipuri_curr.domain
						) {
						*code = PJSIP_SC_DECLINE;
						break;
					}
				}
			}
		}
	}
}

static void on_call_replace_request2(pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text, pjsua_call_setting *opt)
{
	pjsua_call_info call_info;
	if (pjsua_call_get_info(call_id, &call_info) == PJ_SUCCESS) {
		if (!call_info.rem_vid_cnt) {
			opt->vid_cnt = 0;
		}
	}
	else {
		opt->vid_cnt = 0;
	}
}

static void on_call_replaced(pjsua_call_id old_call_id, pjsua_call_id new_call_id)
{
	pjsua_call_info call_info;
	if (pjsua_call_get_info(new_call_id, &call_info) == PJ_SUCCESS) {
		on_call_transfer_request2(old_call_id, &call_info.remote_info, NULL, NULL);
	}
}

static void on_mwi_info(pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info)
{
	bool hasMail = false;
	if (mwi_info->rdata->msg_info.ctype) {
		const pjsip_ctype_hdr *ctype = mwi_info->rdata->msg_info.ctype;
		if (pj_strcmp2(&ctype->media.type, "application") != 0 || pj_strcmp2(&ctype->media.subtype, "simple-message-summary") != 0) {
			return;
		}
	}
	if (!mwi_info->rdata->msg_info.msg->body || !mwi_info->rdata->msg_info.msg->body->len) {
		return;
	}
	pjsip_msg_body *body = mwi_info->rdata->msg_info.msg->body;
	LPARAM lParam = 0;
	pj_scanner scanner;
	pj_scan_init(&scanner, (char*)body->data, body->len, PJ_SCAN_AUTOSKIP_WS, 0);
	while (!pj_scan_is_eof(&scanner)) {
		pj_str_t key;
		pj_scan_get_until_chr(&scanner, ":", &key);
		pj_strtrim(&key);
		if (key.slen && !pj_scan_is_eof(&scanner)) {
			scanner.curptr++;
			pj_str_t value;
			pj_scan_get_until_chr(&scanner, "\r\n", &value);
			pj_strtrim(&value);
			if (pj_stricmp2(&key, "Messages-Waiting") == 0) {
				hasMail = pj_stricmp2(&value, "yes") == 0;
				break;
			}
		}
	}
	pj_scan_fini(&scanner);
	PostMessage(mainDlg->m_hWnd, UM_ON_MWI_INFO, (WPARAM)hasMail, lParam);
}

LRESULT CmainDlg::onMWIInfo(WPARAM wParam, LPARAM lParam)
{
	bool hasMail = (bool)wParam;
	pageDialer->UpdateVoicemailButton(hasMail);
	return 0;
}

static void on_ip_change_progress(pjsua_ip_change_op op, pj_status_t status, const pjsua_ip_change_op_info *info)
{
	ipChangeBusy = (op != PJSUA_IP_CHANGE_OP_COMPLETED);
	if (!ipChangeBusy && ipChangeDelayed) {
		ipChangeDelayed = false;
		PostMessage(mainDlg->m_hWnd, UM_NETWORK_CHANGE, 0, 0);
	}
}

static void on_dtmf_digit(pjsua_call_id call_id, int digit)
{
	char signal[2];
	signal[0] = digit;
	signal[1] = 0;
	call_play_digit(-1, signal);
}

static void on_call_tsx_state(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e)
{
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
		/*
		* Handle INVITE method.
		*/
		if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
			pjsip_rx_data* rdata = e->body.rx_msg.rdata;
			// --
			const pj_str_t headerRPID = { "Remote-Party-ID",15 };
			pjsip_generic_string_hdr* hsr;
			hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerRPID, NULL);
			if (hsr) {
				CString str = MSIP::PjToStr(&hsr->hvalue, true);
				call_user_data* user_data = (call_user_data*)pjsua_call_get_user_data(call_id);
				if (user_data) {
					user_data->CS.Lock();
					user_data->callerID = MSIP::PjToStr(&hsr->hvalue, true);
					int pos = user_data->callerID.Find(';');
					if (pos != -1) {
						user_data->callerID = user_data->callerID.Left(pos);
					}
					user_data->CS.Unlock();
				}
			}
			// -- end reason
		}	
		return;
	}
	const pjsip_method info_method = {
		PJSIP_OTHER_METHOD,
		{ "INFO", 4 }
	};
	if (pjsip_method_cmp(&tsx->method, &info_method) == 0) {
		/*
		* Handle INFO method.
		*/
		if (tsx->role == PJSIP_ROLE_UAS && tsx->state == PJSIP_TSX_STATE_TRYING) {
			if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
				pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
				pjsip_msg_body *body = rdata->msg_info.msg->body;
				int code = 0;
				if (body && body->len
					&& pj_strcmp2(&body->content_type.type, "application") == 0
					&& pj_strcmp2(&body->content_type.subtype, "dtmf-relay") == 0) {
					code = 400;
					pj_scanner scanner;
					pj_scan_init(&scanner, (char*)body->data, body->len, PJ_SCAN_AUTOSKIP_WS, 0);
					char digit;
					int duration = 250;
					while (!pj_scan_is_eof(&scanner)) {
						pj_str_t key;
						pj_scan_get_until_chr(&scanner, "=", &key);
						pj_strtrim(&key);
						if (key.slen && !pj_scan_is_eof(&scanner)) {
							scanner.curptr++;
							pj_str_t value;
							pj_scan_get_until_chr(&scanner, "\r\n", &value);
							pj_strtrim(&value);
							if (pj_stricmp2(&key, "Signal") == 0) {
								if (value.slen == 1) {
									digit = *value.ptr;
									code = 200;
								}
							}
							else if (pj_stricmp2(&key, "Duration") == 0) {
								int res = 0;
								for (int i = 0; i < (unsigned)value.slen; ++i) {
									res = res * 10 + (value.ptr[i] - '0');
									res = res;
								}
								if (res >= 100 || res <= 5000) {
									duration = res;
								}
							}
						}
					}
					pj_scan_fini(&scanner);
					if (code == 200) {
						on_dtmf_digit(-1, digit);
					}
				}
				else if (!body || !body->len) {
					/* 200/OK */
					code = 200;
				}
				if (code) {
					/* Answer incoming INFO */
					pjsip_tx_data *tdata;
					if (pjsip_endpt_create_response(tsx->endpt, rdata,
						code, NULL, &tdata) == PJ_SUCCESS
						) {
						pjsip_tsx_send_msg(tsx, tdata);
					}
				}
			}
		}
		return;
	}
	const pjsip_method cancel_method = {
		PJSIP_CANCEL_METHOD,
		{ "CANCEL", 6 }
	};
	if (pjsip_method_cmp(&tsx->method, &cancel_method) == 0) {
		/*
		* Handle CANCEL method.
		*/
		if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
			pjsip_rx_data *rdata = e->body.rx_msg.rdata;
			// -- reason
			const pj_str_t headerReason = { "Reason",6 };
			pjsip_generic_string_hdr *hsr;
			hsr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &headerReason, NULL);
			if (hsr) {
				CString str = MSIP::PjToStr(&hsr->hvalue, true);
				int pos = str.Find(_T("text=\""));
				if (pos != -1) {
					str = str.Mid(pos + 6);
					pos = str.Find(_T("\""));
					if (pos != -1) {
						str = str.Left(pos);
						call_user_data* user_data = (call_user_data*)pjsua_call_get_user_data(call_id);
						if (user_data) {
							user_data->CS.Lock();
							user_data->reason = str;
							user_data->CS.Unlock();
						}
					}
				}
			}
			// -- end reason
		}
		return;
	}
	if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {
		// display declined REFER status
		const pjsip_method refer_method = {
			PJSIP_OTHER_METHOD,
			{ "REFER", 5 }
		};
		if (pjsip_method_cmp(&tsx->method, &refer_method) == 0 && tsx->status_code / 100 != 2) {
			pj_bool_t cont;
			on_call_transfer_status(call_id,
				tsx->status_code,
				&tsx->status_text,
				PJ_FALSE,
				&cont);
		}
	}
}

static pjsip_redirect_op on_call_redirected(pjsua_call_id call_id,
	const pjsip_uri* target,
	const pjsip_event* e)
{
	return PJSIP_REDIRECT_ACCEPT_REPLACE;
}

static DWORD WINAPI NetworkChangeThread(LPVOID lpParam)
{
	while (NotifyAddrChange(NULL, NULL) == NO_ERROR) {
		PostMessage(mainDlg->m_hWnd, UM_NETWORK_CHANGE, 0, 0);
	}
	return 0;
}

CmainDlg::~CmainDlg(void)
{
}

void CmainDlg::OnDestroy()
{
	if (mmNotificationClient) {
		delete mmNotificationClient;
	}
	WTSUnRegisterSessionNotification(m_hWnd);

	PJDestroy(true);

	accountSettings.SettingsSave();

	RemoveJumpList();
	if (tnd.hWnd) {
		Shell_NotifyIcon(NIM_DELETE, &tnd);
	}
	UnloadLangPackModule();

	CBaseDialog::OnDestroy();
}

void CmainDlg::PostNcDestroy()
{
	CBaseDialog::PostNcDestroy();
	delete this;
}

void CmainDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDialog::DoDataExchange(pDX);
	//	DDX_Control(pDX, IDD_MAIN, *mainDlg);
	DDX_Control(pDX, IDC_MAIN_MENU, m_ButtonMenu);
}

BEGIN_MESSAGE_MAP(CmainDlg, CBaseDialog)
	ON_WM_CREATE()
	ON_WM_SYSCOMMAND()
	ON_WM_QUERYENDSESSION()
	ON_WM_TIMER()
	ON_WM_MOVE()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_CTLCOLOR()
	ON_WM_CONTEXTMENU()
	ON_WM_DEVICECHANGE()
	ON_WM_WTSSESSION_CHANGE()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDC_MAIN_MENU, OnBnClickedMenu)
	ON_MESSAGE(UM_UPDATEWINDOWTEXT, OnUpdateWindowText)
	ON_MESSAGE(UM_NOTIFYICON, onTrayNotify)
	ON_MESSAGE(UM_CREATE_RINGING, onCreateRingingDlg)
	ON_MESSAGE(UM_REFRESH_LEVELS, onRefreshLevels)
	ON_MESSAGE(UM_ON_REG_STATE2, onRegState2)
	ON_MESSAGE(UM_ON_CALL_STATE, onCallState)
	ON_MESSAGE(UM_ON_INCOMING_CALL, onIncomingCall)
	ON_MESSAGE(UM_ON_MWI_INFO, onMWIInfo)
	ON_MESSAGE(UM_ON_CALL_MEDIA_STATE, onCallMediaState)
	ON_MESSAGE(UM_ON_CALL_TRANSFER_STATUS, onCallTransferStatus)
	ON_MESSAGE(UM_ON_PLAYER_STOP, onPlayerStop)
	ON_MESSAGE(UM_ON_COMMAND_LINE, onCommandLine)
	ON_MESSAGE(UM_ON_PAGER, onPager)
	ON_MESSAGE(UM_ON_PAGER_STATUS, onPagerStatus)
	ON_MESSAGE(UM_ON_BUDDY_STATE, onBuddyState)
	ON_MESSAGE(UM_USERS_DIRECTORY, onUsersDirectoryLoaded)
	ON_MESSAGE(UM_CUSTOM, onCustomLoaded)
	ON_MESSAGE(UM_NETWORK_CHANGE, OnNetworkChange)
	ON_MESSAGE(WM_POWERBROADCAST, OnPowerBroadcast)
	ON_MESSAGE(WM_COPYDATA, onCopyData)
	ON_MESSAGE(UM_CALL_ANSWER, onCallAnswer)
	ON_MESSAGE(UM_CALL_HANGUP, onCallHangup)
	ON_MESSAGE(UM_TAB_ICON_UPDATE, onTabIconUpdate)
	ON_MESSAGE(UM_SET_PANE_TEXT, onSetPaneText)
	ON_MESSAGE(UM_ON_ACCOUNT, OnAccount)
	ON_COMMAND(ID_ACCOUNT_ADD, OnMenuAccountAdd)
	ON_COMMAND_RANGE(ID_ACCOUNT_CHANGE_RANGE, ID_ACCOUNT_CHANGE_RANGE + 99, OnMenuAccountChange)
	ON_COMMAND_RANGE(ID_ACCOUNT_EDIT_RANGE, ID_ACCOUNT_EDIT_RANGE + 99, OnMenuAccountEdit)
	ON_COMMAND(ID_ACCOUNT_EDIT_LOCAL, OnMenuAccountLocalEdit)
	ON_COMMAND_RANGE(ID_CUSTOM_RANGE, ID_CUSTOM_RANGE + 99, OnMenuCustomRange)
	ON_COMMAND(ID_UPDATES, OnCheckUpdates)
	ON_MESSAGE(UM_UPDATE_CHECKER_LOADED, OnUpdateCheckerLoaded)
	ON_COMMAND(ID_SETTINGS, OnMenuSettings)
	ON_COMMAND(ID_SHORTCUTS, OnMenuShortcuts)
	ON_COMMAND(ID_ALWAYS_ON_TOP, OnMenuAlwaysOnTop)
	ON_COMMAND(ID_LOG, OnMenuLog)
	ON_COMMAND(ID_EXIT, OnMenuExit)
	ON_NOTIFY(TCN_SELCHANGE, IDC_MAIN_TAB, &CmainDlg::OnTcnSelchangeTab)
	ON_NOTIFY(TCN_SELCHANGING, IDC_MAIN_TAB, &CmainDlg::OnTcnSelchangingTab)
	ON_COMMAND(ID_MENU_WEBSITE, OnMenuWebsite)
	ON_COMMAND(ID_MENU_HELP, OnMenuHelp)
	ON_COMMAND(ID_MENU_ADDL, OnMenuAddl)
	ON_COMMAND(ID_MUTE_INPUT, OnMuteInput)
	ON_COMMAND(ID_MUTE_OUTPUT, OnMuteOutput)
END_MESSAGE_MAP()

LRESULT CmainDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_TASKBARRESTARTMESSAGE) {
		ShowTrayIcon();
	}
	return CBaseDialog::WindowProc(message, wParam, lParam);
}

BOOL CmainDlg::PreTranslateMessage(MSG* pMsg)
{
	BOOL catched = FALSE;
	if (accountSettings.enableMediaButtons) {
		if (pMsg->message == WM_SHELLHOOKMESSAGE) {
			onShellHookMessage(pMsg->wParam, pMsg->lParam);
		}
	}
	if (!catched) {
		return CBaseDialog::PreTranslateMessage(pMsg);
	}
	else {
		return TRUE;
	}
}

// CmainDlg message handlers

void CmainDlg::OnBnClickedOk()
{
}

void CmainDlg::OnBnClickedMenu()
{
	m_ButtonMenu.ModifyStyle(BS_DEFPUSHBUTTON, BS_PUSHBUTTON);
	MainPopupMenu(true);
	TabFocusSet();
}

CmainDlg::CmainDlg(CWnd* pParent /*=NULL*/)
	: CBaseDialog(CmainDlg::IDD, pParent)
{
#ifdef _DEBUG
	if (AllocConsole()) {
		HANDLE console = NULL;
		console = GetStdHandle(STD_OUTPUT_HANDLE);
		freopen("CONOUT$", "wt", stdout);
	}
#endif

	this->m_hWnd = NULL;
	mmNotificationClient = NULL;
	updateCheckerShow = false;

	pageCalls = NULL;
	pageContacts = NULL;
	mainDlg = this;
	widthAdd = 0;
	heightAdd = 0;

	m_tabPrev = -1;
	newMessages = false;
	missed = false;

	usersDirectoryLoaded = false;
	shortcutsURLLoaded = false;
	CString audioCodecsCaptions = _T("opus/48000/2;Opus 24 kHz;\
PCMA/8000/1;G.711 A-law;\
PCMU/8000/1;G.711 u-law;\
G722/16000/1;G.722 16 kHz;\
G7221/16000/1;G.722.1 16 kHz;\
G7221/32000/1;G.722.1 32 kHz;\
G723/8000/1;G.723 8 kHz;\
G729/8000/1;G.729 8 kHz;\
GSM/8000/1;GSM 8 kHz;\
GSM-EFR/8000/1;GSM-EFR 8 kHz;\
AMR/8000/1;AMR 8 kHz;\
AMR-WB/16000/1;AMR-WB 16 kHz;\
iLBC/8000/1;iLBC 8 kHz;\
speex/32000/1;Speex 32 kHz;\
speex/16000/1;Speex 16 kHz;\
speex/8000/1;Speex 8 kHz;\
SILK/24000/1;SILK 24 kHz;\
SILK/16000/1;SILK 16 kHz;\
SILK/12000/1;SILK 12 kHz;\
SILK/8000/1;SILK 8 kHz;\
L16/8000/1;LPCM 8 kHz;\
L16/8000/2;LPCM 8 kHz Stereo;\
L16/16000/1;LPCM 16 kHz;\
L16/16000/2;LPCM 16 kHz Stereo;\
L16/44100/1;LPCM 44 kHz;\
L16/44100/2;LPCM 44 kHz Stereo;\
L16/48000/1;LPCM 48 kHz;\
L16/48000/2;LPCM 48 kHz Stereo");
	int pos = 0;
	CString resToken = audioCodecsCaptions.Tokenize(_T(";"), pos);
	while (!resToken.IsEmpty()) {
		audioCodecList.AddTail(resToken);
		resToken = audioCodecsCaptions.Tokenize(_T(";"), pos);
	}

	wchar_t szBuf[STR_SZ];
	wchar_t szLocale[STR_SZ];
	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, szBuf, STR_SZ);
	_tcscpy(szLocale, szBuf);
	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, szBuf, STR_SZ);
	if (_tcsclen(szBuf) != 0) {
		_tcscat(szLocale, _T("_"));
		_tcscat(szLocale, szBuf);
	}
	::GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, szBuf, STR_SZ);
	if (_tcsclen(szBuf) != 0) {
		_tcscat(szLocale, _T("."));
		_tcscat(szLocale, szBuf);
	}
	_tsetlocale(LC_ALL, szLocale); // e.g. szLocale = "English_United States.1252"

	LoadLangPackModule();

	Create(IDD, pParent);
}

int CmainDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	WM_TASKBARRESTARTMESSAGE = RegisterWindowMessage(_T("TaskbarCreated"));
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	bool setpos = false;
	if (accountSettings.noResize) {
		lpCreateStruct->style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
		::SetWindowLong(m_hWnd, GWL_STYLE, lpCreateStruct->style);
		CRect rectStub;
		GetClientRect(&rectStub);
		AdjustWindowRectEx(&rectStub, lpCreateStruct->style, FALSE, lpCreateStruct->dwExStyle);
		lpCreateStruct->cx = rectStub.Width();
		lpCreateStruct->cy = rectStub.Height();
		setpos = true;
	}

	ShortcutsLoad();
	shortcutsEnabled = accountSettings.enableShortcuts;
	shortcutsBottom = accountSettings.shortcutsBottom;
	if (accountSettings.enableShortcuts) {
		if (shortcutsBottom) {
			if (shortcuts.GetCount()) {
				if (shortcuts.GetCount() > _GLOBAL_SHORTCUTS_QTY / 2) {
					heightAdd += 10 + (shortcuts.GetCount() + shortcuts.GetCount() % 2) * 25 / 2;
				}
				else {
					heightAdd += 10 + shortcuts.GetCount() * 25;
				}
			}
		}
		else {
			if (shortcuts.GetCount() > 12) {
				widthAdd += 200;
			}
			else {
				widthAdd += 140;
			}
		}
	}
	int heightFix = 0;
	if (setpos || widthAdd || heightAdd || heightFix) {
		SetWindowPos(NULL, 0, 0, lpCreateStruct->cx + widthAdd, lpCreateStruct->cy + heightAdd + heightFix, SWP_NOMOVE | SWP_NOZORDER);
	}

	if (langPack.rtl) {
		ModifyStyleEx(0, WS_EX_LAYOUTRTL);
	}

	return CBaseDialog::OnCreate(lpCreateStruct);
}

BOOL CmainDlg::OnInitDialog()
{
	CBaseDialog::OnInitDialog();

	if (lstrcmp(theApp.m_lpCmdLine, _T("/hidden")) == 0) {
		accountSettings.hidden = TRUE;
		theApp.m_lpCmdLine = NULL;
	}

	WTSRegisterSessionNotification(m_hWnd, NOTIFY_FOR_THIS_SESSION);
	mmNotificationClient = new CMMNotificationClient();

	CreateThread(NULL, 0, NetworkChangeThread, 0, 0, NULL);

	pj_ready = false;

	settingsDlg = NULL;
	shortcutsDlg = NULL;

	messagesDlg = new MessagesDlg(this);
	transferDlg = NULL;
	accountDlg = NULL;

	m_lastInputTime = 0;
	m_idleCounter = 0;
	m_PresenceStatus = PJRPID_ACTIVITY_UNKNOWN;

#ifdef _GLOBAL_VIDEO
	previewWin = NULL;
#endif

	SetupJumpList();
	m_hIcon = theApp.LoadIcon(IDR_MAINFRAME);
	iconSmall = (HICON)LoadImage(
		AfxGetInstanceHandle(),
		MAKEINTRESOURCE(IDR_MAINFRAME),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
	PostMessage(WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);

	TranslateDialog(this->m_hWnd);

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// add tray icon or set tnd.hWnd = NULL;
	ShowTrayIcon();

	m_bar.Create(this);
	m_bar.SetIndicators(indicators, sizeof(indicators) / sizeof(indicators[0]));
	m_bar.SetPaneInfo(IDS_STATUSBAR, IDS_STATUSBAR, SBPS_STRETCH, 0);
	m_bar.SetPaneInfo(IDS_STATUSBAR2, IDS_STATUSBAR2, SBPS_NOBORDERS, 0);
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, IDS_STATUSBAR);

	AutoMove(m_bar.m_hWnd, 0, 100, 100, 0);
	//--set window pos
	CRect screenRect;
	if (accountSettings.multiMonitor) {
		MSIP::GetScreenRect(&screenRect);
	}
	else {
		SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
	}
	CRect clientRect;
	GetClientRect(&clientRect);
	CRect rect;
	GetWindowRect(&rect);

	int mx;
	int my;
	int mW = accountSettings.mainW > 0 ? accountSettings.mainW : rect.Width();

	int mH = accountSettings.mainH > 0 ? accountSettings.mainH : rect.Height();
	// coors not specified, first run
	if (!accountSettings.mainX && !accountSettings.mainY) {
		CRect primaryScreenRect;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &primaryScreenRect, 0);
		mx = primaryScreenRect.Width() - mW - widthAdd;
		my = primaryScreenRect.Height() - mH;
	}
	else {
		int maxLeft = screenRect.right - mW;
		if (accountSettings.mainX > maxLeft) {
			mx = maxLeft;
		}
		else {
			mx = accountSettings.mainX < screenRect.left ? screenRect.left : accountSettings.mainX;
		}
		int maxTop = screenRect.bottom - mH;
		if (accountSettings.mainY > maxTop) {
			my = maxTop;
		}
		else {
			my = accountSettings.mainY < screenRect.top ? screenRect.top : accountSettings.mainY;
		}
	}

	//--set messages window pos/size
	messagesDlg->GetWindowRect(&rect);
	int messagesX;
	int messagesY;
	int messagesW = accountSettings.messagesW > 0 ? accountSettings.messagesW : 550;
	int messagesH = accountSettings.messagesH > 0 ? accountSettings.messagesH : mH;
	// coors not specified, first run
	if (!accountSettings.messagesX && !accountSettings.messagesY) {
		accountSettings.messagesX = mx - messagesW;
		accountSettings.messagesY = my;
	}
	int maxLeft = screenRect.right - messagesW;
	if (accountSettings.messagesX > maxLeft) {
		messagesX = maxLeft;
	}
	else {
		messagesX = accountSettings.messagesX < screenRect.left ? screenRect.left : accountSettings.messagesX;
	}
	int maxTop = screenRect.bottom - messagesH;
	if (accountSettings.messagesY > maxTop) {
		messagesY = maxTop;
	}
	else {
		messagesY = accountSettings.messagesY < screenRect.top ? screenRect.top : accountSettings.messagesY;
	}
	messagesDlg->SetWindowPos(NULL, messagesX, messagesY, messagesW, messagesH, SWP_NOZORDER);

	SetWindowPos(accountSettings.alwaysOnTop ? &CWnd::wndTopMost : &CWnd::wndNoTopMost, mx, my, mW, mH, NULL);

	imageListStatus = new CImageList();
	imageListStatus->Create(16,16,ILC_COLOR32,3,3);
	imageListStatus->SetBkColor(RGB(255, 255, 255));
	imageListStatus->Add(LoadImageIcon(IDI_BLANK));
	imageListStatus->Add(LoadImageIcon(IDI_UNKNOWN));
	imageListStatus->Add(LoadImageIcon(IDI_OFFLINE));
	imageListStatus->Add(LoadImageIcon(IDI_AWAY));
	imageListStatus->Add(LoadImageIcon(IDI_ONLINE));
	imageListStatus->Add(LoadImageIcon(IDI_ON_THE_PHONE));
	imageListStatus->Add(LoadImageIcon(IDI_BUSY));
	imageListStatus->Add(LoadImageIcon(IDI_DEFAULT));
	imageListStatus->Add(LoadImageIcon(IDI_UNKNOWN_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_OFFLINE_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_AWAY_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_ONLINE_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_ON_THE_PHONE_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_BUSY_STARRED));
	imageListStatus->Add(LoadImageIcon(IDI_DEFAULT_STARRED));

	CTabCtrl* tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	CRect tabRect;
	tab->GetWindowRect(&tabRect);
	ScreenToClient(&tabRect);
	TC_ITEM tabItem;
	CRect lineRect;
	lineRect.bottom = 3;
	MapDialogRect(&lineRect);
	tabRect.top += lineRect.bottom;
	tabRect.bottom += lineRect.bottom;
	tab->SetWindowPos(NULL, tabRect.left, tabRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	tabItem.mask = TCIF_TEXT | TCIF_PARAM;

	m_ButtonMenu.SetIcon(LoadImageIcon(IDI_DROPDOWN));

	if (widthAdd) {
		CRect pageRect;
		m_ButtonMenu.GetWindowRect(pageRect);
		ScreenToClient(pageRect);
		m_ButtonMenu.SetWindowPos(NULL, pageRect.left + widthAdd, pageRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		//--
		tabRect.right += widthAdd;
		tab->SetWindowPos(NULL, 0, 0, tabRect.Width(), tabRect.Height(), SWP_NOZORDER | SWP_NOMOVE);
	}

	AutoMove(tab->m_hWnd, 0, 0, 100, 0);
	AutoMove(m_ButtonMenu.m_hWnd, 100, 0, 0, 0);

	BYTE offset = tabRect.bottom - 1;
	CRect pageRect;

	pageDialer = new Dialer(this);
	tabItem.pszText = Translate(_T("Phone"));
	tabItem.iImage = 0;
	tabItem.lParam = (LPARAM)pageDialer;
	tab->InsertItem(99, &tabItem);
	pageDialer->GetWindowRect(pageRect);
	int pageWidth = pageRect.Width() + (clientRect.Width() - pageRect.Width()) / 3;
	int offsetX = (clientRect.Width() - pageWidth) / 2;
	pageDialer->SetWindowPos(NULL, offsetX, offset, pageWidth, pageRect.Height(), SWP_NOZORDER);
	AutoMove(pageDialer->m_hWnd, 40, 40, 20, 20);

		pageCalls = new Calls(this);
		pageCalls->OnCreated();
		tabItem.pszText = Translate(_T("Logs"));
		tabItem.iImage = 1;
		tabItem.lParam = (LPARAM)pageCalls;
		tab->InsertItem(99, &tabItem);
		pageCalls->GetWindowRect(pageRect);
		pageCalls->SetWindowPos(NULL, 0, offset, pageRect.Width() + widthAdd, pageRect.Height() + heightAdd, SWP_NOZORDER);
		AutoMove(pageCalls->m_hWnd, 0, 0, 100, 100);

		pageContacts = new Contacts(this);
		pageContacts->OnCreated();
		tabItem.pszText = Translate(_T("Contacts"));
		tabItem.iImage = 3;
		tabItem.lParam = (LPARAM)pageContacts;
		tab->InsertItem(99, &tabItem);
		pageContacts->GetWindowRect(pageRect);
		pageContacts->SetWindowPos(NULL, 0, offset, pageRect.Width() + widthAdd, pageRect.Height() + heightAdd, SWP_NOZORDER);
		AutoMove(pageContacts->m_hWnd, 0, 0, 100, 100);

	tab->SetCurSel(accountSettings.activeTab);

	BOOL minimized = !lstrcmp(theApp.m_lpCmdLine, _T("/minimized"));
	if (minimized) {
		theApp.m_lpCmdLine = _T("");
	}
	if (MACRO_SILENT) {
		minimized = true;
	}

	m_startMinimized = (!firstRun && minimized) || accountSettings.hidden || accountSettings.minimized;

	InitUI();
	OnAccountChanged();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CmainDlg::InitUI()
{
	onMWIInfo(0, 0); // voicemail button
	SetPaneText2();
	SetWindowText(_T(_GLOBAL_NAME_NICE));
	UpdateWindowText();
	pageDialer->SetName();
}

void CmainDlg::ShowTrayIcon()
{
	// add tray icon
	tnd.cbSize = sizeof(NOTIFYICONDATA);
	tnd.hWnd = this->GetSafeHwnd();
	tnd.uID = IDR_MAINFRAME;
	tnd.uCallbackMessage = UM_NOTIFYICON;
	tnd.uFlags = NIF_MESSAGE | NIF_ICON;
	iconMissed = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MISSED));
	iconInactive = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_INACTIVE));
	tnd.hIcon = iconInactive;
	DWORD dwMessage = NIM_ADD;
	Shell_NotifyIcon(dwMessage, &tnd);
}

void CmainDlg::OnCreated()
{
	LRESULT pResult;
	mainDlg->OnTcnSelchangeTab(NULL, &pResult);

	if (!m_startMinimized) {
		ShowWindow(SW_SHOW);
		TabFocusSet();
	}

	PJCreate();

	if (lstrlen(theApp.m_lpCmdLine)) {
		CommandLine(theApp.m_lpCmdLine);
		theApp.m_lpCmdLine = NULL;
	}
	PJAccountAdd();
	//--
	WM_SHELLHOOKMESSAGE = RegisterWindowMessage(_T("SHELLHOOK"));
	if (WM_SHELLHOOKMESSAGE) {
		RegisterShellHookWindow(m_hWnd);
	}
}

void CmainDlg::TrayIconUpdateTip()
{
	if (tnd.hWnd) {
		CString tip;
		tip = _T(_GLOBAL_NAME_NICE);
		if (accountSettings.accountId) {
			if (!accountSettings.account.label.IsEmpty()) {
				//tip.AppendFormat(_T("\r\n%s: %s"), Translate(_T("Account")), accountSettings.account.label);
				tip.AppendFormat(_T("\r\n%s"), accountSettings.account.label);
			}
			else if (!accountSettings.account.username.IsEmpty()) {
				tip.AppendFormat(_T("\r\n%s"), accountSettings.account.username);
			}
			if (!accountSettings.account.displayName.IsEmpty()) {
				tip.AppendFormat(_T("\r\n%s"), accountSettings.account.displayName);
			}
		}
		lstrcpyn(tnd.szTip, (LPCTSTR)tip, sizeof(tnd.szTip));
		tnd.uFlags = NIF_TIP;
		DWORD dwMessage = NIM_MODIFY;
		Shell_NotifyIcon(dwMessage, &tnd);
	}
}

void CmainDlg::BaloonPopup(CString title, CString message, DWORD flags)
{
	if (tnd.hWnd) {
		lstrcpyn(tnd.szInfo, message, sizeof(tnd.szInfo));
		lstrcpyn(tnd.szInfoTitle, title, sizeof(tnd.szInfoTitle));
		tnd.uFlags = NIF_INFO | NIF_ICON;
		tnd.hIcon = iconSmall;
		tnd.dwInfoFlags = flags;
		DWORD dwMessage = NIM_MODIFY;
		Shell_NotifyIcon(dwMessage, &tnd);
	}
}

void CmainDlg::SwitchDND(int state)
{
	if (state == -1) {
		accountSettings.DND = !accountSettings.DND;
	} else {
		accountSettings.DND = state;
	}
	CButton *button = (CButton *)pageDialer->GetDlgItem(IDC_DIALER_DND);
	if (button) {
		button->SetCheck(accountSettings.DND ? BST_CHECKED : BST_UNCHECKED);
	}
	mainDlg->PublishStatus();
	accountSettings.SettingsSave();
}

void CmainDlg::OnMenuAccountAdd()
{
	if (!accountSettings.hidden) {
		if (!accountDlg) {
			accountDlg = new AccountDlg(this);
		}
		else {
			accountDlg->SetForegroundWindow();
		}
		if (accountDlg) {
			accountDlg->Load(-1);
		}
	}
}
void CmainDlg::OnMenuAccountChange(UINT nID)
{
	if (accountSettings.accountId) {
		PJAccountDelete(true);
	}
	int idNew = nID - ID_ACCOUNT_CHANGE_RANGE + 1;
	if (accountSettings.accountId != idNew) {
		accountSettings.accountId = idNew;
		accountSettings.AccountLoad(accountSettings.accountId, &accountSettings.account);
	}
	else {
		accountSettings.accountId = 0;
		InitUI();
	}
	OnAccountChanged();
	accountSettings.SettingsSave();
	mainDlg->PJAccountAdd();
}

void CmainDlg::OnMenuAccountEdit(UINT nID)
{
	if (!accountDlg) {
		accountDlg = new AccountDlg(this);
	}
	else {
		accountDlg->SetForegroundWindow();
	}
	if (accountDlg) {
		int id = accountSettings.accountId > 0 ? accountSettings.accountId : nID - ID_ACCOUNT_EDIT_RANGE + 1;
		accountDlg->Load(id ? id : -1);
	}
}

void CmainDlg::OnMenuAccountLocalEdit()
{
	if (MACRO_ENABLE_LOCAL_ACCOUNT) {
		if (!accountDlg) {
			accountDlg = new AccountDlg(this);
		}
		else {
			accountDlg->SetForegroundWindow();
		}
		if (accountDlg) {
			accountDlg->Load(0);
		}
	}
}

void CmainDlg::OnMenuCustomRange(UINT nID)
{
}

void CmainDlg::OnMenuSettings()
{
	if (!accountSettings.hidden) {
		if (!settingsDlg) {
			bool showDlg = true;
			if (showDlg) {
				settingsDlg = new SettingsDlg(this);
			}
		}
		else {
			settingsDlg->SetForegroundWindow();
		}
	}
}

void CmainDlg::OnMenuShortcuts()
{
	if (!accountSettings.hidden) {
		if (!shortcutsDlg) {
			shortcutsDlg = new ShortcutsDlg(this);
		}
		else {
			shortcutsDlg->SetForegroundWindow();
		}
	}
}

void CmainDlg::OnMenuAlwaysOnTop()
{
	accountSettings.alwaysOnTop = 1 - accountSettings.alwaysOnTop;
	AccountSettingsPendingSave();
	SetWindowPos(accountSettings.alwaysOnTop ? &this->wndTopMost : &this->wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void CmainDlg::OnMenuLog()
{
	ShellExecute(NULL, NULL, accountSettings.logFile, NULL, NULL, SW_SHOWNORMAL);
}

void CmainDlg::OnMenuExit()
{
	this->DestroyWindow();
}

LRESULT CmainDlg::onTrayNotify(WPARAM wParam, LPARAM lParam)
{
	UINT uMsg = (UINT)lParam;
	switch (uMsg)
	{
	case NIN_BALLOONUSERCLICK:
		onTrayNotify(NULL, WM_LBUTTONUP);
		break;
	case WM_LBUTTONUP:
		if (this->IsWindowVisible() && !IsIconic())
		{
			if (wParam) {
				ShowWindow(SW_HIDE);
			}
			else {
				//set up a generic keyboard event
				INPUT keyInput;
				keyInput.type = INPUT_KEYBOARD;
				keyInput.ki.wScan = 0; //hardware scan code for key
				keyInput.ki.time = 0;
				keyInput.ki.dwExtraInfo = 0;

				//set focus to the hWnd (sending Alt allows to bypass limitation)
				keyInput.ki.wVk = VK_MENU;
				keyInput.ki.dwFlags = 0;   //0 for key press
				SendInput(1, &keyInput, sizeof(INPUT));

				SetForegroundWindow(); //sets the focus

				keyInput.ki.wVk = VK_MENU;
				keyInput.ki.dwFlags = KEYEVENTF_KEYUP;  //for key release
				SendInput(1, &keyInput, sizeof(INPUT));
			}
		}
		else
		{
			bool blockRestore = false;
			if (accountSettings.hidden) {
				blockRestore = true;
			}
			if (!blockRestore) {
				if (IsIconic()) {
					ShowWindow(SW_RESTORE);
				}
				else {
					ShowWindow(SW_SHOW);
				}
				SetForegroundWindow();
				if (missed) {
					GotoTabLParam((LPARAM)pageCalls);
					missed = false;
					UpdateWindowText();
				}
				// -- show ringing dialogs
				int count = ringinDlgs.GetCount();
				for (int i = 0; i < count; i++) {
					RinginDlg *ringinDlg = ringinDlgs.GetAt(i);
					ringinDlg->ShowWindow(SW_SHOWNORMAL);
				}
				// -- show messages dialog
				if (MACRO_SILENT && ((!accountSettings.singleMode && messagesDlg->GetCallsCount()) || newMessages)) {
					newMessages = false;
					messagesDlg->ShowWindow(SW_SHOW);
				}
				//--
				TabFocusSet();
			}
		}
		break;
	case WM_RBUTTONUP:
		MainPopupMenu();
		break;
	}
	return TRUE;
}

void CmainDlg::MainPopupMenu(bool isMenuButton)
{
	CString str;
	CPoint point;
	if (isMenuButton) {
		CWnd *menuButton = mainDlg->GetDlgItem(IDC_MAIN_MENU);
		CRect rect;
		menuButton->GetWindowRect(rect);
		point = rect.TopLeft();
	}
	else {
		GetCursorPos(&point);
	}
	CMenu menu;
	menu.CreatePopupMenu();
	CMenu* tracker = &menu;
	bool basic = false;
	if (!accountSettings.hidden && !basic) {

			// -- add
			tracker->AppendMenu(MF_STRING, ID_ACCOUNT_ADD, Translate(_T("Add Account...")));
			//-- edit
			CMenu editMenu;
			editMenu.CreatePopupMenu();
			bool checked = false;
			Account acc;
			int i = 0;
			while (true) {
				if (!accountSettings.AccountLoad(i + 1, &acc)) {
					break;
				}
				if (!acc.label.IsEmpty()) {
					str = acc.label;
				}
				else {
					str.Format(_T("%s@%s"), acc.username, acc.domain);
				}
					tracker->InsertMenu(ID_ACCOUNT_ADD, (accountSettings.accountId == i + 1 ? MF_CHECKED : 0), ID_ACCOUNT_CHANGE_RANGE + i, str);
					editMenu.AppendMenu(MF_STRING, ID_ACCOUNT_EDIT_RANGE + i, str);
					if (!checked) {
						checked = accountSettings.accountId == i + 1;
					}
					i++;
			}
			if (i == 1) {
					MENUITEMINFO menuItemInfo;
					menuItemInfo.cbSize = sizeof(MENUITEMINFO);
					menuItemInfo.fMask = MIIM_STRING;
					menuItemInfo.dwTypeData = Translate(_T("Make Active"));
					tracker->SetMenuItemInfo(ID_ACCOUNT_CHANGE_RANGE, &menuItemInfo);
			}
			str = Translate(_T("Edit Account"));
			str.Append(_T("\tCtrl+M"));
			if (i == 1) {
					tracker->InsertMenu(ID_ACCOUNT_ADD, 0, ID_ACCOUNT_EDIT_RANGE, str);
			}
			else if (i > 1) {
				tracker->InsertMenu(ID_ACCOUNT_ADD, MF_SEPARATOR);
				if (checked) {
					tracker->InsertMenu(ID_ACCOUNT_ADD, 0, ID_ACCOUNT_EDIT_RANGE, str);
				}
				else {
					tracker->InsertMenu(ID_ACCOUNT_ADD, MF_POPUP, (UINT_PTR)editMenu.m_hMenu, Translate(_T("Edit Account")));
				}
			}

		if (accountSettings.enableLocalAccount && MACRO_ENABLE_LOCAL_ACCOUNT) {
			str = Translate(_T("Edit Local Account"));
			str.Append(_T("\tCtrl+L"));
			tracker->AppendMenu(MF_STRING, ID_ACCOUNT_EDIT_LOCAL, str);
		}

		str = Translate(_T("Settings"));
		str.Append(_T("\tCtrl+P"));
		tracker->AppendMenu(MF_STRING, ID_SETTINGS, str);
		tracker->AppendMenu(MF_SEPARATOR);
		str = Translate(_T("Shortcuts"));
		str.Append(_T("\tCtrl+S"));
		tracker->AppendMenu(MF_STRING, ID_SHORTCUTS, str);
	}

	bool separator = false;
	if (!accountSettings.hidden) {
		if (!separator) {
			tracker->AppendMenu(MF_SEPARATOR);
			separator = true;
		}
		tracker->AppendMenu(MF_STRING | (accountSettings.alwaysOnTop ? MF_CHECKED : 0), ID_ALWAYS_ON_TOP, Translate(_T("Always on Top")));
	}
			if (!separator) {
				tracker->AppendMenu(MF_SEPARATOR);
				separator = true;
			}
			tracker->AppendMenu(MF_STRING | (!accountSettings.enableLog ? MF_DISABLED | MF_GRAYED : 0), ID_LOG, Translate(_T("View Log File")));

	separator = false;

	if (!separator) {
		tracker->AppendMenu(MF_SEPARATOR);
		separator = true;
	}
	str = Translate(_T("Visit Website"));
	str.Append(_T("\tCtrl+W"));
	tracker->AppendMenu(MF_STRING, ID_MENU_WEBSITE, str);
	separator = false;

	if (!separator) {
		tracker->AppendMenu(MF_SEPARATOR);
		separator = true;
	}
	str = Translate(_T("Help"));
	str.AppendFormat(_T("\tVer. %s"), _T(_GLOBAL_VERSION));
	tracker->AppendMenu(MF_STRING, ID_MENU_HELP, str);
	separator = false;

	tracker->AppendMenu(MF_SEPARATOR);
	str = Translate(_T("Exit"));
	str.Append(_T("\tCtrl+Q"));
	tracker->AppendMenu(MF_STRING, ID_EXIT, str);

	MENUITEMINFO menuItemInfo;
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_FTYPE;
	tracker->GetMenuItemInfo(0, &menuItemInfo, TRUE);
	if (menuItemInfo.fType == MFT_SEPARATOR) {
		tracker->RemoveMenu(0, MF_BYPOSITION);
	}

	SetForegroundWindow();
	tracker->TrackPopupMenu(0, point.x, point.y, this);
	PostMessage(WM_NULL, 0, 0);
}

LRESULT CmainDlg::onCreateRingingDlg(WPARAM wParam, LPARAM lParam)
{
	pjsua_call_id call_id = wParam;
	pjsua_call_info call_info;

	if (pjsua_var.state != PJSUA_STATE_RUNNING || pjsua_call_get_info(call_id, &call_info) != PJ_SUCCESS) {
		int count = ringinDlgs.GetCount();
		if (!count) {
			PlayerStop();
		}
		return  0;
	}

	call_user_data* user_data = (call_user_data*)pjsua_call_get_user_data(call_info.id);
	if (!user_data) {
		return  0;
	}

	user_data->CS.Lock();

	RinginDlg* ringinDlg = new RinginDlg(this);

	ringinDlg->remoteHasVideo = call_info.rem_vid_cnt;
#ifdef _GLOBAL_VIDEO
	if (call_info.rem_vid_cnt) {
		((CButton*)ringinDlg->GetDlgItem(IDC_VIDEO))->EnableWindow(TRUE);
	}
#endif
	ringinDlg->SetCallId(call_info.id);
	SIPURI sipuri;
	CStringW rab;
	CString str;
	CString info;

	if (!user_data->callerID.IsEmpty()) {
		info = user_data->callerID;
	}
	else {
		info = MSIP::PjToStr(&call_info.remote_info, TRUE);
	}

	MSIP::ParseSIPURI(info, &sipuri);

	CString name;
	name = pageContacts->GetNameByNumber(!sipuri.user.IsEmpty() ? sipuri.user : sipuri.domain);
	if (!name.IsEmpty()) {
		if (!sipuri.name.IsEmpty() && name != sipuri.name) {
			name.Format(_T("%s %s"), sipuri.name, name);
			name.Trim();
		}
	}
	if (name.IsEmpty()) {
		name = user_data->name;
	}
	if (name.IsEmpty()) {
		name = !sipuri.name.IsEmpty() ? sipuri.name : (!sipuri.user.IsEmpty() ? sipuri.user : sipuri.domain);
	}
	ringinDlg->GetDlgItem(IDC_CALLER_NAME)->SetWindowText(name);
	ringinDlg->GetDlgItem(IDC_RINGIN_NAME_BLIND)->SetWindowText(name);
	str.Empty();

	info = (!sipuri.user.IsEmpty() ? sipuri.user + _T("@") : _T("")) + sipuri.domain;
	if (!sipuri.name.IsEmpty() && sipuri.name != name) {
		info = sipuri.name + _T(" <") + info + _T(">");
	}
	str.AppendFormat(_T("%s\r\n"), info);
	if (user_data && !user_data->userAgent.IsEmpty()) {
		str.AppendFormat(_T("%s\r\n"), user_data->userAgent);
	}
	str.Append(_T("\r\n"));
	info = MSIP::PjToStr(&call_info.local_info, TRUE);
	MSIP::ParseSIPURI(info, &sipuri);
	info = (!sipuri.user.IsEmpty() ? sipuri.user + _T("@") : _T("")) + sipuri.domain;
	str.AppendFormat(_T("%s: %s\r\n"), Translate(_T("To")), info);

	if (user_data && !user_data->diversion.IsEmpty()) {
		str.AppendFormat(_T("%s: %s\r\n"), Translate(_T("Diversion")), user_data->diversion);
	}
	if (str == name) {
		str.Empty();
	}
	if (!str.IsEmpty()) {
		ringinDlg->GetDlgItem(IDC_CALLER_ADDR)->SetWindowText(str);
	}
	else {
		ringinDlg->GetDlgItem(IDC_CALLER_ADDR)->EnableWindow(FALSE);
	}
	ringinDlgs.Add(ringinDlg);
	if (!accountSettings.bringToFrontOnIncoming) {
		if (GetForegroundWindow()->GetTopLevelParent() != this) {
			BaloonPopup(Translate(_T("Incoming Call")), name, NIIF_INFO);
		}
	}
	user_data->CS.Unlock();
	return 0;
}

LRESULT CmainDlg::onRefreshLevels(WPARAM wParam, LPARAM lParam)
{
	pageDialer->OnHScroll(0, 0, NULL);
	return 0;
}

LRESULT CmainDlg::onPager(WPARAM wParam, LPARAM lParam)
{
	CString *number = (CString *)wParam;
	CString *message = (CString *)lParam;
	bool doNotShowMessagesWindow = MACRO_SILENT && !mainDlg->IsWindowVisible();
	if (doNotShowMessagesWindow) {
		newMessages = true;
	}
	MessagesContact* messagesContact = messagesDlg->AddTab(*number,
		FALSE, NULL, NULL,
		doNotShowMessagesWindow
	);
	if (messagesContact) {
		messagesDlg->AddMessage(messagesContact, *message, MSIP_MESSAGE_TYPE_REMOTE);
		onPlayerPlay(MSIP_SOUND_MESSAGE_IN, 0);
	}
	delete number;
	delete message;
	return 0;
}

LRESULT CmainDlg::onPagerStatus(WPARAM wParam, LPARAM lParam)
{
	CString *number = (CString *)wParam;
	CString *message = (CString *)lParam;
	bool doNotShowMessagesWindow = MACRO_SILENT && !mainDlg->IsWindowVisible();
	MessagesContact* messagesContact = mainDlg->messagesDlg->AddTab(*number,
		FALSE, NULL, NULL,
		doNotShowMessagesWindow);
	if (messagesContact) {
		mainDlg->messagesDlg->AddMessage(messagesContact, *message);
	}
	delete number;
	delete message;
	return 0;
}

LRESULT CmainDlg::OnNetworkChange(WPARAM wParam, LPARAM lParam)
{
	if (ipChangeBusy) {
		ipChangeDelayed = true;
	}
	else {
		if (pjsua_var.state == PJSUA_STATE_RUNNING) {
			bool confirmed = true;
			if (pjsua_acc_is_valid(account)) {
				if (!MSIP::IsConnectedToInternet()) {
					pjsua_acc_info info;
					pjsua_acc_get_info(account, &info);
					SIPURI accURI;
					MSIP::ParseSIPURI(MSIP::PjToStr(&info.acc_uri, TRUE), &accURI);
					CString url;
					url.Format(_T("http://%s"), accURI.domain);
					if (!InternetCheckConnection(url, FLAG_ICC_FORCE_CONNECTION, 0)) {
						confirmed = false;
						//!!PJ_LOG(3, (THIS_FILENAME, "NETWORK CHANGED, not confirmed %ld", GetLastError()));
					}
				}
			}
			if (confirmed) {
				//!!PJ_LOG(3, (THIS_FILENAME, "NETWORK CHANGED, update transports, accounts and calls"));
				MSIP::PortKnock();
				pjsua_ip_change_param param;
				pjsua_ip_change_param_default(&param);
				ipChangeBusy = true;
				pjsua_handle_ip_change(&param);
				pjsua_acc_id ids[PJSUA_MAX_ACC];
				unsigned count = PJSUA_MAX_ACC;
				if (pjsua_enum_accs(ids, &count) == PJ_SUCCESS) {
					for (unsigned i = 0; i < count; i++) {
						pjsua_acc_info info;
						pjsua_acc_get_info(ids[i], &info);
						pjsua_acc_config acc_cfg;
						pjsua_acc_config_default(&acc_cfg);
						pj_pool_t *tmp_pool = pjsua_pool_create("tmp-msip", 1000, 1000);
						pjsua_acc_get_config(ids[i], tmp_pool, &acc_cfg);
						pj_pool_release(tmp_pool);
						if (!acc_cfg.allow_contact_rewrite && info.has_registration) {
							ipChangeBusy = true;
							pjsua_acc_update_contact_on_ip_change(&pjsua_var.acc[ids[i]]);
						}
					}
				}
				}
		}
	}
	return TRUE;
}

LRESULT CmainDlg::OnPowerBroadcast(WPARAM wParam, LPARAM lParam)
{
	if (wParam == PBT_APMRESUMEAUTOMATIC) {
		PJCreate();
		PJAccountAdd();
	}
	else if (wParam == PBT_APMSUSPEND) {
		PJDestroy();
	}
	return TRUE;
}

LRESULT CmainDlg::OnAccount(WPARAM wParam, LPARAM lParam)
{
	if (!accountDlg) {
		accountDlg = new AccountDlg(this);
	}
	else {
		accountDlg->SetForegroundWindow();
	}
	if (accountDlg) {
		accountDlg->Load(accountSettings.accountId ? accountSettings.accountId : -1);
		if (wParam && accountDlg) {
			CEdit *edit = (CEdit*)accountDlg->GetDlgItem(IDC_EDIT_PASSWORD);
			if (edit) {
				edit->SetFocus();
				int nLength = edit->GetWindowTextLength();
				edit->SetSel(nLength, nLength);
			}
		}
	}
	return 0;
}

void CmainDlg::OnTimerProgress()
{
}

void CmainDlg::OnTimerCall()
{
	pjsua_call_id call_id;
	int duration = messagesDlg->GetCallDuration(&call_id);
	if (duration != -1) {
		CString str;
		unsigned icon = IDI_ACTIVE;
		if (call_id != PJSUA_INVALID_ID) {
			int holdFrom = -1;
			if (pjsua_var.state == PJSUA_STATE_RUNNING) {
				call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
				if (user_data) {
					user_data->CS.Lock();
					holdFrom = user_data->holdFrom;
					user_data->CS.Unlock();
				}
			}
			if (holdFrom != -1) {
				icon = IDI_HOLD;
				str.Format(_T("%s %s / %s"), Translate(_T("Hold")), MSIP::GetDuration(duration - holdFrom, true), MSIP::GetDuration(duration, true));
			}
			else {
				str.Format(_T("%s %s"), Translate(_T("Connected")), MSIP::GetDuration(duration, true));
			}
		}
		else {
			str.Format(_T("%s (%d)"), Translate(_T("Connected")), duration);
		}
		if (call_id != PJSUA_INVALID_ID && icon != IDI_HOLD) {
			call_user_data *user_data;
			if (pjsua_var.state == PJSUA_STATE_RUNNING) {
				user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
			}
			else {
				user_data = NULL;
			}
			if (user_data) {
				user_data->CS.Lock();
				if (user_data->srtp == MSIP_SRTP) {
					icon = IDI_ACTIVE_SECURE;
				}
				float MOS;
				if (duration > 0 && user_data && msip_call_statistics(user_data, &MOS)) {
					if (MOS <= 2) {
						icon = (icon == IDI_ACTIVE_SECURE ? IDI_ACTIVE_SECURE_RED : IDI_ACTIVE_RED);
					}
					else if (MOS <= 3) {
						icon = (icon == IDI_ACTIVE_SECURE ? IDI_ACTIVE_SECURE_YELLOW : IDI_ACTIVE_YELLOW);
					}
				}
				user_data->CS.Unlock();
			}
		}
		UpdateWindowText(str, icon);
	}
	else {
		KillTimer(IDT_TIMER_CALL);
	}
}

void CmainDlg::OnTimer(UINT_PTR TimerVal)
{
	if (TimerVal == IDT_TIMER_AUTOANSWER) {
		KillTimer(IDT_TIMER_AUTOANSWER);
		if (autoAnswerTimerCallId != PJSUA_INVALID_ID) {
			AutoAnswer(autoAnswerTimerCallId);
			autoAnswerTimerCallId = PJSUA_INVALID_ID;
		}
	}
	else if (TimerVal == IDT_TIMER_FORWARDING) {
		KillTimer(IDT_TIMER_FORWARDING);
		if (forwardingTimerCallId != PJSUA_INVALID_ID) {
			messagesDlg->CallAction(MSIP_ACTION_FORWARD, _T(""), forwardingTimerCallId);
			forwardingTimerCallId = PJSUA_INVALID_ID;
		}
	}
	else if (TimerVal == IDT_TIMER_SWITCH_DEVICES) {
		KillTimer(IDT_TIMER_SWITCH_DEVICES);
		if (pjsua_var.state == PJSUA_STATE_RUNNING) {
			PJ_LOG(3, (THIS_FILENAME, "Execute refresh devices"));
			bool snd_is_active = pjsua_snd_is_active();
			bool is_ring;
			if (snd_is_active) {
				int in, out;
				if (pjsua_get_snd_dev(&in, &out) == PJ_SUCCESS) {
					is_ring = (out == msip_audio_ring);
				}
				else {
					is_ring = false;
				}
				pjsua_set_null_snd_dev();
			}
			pjmedia_aud_dev_refresh();
			UpdateSoundDevicesIds();
			if (snd_is_active) {
				msip_set_sound_device(is_ring ? msip_audio_ring : msip_audio_output, true);
			}
#ifdef _GLOBAL_VIDEO
			pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();
			if (vid_subsys->init_count) {
				pjmedia_vid_dev_refresh();
			}
#endif
			if (accountSettings.headsetSupport) {
				Hid::OpenDevice();
			}
		}
	}
	else if (TimerVal == IDT_TIMER_SAVE) {
		KillTimer(IDT_TIMER_SAVE);
		accountSettings.SettingsSave();
	}
	else if (TimerVal == IDT_TIMER_DIRECTORY) {
		UsersDirectoryLoad();
	}
	else if (TimerVal == IDT_TIMER_PROGRESS) {
		OnTimerProgress();
	}
	else if (TimerVal == IDT_TIMER_CALL) {
		OnTimerCall();
	}
	else
					if (TimerVal == IDT_TIMER_IDLE) {
				if (pjsua_var.state == PJSUA_STATE_RUNNING && m_PresenceStatus != PJRPID_ACTIVITY_BUSY) {
					//--
					LASTINPUTINFO lii;
					lii.cbSize = sizeof(LASTINPUTINFO);
					if (GetLastInputInfo(&lii)) {
						if (lii.dwTime != m_lastInputTime) {
							m_lastInputTime = lii.dwTime;
							m_idleCounter = 0;
							if (m_PresenceStatus == PJRPID_ACTIVITY_AWAY) {
								PublishStatus();
							}
						}
						else {
							m_idleCounter++;
							if (m_idleCounter == 120) {
								PublishStatus(false);
							}
						}
					}
					//--
				}
			}
			else
				if (TimerVal = IDT_TIMER_TONE) {
					onPlayerPlay(MSIP_SOUND_RINGING, 0);
				}
}

void CmainDlg::PJCreate()
{
	while (!pj_ready) {
		PJCreateRaw();
		if (pj_ready) {
			break;
		}
		UpdateWindowText();
		if (AfxMessageBox(Translate(_T("Unable to initialize network sockets.")), MB_RETRYCANCEL | MB_ICONEXCLAMATION) != IDRETRY) {
			OnMenuSettings();
			break;
		}
	}
}

void CmainDlg::PJCreateRaw()
{
	player_eof_data = NULL;
	autoAnswerTimerCallId = PJSUA_INVALID_ID;
	autoAnswerPlayCallId = PJSUA_INVALID_ID;
	ipChangeBusy = false;
	ipChangeDelayed = false;
	forwardingTimerCallId = PJSUA_INVALID_ID;

	isSubscribed = false;
	if (accountSettings.audioCodecs.IsEmpty())
	{
		accountSettings.audioCodecs = _T(_GLOBAL_CODECS_ENABLED);
	}

	// check updates
	if (accountSettings.updatesInterval != _T("never"))
	{
		CTime t = CTime::GetCurrentTime();
		time_t time = t.GetTime();
		int days;
		if (accountSettings.updatesInterval == _T("daily"))
		{
			days = 1;
		}
		else if (accountSettings.updatesInterval == _T("monthly"))
		{
			days = 30;
		}
		else if (accountSettings.updatesInterval == _T("quarterly"))
		{
			days = 90;
		}
		else
		{
			days = 7;
		}
		if (accountSettings.updatesInterval == _T("always") || accountSettings.checkUpdatesTime + days * 86400 < time) {
			CheckUpdates();
			accountSettings.checkUpdatesTime = time;
			accountSettings.SettingsSave();
		}
	}

	// pj create
	pj_status_t status;
	pjsua_config         ua_cfg;
	pjsua_media_config   media_cfg;
	pjsua_transport_config cfg;

	// Must create pjsua before anything else!
	status = pjsua_create();
	if (status != PJ_SUCCESS) {
		return;
	}

	// Initialize configs with default settings.
	pjsua_config_default(&ua_cfg);
	pjsua_media_config_default(&media_cfg);

	CString userAgent;
	if (accountSettings.userAgent.IsEmpty()) {
		userAgent.Format(_T("%s/%s"), _T(_GLOBAL_NAME_NICE), _T(_GLOBAL_VERSION));
		ua_cfg.user_agent = MSIP::StrToPjStr(userAgent);
	}
	else {
		ua_cfg.user_agent = MSIP::StrToPjStr(accountSettings.userAgent);
	}

	ua_cfg.cb.on_reg_started2 = &on_reg_started2;
	ua_cfg.cb.on_reg_state2 = &on_reg_state2;
	ua_cfg.cb.on_call_state = &on_call_state;
	ua_cfg.cb.on_dtmf_digit = &on_dtmf_digit;
	ua_cfg.cb.on_call_tsx_state = &on_call_tsx_state;

	ua_cfg.cb.on_call_redirected = &on_call_redirected;

	ua_cfg.cb.on_call_media_state = &on_call_media_state;
	ua_cfg.cb.on_call_media_event = &on_call_media_event;
	ua_cfg.cb.on_incoming_call = &on_incoming_call;
	ua_cfg.cb.on_nat_detect = &on_nat_detect;
	ua_cfg.cb.on_buddy_state = &on_buddy_state;
	ua_cfg.cb.on_pager2 = &on_pager2;
	ua_cfg.cb.on_pager_status2 = &on_pager_status2;
	ua_cfg.cb.on_call_transfer_request2 = &on_call_transfer_request2;
	ua_cfg.cb.on_call_transfer_status = &on_call_transfer_status;

	ua_cfg.cb.on_call_replace_request2 = &on_call_replace_request2;
	ua_cfg.cb.on_call_replaced = &on_call_replaced;

	ua_cfg.cb.on_mwi_info = &on_mwi_info;

	ua_cfg.cb.on_ip_change_progress = &on_ip_change_progress;
	
	ua_cfg.srtp_secure_signaling = 0;

	/*
	TODO: accountSettings.account: public_addr
	*/

	if (accountSettings.enableSTUN && !accountSettings.stun.IsEmpty()) {
		int pos = 0;
		int i = 0;
		while (i < 8) {
			CString resToken = accountSettings.stun.Tokenize(_T(";,"), pos);
			if (pos == -1) {
				break;
			}
			resToken.Trim();
			if (!resToken.IsEmpty()) {
				ua_cfg.stun_srv[i] = MSIP::StrToPjStr(resToken);
				i++;
			}
		}
		ua_cfg.stun_srv_cnt = i;
	}

	media_cfg.enable_ice = PJ_FALSE;
	
	media_cfg.no_vad = accountSettings.vad ? PJ_FALSE : PJ_TRUE;
	media_cfg.ec_tail_len = accountSettings.ec && !accountSettings.opusStereo ? 20 : 0;

	int maxClockRate = 8000;
	int maxChannelCount = 1;
	int curPos = 0;
	CString resToken = accountSettings.audioCodecs.Tokenize(_T(" "), curPos);
	while (!resToken.IsEmpty()) {
		int pos = 0;
		bool isOpus = resToken.Tokenize(_T("/"), pos) == _T("opus");
		int clockRate = 0;
		if (isOpus) {
			clockRate = 24000;
		}
		else {
			if (pos != -1) {
				clockRate = _wtoi(resToken.Tokenize(_T("/"), pos));
			}
		}
		if (clockRate > maxClockRate) {
			maxClockRate = clockRate;
		}
		if (!accountSettings.ec && !isOpus) {
			if (pos != -1) {
				BYTE channelCount = resToken.Tokenize(_T("/"), pos) == _T("2") ? 2 : 1;
				if (channelCount > maxChannelCount) {
					maxChannelCount = channelCount;
				}
			}
		}
		resToken = accountSettings.audioCodecs.Tokenize(_T(" "), curPos);
	}
	media_cfg.clock_rate = maxClockRate;
	if (accountSettings.opusStereo) {
		media_cfg.channel_count = 2;
	}
	else {
		media_cfg.channel_count = maxChannelCount;
	}

	if (accountSettings.dnsSrv && !accountSettings.dnsSrvNs.IsEmpty()) {
		int pos = 0;
		int i = 0;
		while (i < 4) {
			CString resToken = accountSettings.dnsSrvNs.Tokenize(_T(";,"), pos);
			if (pos == -1) {
				break;
			}
			resToken.Trim();
			if (!resToken.IsEmpty()) {
				ua_cfg.nameserver[i] = MSIP::StrToPjStr(resToken);
				i++;
			}
		}
		ua_cfg.nameserver_count = i;
	}

	// Initialize pjsua
	if (accountSettings.enableLog) {
		pjsua_logging_config log_cfg;
		pjsua_logging_config_default(&log_cfg);
		log_cfg.decor |= PJ_LOG_HAS_CR;
		char *buf = MSIP::WideCharToPjStr(accountSettings.logFile);
		log_cfg.log_filename = pj_str(buf);
		status = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
		delete buf;
	}
	else {
		status = pjsua_init(&ua_cfg, NULL, &media_cfg);
	}

	if (status != PJ_SUCCESS) {
		pjsua_destroy();
		return;
	}

	// Start pjsua
	status = pjsua_start();

	if (status != PJ_SUCCESS) {
		pjsua_destroy();
		return;
	}

	pjsip_cfg()->endpt.disable_rport = accountSettings.rport ? PJ_FALSE : PJ_TRUE;
	
	pj_ready = true;

	// Set snd devices
	UpdateSoundDevicesIds();

	PJAudioCodecs();
#ifdef _GLOBAL_VIDEO
	PJVideoCodecs();
#endif

	// Create transport
	PJ_LOG(3, (THIS_FILENAME, "Create transport"));
	transport_udp_local = -1;
	transport_udp = -1;
	transport_tcp = -1;
	transport_tls = -1;

	pjsua_transport_config_default(&cfg);
	if (accountSettings.sourcePort) {
		cfg.port = accountSettings.sourcePort;
		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_udp);
		if (status != PJ_SUCCESS) {
			cfg.port = 0;
			pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_udp);
		}
		if (MACRO_ENABLE_LOCAL_ACCOUNT) {
			if (accountSettings.sourcePort == 5060) {
				transport_udp_local = transport_udp;
			}
			else {
				cfg.port = 5060;
				status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_udp_local);
				if (status != PJ_SUCCESS) {
					transport_udp_local = transport_udp;
				}
			}
		}
	}
	else {
		if (MACRO_ENABLE_LOCAL_ACCOUNT) {
			cfg.port = 5060;
			status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_udp_local);
			if (status != PJ_SUCCESS) {
				transport_udp_local = -1;
			}
		}
		cfg.port = 0;
		pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_udp);
		if (transport_udp_local == -1) {
			transport_udp_local = transport_udp;
		}
	}

		cfg.port = MACRO_ENABLE_LOCAL_ACCOUNT ? 5060 : 0;
		status = pjsua_transport_create(PJSIP_TRANSPORT_TCP, &cfg, &transport_tcp);
		if (status != PJ_SUCCESS && cfg.port) {
			cfg.port = 0;
			pjsua_transport_create(PJSIP_TRANSPORT_TCP, &cfg, &transport_tcp);
		}
		cfg.port = MACRO_ENABLE_LOCAL_ACCOUNT ? 5061 : 0;
		status = pjsua_transport_create(PJSIP_TRANSPORT_TLS, &cfg, &transport_tls);
		if (status != PJ_SUCCESS && cfg.port) {
			cfg.port = 0;
			pjsua_transport_create(PJSIP_TRANSPORT_TLS, &cfg, &transport_tls);
		}

	if (accountSettings.usersDirectory.Find(_T("%s")) == -1 && accountSettings.usersDirectory.Find(_T("{")) == -1) {
		UsersDirectoryLoad();
	}

	SetTimer(IDT_TIMER_IDLE, 5000, NULL);

	account = PJSUA_INVALID_ID;
	account_local = PJSUA_INVALID_ID;

	PJAccountAddLocal();

	if (accountSettings.headsetSupport) {
		Hid::OpenDevice();
	}
}

void CmainDlg::PJAudioCodecs()
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	//Set aud codecs prio
	PJ_LOG(3, (THIS_FILENAME, "Set audio codecs"));
	if (accountSettings.audioCodecs.GetLength())
	{
		// add unknown new codecs to the list
		unsigned count = PJMEDIA_CODEC_MGR_MAX_CODECS;
		pjsua_codec_info codec_info[PJMEDIA_CODEC_MGR_MAX_CODECS];
		if (pjsua_enum_codecs(codec_info, &count) == PJ_SUCCESS) {
			for (unsigned i = 0; i < count; i++) {
				pjsua_codec_set_priority(&codec_info[i].codec_id, PJMEDIA_CODEC_PRIO_DISABLED);
				CString rab = MSIP::PjToStr(&codec_info[i].codec_id);
				if (!audioCodecList.Find(rab)) {
					audioCodecList.AddTail(rab);
					rab.Append(_T("~"));
					audioCodecList.AddTail(rab);
			}
		}
		}
		// remove unsupported codecs from list
		POSITION pos = audioCodecList.GetHeadPosition();
		while (pos) {
			POSITION posKey = pos;
			CString key = audioCodecList.GetNext(pos);
			POSITION posValue = pos;
			CString value = audioCodecList.GetNext(pos);
			pj_str_t codec_id = MSIP::StrToPjStr(key);
			pjmedia_codec_param param;
			if (pjsua_codec_get_param(&codec_id, &param) != PJ_SUCCESS) {
				audioCodecList.RemoveAt(posKey);
				audioCodecList.RemoveAt(posValue);
			}
		};

		int curPos = 0;
		int i = PJMEDIA_CODEC_PRIO_NORMAL;
		CString resToken = accountSettings.audioCodecs.Tokenize(_T(" "), curPos);
		while (!resToken.IsEmpty()) {
			int pos = resToken.Find('/', 0);
			if (pos > 0 && resToken.Find('/', pos + 1) > 0) {
				pj_str_t codec_id = MSIP::StrToPjStr(resToken);
				pjmedia_codec_param param;
				if (pjsua_codec_get_param(&codec_id, &param) == PJ_SUCCESS) {
					if (accountSettings.opusStereo) {
						if (pj_strcmp2(&codec_id, "opus/48000/2") == 0) {
							for (int j = 0; j < param.setting.dec_fmtp.cnt; j++) {
								if (pj_strcmp2(&param.setting.dec_fmtp.param[j].name, "maxaveragebitrate") == 0) {
									param.setting.dec_fmtp.param[j].val = pj_str("96000");
								}
							}
							param.info.avg_bps = 96000;
							param.info.max_bps = 96000;
							param.setting.dec_fmtp.param[param.setting.dec_fmtp.cnt].name = pj_str("stereo");
							param.setting.dec_fmtp.param[param.setting.dec_fmtp.cnt].val = pj_str("1");
							param.setting.dec_fmtp.cnt++;
							pjsua_codec_set_param(&codec_id, &param);
						}
					}
					pjsua_codec_set_priority(&codec_id, i);
		}
}
			resToken = accountSettings.audioCodecs.Tokenize(_T(" "), curPos);
			i--;
		}
	}
}

#ifdef _GLOBAL_VIDEO
void CmainDlg::PJVideoCodecs()
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	//Set vid codecs prio
	PJ_LOG(3, (THIS_FILENAME, "Set video codecs"));
	if (accountSettings.videoCodec.GetLength())
	{
		pj_str_t codec_id = MSIP::StrToPjStr(accountSettings.videoCodec);
		pjsua_vid_codec_set_priority(&codec_id, 255);
	}
	int bitrate;
	if (!accountSettings.videoH264) {
		pjsua_vid_codec_set_priority(&pj_str("H264/99"), 0);
	}
	else
	{
		const pj_str_t codec_id = { "H264/99", 7 };
		pjmedia_vid_codec_param param;
		pjsua_vid_codec_get_param(&codec_id, &param);
		if (accountSettings.videoBitrate) {
			bitrate = 1000 * accountSettings.videoBitrate;
			param.enc_fmt.det.vid.avg_bps = bitrate;
			param.enc_fmt.det.vid.max_bps = bitrate;
		}
		pjsua_vid_codec_set_param(&codec_id, &param);
	}
	if (!accountSettings.videoH263) {
		pjsua_vid_codec_set_priority(&pj_str("H263-1998/98"), 0);
	}
	else {
		if (accountSettings.videoBitrate) {
			bitrate = 1000 * accountSettings.videoBitrate;
			const pj_str_t codec_id = { "H263-1998/98", 12 };
			pjmedia_vid_codec_param param;
			pjsua_vid_codec_get_param(&codec_id, &param);
			param.enc_fmt.det.vid.avg_bps = bitrate;
			param.enc_fmt.det.vid.max_bps = bitrate;
			pjsua_vid_codec_set_param(&codec_id, &param);
		}
	}
	if (!accountSettings.videoVP8) {
		pjsua_vid_codec_set_priority(&pj_str("VP8/100"), 0);
	}
	else {
		if (accountSettings.videoBitrate) {
			bitrate = 1000 * accountSettings.videoBitrate;
			const pj_str_t codec_id = { "VP8/100", 7 };
			pjmedia_vid_codec_param param;
			pjsua_vid_codec_get_param(&codec_id, &param);
			param.enc_fmt.det.vid.avg_bps = bitrate;
			param.enc_fmt.det.vid.max_bps = bitrate;
			pjsua_vid_codec_set_param(&codec_id, &param);
		}
	}
	if (!accountSettings.videoVP9) {
		pjsua_vid_codec_set_priority(&pj_str("VP9/101"), 0);
	}
	else {
		if (accountSettings.videoBitrate) {
			bitrate = 1000 * accountSettings.videoBitrate;
			const pj_str_t codec_id = { "VP9/101", 7 };
			pjmedia_vid_codec_param param;
			pjsua_vid_codec_get_param(&codec_id, &param);
			param.enc_fmt.det.vid.avg_bps = bitrate;
			param.enc_fmt.det.vid.max_bps = bitrate;
			pjsua_vid_codec_set_param(&codec_id, &param);
		}
	}
}
#endif

void CmainDlg::UpdateSoundDevicesIds()
{
	msip_audio_input = -1;
	msip_audio_output = -2;
	msip_audio_ring = -2;
	CString audioOutputDevice = accountSettings.audioOutputDevice;
	CString audioInputDevice = accountSettings.audioInputDevice;
	unsigned count = PJMEDIA_AUD_MAX_DEVS;
	pjmedia_aud_dev_info aud_dev_info[PJMEDIA_AUD_MAX_DEVS];
	pjsua_enum_aud_devs(aud_dev_info, &count);
	for (unsigned i = 0; i < count; i++)
	{
		CString audDevName = MSIP::PjStrToWideChar(aud_dev_info[i].name);
		if (aud_dev_info[i].input_count && !audioInputDevice.Compare(audDevName)) {
			msip_audio_input = i;
		}
		if (aud_dev_info[i].output_count) {
			if (!audioOutputDevice.Compare(audDevName)) {
				msip_audio_output = i;
			}
			if (!accountSettings.audioRingDevice.Compare(audDevName)) {
				msip_audio_ring = i;
			}
		}
	}
}

void CmainDlg::PJDestroy(bool exit)
{
	KillTimer(IDT_TIMER_IDLE);
	KillTimer(IDT_TIMER_CALL);

	usersDirectoryLoaded = false;
	shortcutsURLLoaded = false;
	if (pj_ready) {
		if (accountSettings.headsetSupport) {
			Hid::CloseDevice(true);
		}
		Unsubscribe();
		call_deinit_tonegen(-1);

		toneCalls.RemoveAll();

		if (IsWindow(m_hWnd)) {
			KillTimer(IDT_TIMER_TONE);
		}

		PlayerStop();

		if (player_eof_data) {
			pj_pool_release(player_eof_data->pool);
			player_eof_data = NULL;
		}

		if (accountSettings.accountId) {
			PJAccountDelete(false, exit);
		}

		pj_ready = false;

		//if (transport_udp_local!=PJSUA_INVALID_ID && transport_udp_local!=transport_udp) {
		//	pjsua_transport_close(transport_udp_local,PJ_TRUE);
		//}
		if (transport_udp != PJSUA_INVALID_ID) {
			//pjsua_transport_close(transport_udp,PJ_TRUE);
		}
		//if (transport_tcp!=PJSUA_INVALID_ID) {
		//	pjsua_transport_close(transport_tcp,PJ_TRUE);
		//}
		//if (transport_tls!=PJSUA_INVALID_ID) {
		//	pjsua_transport_close(transport_tls,PJ_TRUE);
		//}
		pjsua_destroy();
		pjsua_destroy();
	}
	transport_udp_local = -1;
	transport_udp = -1;
	transport_tcp = -1;
	transport_tls = -1;
}

void CmainDlg::PJAccountConfig(pjsua_acc_config *acc_cfg, Account *account)
{
	bool isLocal = (account == &accountSettings.accountLocal);
	pjsua_acc_config_default(acc_cfg);
	// global
	acc_cfg->ka_interval = account->keepAlive;
#ifdef _GLOBAL_VIDEO
	acc_cfg->vid_in_auto_show = PJ_TRUE;
	acc_cfg->vid_out_auto_transmit = PJ_TRUE;
	acc_cfg->vid_cap_dev = VideoCaptureDeviceId();
	acc_cfg->vid_wnd_flags = PJMEDIA_VID_DEV_WND_BORDER | PJMEDIA_VID_DEV_WND_RESIZABLE;
#endif

	if (accountSettings.rtpPortMin > 0) {
		acc_cfg->rtp_cfg.port = accountSettings.rtpPortMin;
		if (accountSettings.rtpPortMax > accountSettings.rtpPortMin) {
			acc_cfg->rtp_cfg.port_range = accountSettings.rtpPortMax - accountSettings.rtpPortMin;
		}
	}
	// account
	if (account->disableSessionTimer) {
		acc_cfg->use_timer = PJSUA_SIP_TIMER_INACTIVE;
	}

	acc_cfg->reg_timeout = account->registerRefresh;

	if (account->srtp == _T("optional")) {
		acc_cfg->use_srtp = PJMEDIA_SRTP_OPTIONAL;
	}
	else if (account->srtp == _T("mandatory")) {
		acc_cfg->use_srtp = PJMEDIA_SRTP_MANDATORY;
	}
	else {
		acc_cfg->use_srtp = PJMEDIA_SRTP_DISABLED;
	}
	if (!accountSettings.enableSTUN || accountSettings.stun.IsEmpty()) {
		acc_cfg->rtp_cfg.public_addr = MSIP::StrToPjStr(get_public_addr(account));
	}
	acc_cfg->ice_cfg_use = PJSUA_ICE_CONFIG_USE_CUSTOM;
	acc_cfg->ice_cfg.enable_ice = account->ice ? PJ_TRUE : PJ_FALSE;
	acc_cfg->allow_via_rewrite = account->allowRewrite ? PJ_TRUE : PJ_FALSE;
	acc_cfg->allow_sdp_nat_rewrite = acc_cfg->allow_via_rewrite;
	acc_cfg->allow_contact_rewrite = acc_cfg->allow_via_rewrite ? 2 : PJ_FALSE;
	acc_cfg->contact_rewrite_method = PJSUA_CONTACT_REWRITE_UNREGISTER; //when enabled, one way audio when SIP ALG + Allow IP rewrite enabled

	acc_cfg->publish_enabled = account->publish ? PJ_TRUE : PJ_FALSE;

	if (!account->voicemailNumber.IsEmpty()) {
		acc_cfg->mwi_enabled = PJ_TRUE;
	}

	if (account->transport == _T("udp") && transport_udp != -1) {
		acc_cfg->transport_id = transport_udp;
	}
	else if (account->transport == _T("tcp") && transport_tcp != -1) {
		if (isLocal) {
			acc_cfg->transport_id = transport_tcp;
		}
	}
	else if (account->transport == _T("tls") && transport_tls != -1) {
		if (isLocal) {
			acc_cfg->transport_id = transport_tls;
		}
	}

	acc_cfg->cred_count = 1;
	acc_cfg->cred_info[0].username = MSIP::StrToPjStr(!account->authID.IsEmpty() ? account->authID : (isLocal ? account->username : get_account_username()));
	acc_cfg->cred_info[0].realm = pj_str("*");
	acc_cfg->cred_info[0].scheme = pj_str("Digest");
	if (!account->digest.IsEmpty()) {
		acc_cfg->cred_info[0].data_type = PJSIP_CRED_DATA_DIGEST;
		acc_cfg->cred_info[0].data = MSIP::StrToPjStr(account->digest);
	}
	else {
		acc_cfg->cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		acc_cfg->cred_info[0].data = MSIP::StrToPjStr((isLocal ? account->password : get_account_password()));
	}

	CStringList proxies;
	get_account_proxy(account, proxies);
	acc_cfg->proxy_cnt = proxies.GetCount();
	POSITION pos = proxies.GetHeadPosition();
	int i = 0;
	while (pos) {
		CString proxy = proxies.GetNext(pos);
		proxy.Format(_T("sip:%s"), proxy);
		if (account->port > 0) {
			proxy.AppendFormat(_T(":%d"), account->port);
		}
		AddTransportSuffix(proxy, account);
		acc_cfg->proxy[i] = MSIP::StrToPjStr(proxy);
		i++;
	}
	if (isLocal) {
		acc_cfg->sip_stun_use = PJSUA_STUN_USE_DISABLED;
		acc_cfg->media_stun_use = PJSUA_STUN_USE_DISABLED;
	}
}

/**
 * Add account is not exists.
 */
void CmainDlg::PJAccountAdd()
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING || pjsua_acc_is_valid(account)) {
		return;
	}
	CString str;

	if (!accountSettings.accountId) {
		return;
	}
	if (accountSettings.account.username.IsEmpty()
		) {
		if (!MACRO_SILENT) {
			OnAccount(0, 0);
		}
		return;
	}
	PJAccountAddRaw();
}

void CmainDlg::PJAccountAddRaw()
{
	CString str;

	CString title = _T(_GLOBAL_NAME_NICE);
	CString titleAdder;
	CString usernameLocal;
	usernameLocal = accountSettings.account.username;
	if (!accountSettings.account.label.IsEmpty())
	{
		titleAdder = accountSettings.account.label;
	}
	else if (!accountSettings.account.displayName.IsEmpty())
	{
		titleAdder = accountSettings.account.displayName;
	}
	else if (!usernameLocal.IsEmpty())
	{
		titleAdder = usernameLocal;
	}
	if (!titleAdder.IsEmpty()) {
		title.AppendFormat(_T(" - %s"), titleAdder);
	}
	SetPaneText2(accountSettings.account.username);
	SetWindowText(title);
	pageDialer->SetName();

	pjsua_acc_config acc_cfg;
	PJAccountConfig(&acc_cfg, &accountSettings.account);

	//-- port knocker
	MSIP::PortKnock();

	//--
	bool ok = false;
	pj_status_t status = -1;
	//--
	CString localURI;
	if (!accountSettings.account.displayName.IsEmpty()) {
		localURI = _T("\"") + accountSettings.account.displayName + _T("\" ");
	}
	localURI += GetSIPURI(get_account_username());
	acc_cfg.id = MSIP::StrToPjStr(localURI);
	//--
	if (get_account_server().IsEmpty()) {
		acc_cfg.register_on_acc_add = PJ_FALSE;
	}
	else {
		CString regURI;
		regURI.Format(_T("sip:%s"), get_account_server());
		AddTransportSuffix(regURI,&accountSettings.account);
		acc_cfg.reg_uri = MSIP::StrToPjStr(regURI);
	}	
	//--
	status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &account);
	if (status == PJ_SUCCESS) {
		ok = true;
		if (acc_cfg.register_on_acc_add == PJ_FALSE) {
			Subscribe();
		}
	}
	if (!ok) {
		if (status != -1) {
			MSIP::ShowErrorMessage(status);
		}
		UpdateWindowText(_T(""), IDI_DEFAULT, true);
	}
	PublishStatus(true, acc_cfg.register_on_acc_add);
}

void CmainDlg::PJAccountAddLocal()
{
	if (MACRO_ENABLE_LOCAL_ACCOUNT) {
		pj_status_t status;
		pjsua_acc_config acc_cfg;
		PJAccountConfig(&acc_cfg, &accountSettings.accountLocal);

		CString localURI;
		if (!accountSettings.accountLocal.displayName.IsEmpty()) {
			localURI = _T("\"") + accountSettings.accountLocal.displayName + _T("\" ");
		}
		CString domain;
		if (!accountSettings.accountLocal.domain.IsEmpty()) {
			domain = accountSettings.accountLocal.domain;
		}
		else {
			pjsua_transport_data *t = &pjsua_var.tpdata[0];
			domain = MSIP::PjToStr(&t->local_name.host);
		}
		if (!accountSettings.accountLocal.username.IsEmpty()) {
			localURI.AppendFormat(_T("<sip:%s@%s>"), accountSettings.accountLocal.username, domain);
		}
		else {
			localURI.AppendFormat(_T("<sip:%s>"), domain);
		}

		acc_cfg.id = MSIP::StrToPjStr(localURI);
		acc_cfg.priority--;
		pjsua_acc_add(&acc_cfg, PJ_TRUE, &account_local);
		acc_cfg.priority++;
	}
}

/**
 * Delete account if exists.
 */
void CmainDlg::PJAccountDelete(bool deep, bool exit, CStringA code)
{
	Unsubscribe();
	if (pjsua_acc_is_valid(account)) {
		pjsua_acc_del(account);
		account = PJSUA_INVALID_ID;
	}

}

void CmainDlg::PJAccountDeleteLocal()
{
	if (pjsua_acc_is_valid(account_local)) {
		pjsua_acc_del(account_local);
		account_local = PJSUA_INVALID_ID;
	}
}

void CmainDlg::OnTcnSelchangeTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	CTabCtrl* tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	int nTab = tab->GetCurSel();
	TC_ITEM tci;
	tci.mask = TCIF_PARAM;
	tab->GetItem(nTab, &tci);
	if (tci.lParam > 0) {
		CWnd* pWnd = (CWnd *)tci.lParam;
		if (m_tabPrev != -1) {
			tab->GetItem(m_tabPrev, &tci);
			if (tci.lParam > 0) {
				((CWnd *)tci.lParam)->ShowWindow(SW_HIDE);
			}
		}
		pWnd->ShowWindow(SW_SHOW);
		if (IsWindowVisible()) {
			pWnd->SetFocus();
		}
		if (nTab != accountSettings.activeTab) {
			accountSettings.activeTab = nTab;
			AccountSettingsPendingSave();
		}
		if (pWnd == pageCalls && missed) {
			missed = false;
			UpdateWindowText();
		}
	}
	else {
	}
	*pResult = 0;
}

void CmainDlg::OnTcnSelchangingTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	CTabCtrl* tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	m_tabPrev = tab->GetCurSel();
	*pResult = FALSE;
}

LRESULT CmainDlg::OnUpdateWindowText(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 1) {
		bool show = !messagesDlg->GetCallsCount();
		if (show) {
			CString str;
			str.Format(_T("%s..."), Translate(_T("Connecting")));
			UpdateWindowText(str);
		}
	}
	else {
		UpdateWindowText(_T("-"));
	}
	return TRUE;
}

void CmainDlg::TabFocusSet()
{
	CTabCtrl* tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	int nTab = tab->GetCurSel();
	TC_ITEM tci;
	tci.mask = TCIF_PARAM;
	tab->GetItem(nTab, &tci);
	if (tci.lParam > 0) {
		CWnd* pWnd = (CWnd *)tci.lParam;
		pWnd->SetFocus();
	}
}

void CmainDlg::UpdateWindowText(CString text, int icon, bool afterRegister)
{
	if (text.IsEmpty() && pjsua_var.state == PJSUA_STATE_RUNNING && messagesDlg->GetCallsCount()) {
		return;
	}
	CString str;
	bool showAccountDlg = false;
	bool noReg = false;
	bool isOffline = false;
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		isOffline = true;
	} else if (text.IsEmpty() || text == _T("-")) {
		pjsua_acc_id acc_id = account;
		if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_acc_is_valid(acc_id)) {
			pjsua_acc_info info;
			pjsua_acc_get_info(acc_id, &info);
			str = MSIP::PjToStr(&info.status_text);
			if (str != _T("Default status message")) {
				if (!info.has_registration) {
					icon = IDI_DEFAULT;
					str = Translate(_T("Idle"));
					noReg = true;
				} else if (str == _T("OK")) {
					if (m_PresenceStatus == PJRPID_ACTIVITY_BUSY) {
						icon = IDI_BUSY;
						str = Translate(_T("Do Not Disturb"));
					}
					else {
						if (m_PresenceStatus == PJRPID_ACTIVITY_AWAY) {
							icon = IDI_AWAY;
							str = Translate(_T("Away"));
						}
						else {
							if (accountSettings.account.transport == _T("tls") && transport_tls != -1) {
								icon = IDI_SECURE;
							}
							else {
								icon = IDI_ONLINE;
							}
							str = Translate(_T("Online"));
						}
						if (accountSettings.forwarding == _T("button") && accountSettings.FWD) {
							icon = IDI_FORWARDING;
							str = Translate(_T("Call Forwarding"));
						}
						else {
							if (!accountSettings.singleMode && accountSettings.AC) {
								str.AppendFormat(_T(" (%s)"), Translate(_T("Auto Conference")));
							}
							else if (accountSettings.autoAnswer == _T("button") && accountSettings.AA) {
								str.AppendFormat(_T(" (%s)"), Translate(_T("Auto Answer")));
							}
						}
					}
					if (!dialNumberDelayed.IsEmpty()) {
						DialNumber(dialNumberDelayed);
						dialNumberDelayed = _T("");
					}
				}
				else if (str == _T("In Progress")) {
					str.Format(_T("%s..."), Translate(_T("Connecting")));
				}
				else if (info.status == 401 || info.status == 403) {
					icon = IDI_OFFLINE;
					str = Translate(_T("Incorrect Password"));
					if (afterRegister) {
						if (IsWindowVisible() && !IsIconic()) {
							showAccountDlg = true;
						}
						else {
							BaloonPopup(_T(""), str);
						}
					}
				}
				else {
					if (info.status == 502) {
						str = _T("Connection Failed");
						icon = IDI_OFFLINE;
					}
					str = Translate(str.GetBuffer());
				}
			}
			else {
				str.Format(_T("%s: %d"), Translate(_T("Response Code")), info.status);
			}
		}
		else {
			if (afterRegister) {
				showAccountDlg = true;
			}
			isOffline = true;
		}
	}
	 else {
		str = text;
	}
	if (isOffline) {
		icon = IDI_DEFAULT;
		if (MACRO_ENABLE_LOCAL_ACCOUNT) {
			str = _T(_GLOBAL_NAME_NICE);
		}
		else {
			str = Translate(_T("Offline"));
			icon = IDI_OFFLINE;
		}
	}
#ifdef _GLOABL_ICON_DEFAULT_OFFLINE
	if (icon == IDI_DEFAULT) {
		icon = IDI_OFFLINE;
	}
#endif

	CString* pPaneText = new CString();
	*pPaneText = str;
	PostMessage(UM_SET_PANE_TEXT, NULL, (LPARAM)pPaneText);

	if (icon != -1) {
		HICON hIcon = (HICON)LoadImage(
			AfxGetInstanceHandle(),
			MAKEINTRESOURCE(icon),
			IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
		m_bar.GetStatusBarCtrl().SetIcon(0, hIcon);
		iconStatusbar = icon;

		//--
		tnd.uFlags = NIF_ICON;
		if ((pjsua_var.state == PJSUA_STATE_RUNNING && !pjsua_acc_is_valid(account) && MACRO_ENABLE_LOCAL_ACCOUNT) || ((icon != IDI_DEFAULT || noReg) && icon != IDI_OFFLINE)) {
			if (missed) {
				if (tnd.hIcon != iconMissed) {
					tnd.hIcon = iconMissed;
					Shell_NotifyIcon(NIM_MODIFY, &tnd);
				}
			}
			else {
				if (tnd.hIcon != iconSmall) {
					tnd.hIcon = iconSmall;
					Shell_NotifyIcon(NIM_MODIFY, &tnd);
				}
			}
		}
		else {
			if (tnd.hIcon != iconInactive) {
				tnd.hIcon = iconInactive;
				Shell_NotifyIcon(NIM_MODIFY, &tnd);
			}
		}
		//--
	}
	if (showAccountDlg) {
		PostMessage(UM_ON_ACCOUNT, 1);
	}
}

void CmainDlg::PublishStatus(bool online, bool init)
{
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	bool busy = (accountSettings.denyIncoming == _T("button") && accountSettings.DND);
	pjrpid_activity presenceStatusNew;
	pj_str_t note = pj_str("");
	if (m_PresenceStatus == PJRPID_ACTIVITY_BUSY) {
		if (!busy) {
			presenceStatusNew = PJRPID_ACTIVITY_UNKNOWN;
			note = pj_str("Idle");
		}
	}
	else {
		if (busy) {
			presenceStatusNew = PJRPID_ACTIVITY_BUSY;
			note = pj_str("Busy");
		}
		else {
			presenceStatusNew = online ? PJRPID_ACTIVITY_UNKNOWN : PJRPID_ACTIVITY_AWAY;
			note = online ? pj_str("Idle") : pj_str("Away");
		}
	}
	if (note.slen) {
		pjsua_acc_id ids[PJSUA_MAX_ACC];
		unsigned count = PJSUA_MAX_ACC;
		if (pjsua_enum_accs(ids, &count) == PJ_SUCCESS) {
			pjrpid_element pr;
			pr.type = PJRPID_ELEMENT_TYPE_PERSON;
			pr.id = pj_str(NULL);
			pr.note = pj_str(NULL);
			pr.note = note;
			pr.activity = presenceStatusNew;
			for (unsigned i = 0; i < count; i++) {
				pjsua_acc_set_online_status2(ids[i], PJ_TRUE, &pr);
			}
		}
		m_PresenceStatus = presenceStatusNew;
	}
	if (!init) {
		UpdateWindowText();
	}
}

LRESULT CmainDlg::onCopyData(WPARAM wParam, LPARAM lParam)
{
	LRESULT res = TRUE;
	if (pj_ready) {
		COPYDATASTRUCT *s = (COPYDATASTRUCT*)lParam;
		if (s) {
			CString params = (LPCTSTR)s->lpData;
			if (s->dwData == 1) {
				res = CommandLine(params);
			} else if (s->dwData == 2) {
				res = FALSE;
				CString *str = new CString();
				str->SetString(params);
				PostMessage(UM_ON_COMMAND_LINE, 0, (LPARAM)str);
			}
		}
	}
	return res;
}

LRESULT CmainDlg::onCommandLine(WPARAM wParam, LPARAM lParam)
{
	CString *str = (CString *)lParam;
	CommandLine(*str);
	delete str;
	return 0;
}

bool CmainDlg::CommandLine(CString params) {
	bool activate = false;
	params.Trim();
	if (params.GetAt(0) == '"' && params.GetAt(params.GetLength() - 1) == '"') {
		params = params.Mid(1, params.GetLength() - 2);
	}
	if (!params.IsEmpty()) {
		if (params.Find(_T("msip:")) == 0) {
			CString cmd = params.Mid(5);
			if (cmd == _T("minimize")) {
				ShowWindow(SW_HIDE);
			}
			else if (cmd == _T("answer")) {
				msip_call_answer();
			}
			else if (cmd == _T("hangupall")) {
				call_hangup_all_noincoming();
			}
			else if (cmd == _T("hold")) {
				messagesDlg->OnBnClickedHold();
			}
			else if (cmd.Find(_T("transfer_")) == 0) {
				messagesDlg->CallAction(MSIP_ACTION_TRANSFER, cmd.Mid(9));
			}
			else if (cmd == _T("micmute")) {
				pageDialer->MuteInput(true);
			}
			else if (cmd == _T("micunmute")) {
				pageDialer->MuteInput(false);
			}
			else if (cmd == _T("speakmute")) {
				pageDialer->MuteOutput(true);
			}
			else if (cmd == _T("speakunmute")) {
				pageDialer->MuteOutput(false);
			}
			else if (cmd == _T("micmuteclick")) {
				pageDialer->OnBnClickedMuteInput();
			}
			else if (cmd == _T("speakmuteclick")) {
				pageDialer->OnBnClickedMuteOutput();
			}
			else if (cmd == _T("micup")) {
				pageDialer->OnBnClickedPlusInput();
			}
			else if (cmd == _T("micdown")) {
				pageDialer->OnBnClickedMinusInput();
			}
			else if (cmd == _T("speakup")) {
				pageDialer->OnBnClickedPlusOutput();
			}
			else if (cmd == _T("speakdown")) {
				pageDialer->OnBnClickedMinusOutput();
			}
			else if (!cmd.IsEmpty()) {
				DialNumberFromCommandLine(cmd);
			}
			return activate;
		}
		DialNumberFromCommandLine(params);
	}
	return activate;
}

bool CmainDlg::GotoTabLParam(LPARAM lParam) {
	CTabCtrl* tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	for (int i = 0; i < tab->GetItemCount(); i++) {
		TC_ITEM tci;
		tci.mask = TCIF_PARAM;
		tab->GetItem(i, &tci);
		if (tci.lParam == lParam) {
			return GotoTab(i, tab);
		}
	}
	return false;
}

bool CmainDlg::GotoTab(int i, CTabCtrl* tab) {
	if (!tab) {
		tab = (CTabCtrl*)GetDlgItem(IDC_MAIN_TAB);
	}
	int nTab = tab->GetCurSel();
	if (i < 0) {
		int max = tab->GetItemCount() - 1;
		if (i == -1) {
			i = nTab < max ? nTab + 1 : 0;
		}
		else {
			i = nTab == 0 ? max : nTab - 1;
		}
	}
	if (nTab != i) {
		TC_ITEM tci;
		tci.mask = TCIF_PARAM;
		if (tab->GetItem(i, &tci) && tci.lParam < 0) {
			i = 0;
		}
		if (nTab != i) {
			LRESULT pResult;
			OnTcnSelchangingTab(NULL, &pResult);
			tab->SetCurSel(i);
			OnTcnSelchangeTab(NULL, &pResult);
			return true;
		}
	}
	return false;
}


void CmainDlg::DialNumberFromCommandLine(CString params) {
	pjsua_acc_info info;
	if (params.Mid(0, 4).CompareNoCase(_T("tel:")) == 0 || params.Mid(0, 4).CompareNoCase(_T("sip:")) == 0) {
		params = params.Mid(4);
	}
	else if (params.Mid(0, 7).CompareNoCase(_T("callto:")) == 0) {
		params = params.Mid(7);
	}
	else if (params.Mid(0, 8).CompareNoCase(_T("dialpad:")) == 0) {
		params = params.Mid(8);
	}
	else if (params.Mid(0, 5).CompareNoCase(_T("dial:")) == 0) {
		params = params.Mid(5);
	}
	if (params.Mid(0, 2) == _T("//")) {
		params = params.Mid(2);
		if (params.Right(1) == _T("/")) {
			params = params.Mid(0, params.GetLength() - 1);
		}
	}
	int pos = params.Find(_T("/account:"));
	if (pos != -1) {
		CString value = params.Mid(pos + 9);
		int pos2 = -1;
		if (!value.IsEmpty()) {
			pos2 = value.Find(_T(" "));
			if (pos2 != -1) {
				value = value.Left(pos2);
			}
			int accountId = _wtoi(value);
			if (accountId > 0) {
				Account account;
				if (accountSettings.AccountLoad(accountId, &account)) {
					int pos = params.Find(_T("/password:"));
					if (pos != -1) {
						account.password = params.Mid(pos + 10);
						accountSettings.AccountSave(accountId, &account);
						if (accountSettings.accountId == accountId) {
							PJAccountDelete();
							accountSettings.AccountLoad(accountSettings.accountId, &accountSettings.account);
							OnAccountChanged();
							PJAccountAdd();
						}
						params.Empty();
					}
					else {
						if (accountSettings.accountId != accountId) {
							if (accountSettings.accountId) {
								PJAccountDelete();
							}
							accountSettings.accountId = accountId;
							accountSettings.AccountLoad(accountSettings.accountId, &accountSettings.account);
							accountSettings.SettingsSave();
							OnAccountChanged();
							PJAccountAdd();
						}
					}

				}
				else {
					params.Empty();
				}
			}
		}
		if (pos2 == -1) {
			params.Delete(pos - 1, params.GetLength());
		}
		else {
			params.Delete(pos, pos + 9 + pos2 + 1);
		}
	}
		if (params == _T("/answer")) {
			msip_call_answer();
		}
		else if (params == _T("/hangupall")) {
			call_hangup_all_noincoming();
		}
		else if (params == _T("/hangupincoming")) {
			call_hangup_incoming();
		}
		else if (params == _T("/hangupcalling")) {
			call_hangup_calling();
		}
		else if (params.Find(_T("/dtmf:")) == 0) {
			CString value = params.Mid(6);
			if (!value.IsEmpty()) {
				mainDlg->pageDialer->DTMF(value);
			}
		}
		else if (params.Find(_T("/password:")) == 0) {
			CString value = params.Mid(10);
			password = value;
		}
		else if (params.Find(_T("/transfer:")) == 0) {
			CString value = params.Mid(10);
			if (!value.IsEmpty()) {
				messagesDlg->CallAction(MSIP_ACTION_TRANSFER, value);
			}
		}
		else {
			if (!MACRO_SILENT) {
				GotoTab(0);
				onTrayNotify(NULL, WM_LBUTTONUP);
			}
			if (accountSettings.accountId > 0) {
				if (pjsua_acc_is_valid(account) &&
					(get_account_server().IsEmpty() ||
					(pjsua_acc_get_info(account, &info) == PJ_SUCCESS && info.status == 200)
						)
					) {
					DialNumber(params);
				}
				else {
					dialNumberDelayed = params;
				}
			}
			else {
				if (pjsua_acc_is_valid(account_local)) {
					DialNumber(params);
				}
				else if (accountSettings.enableLocalAccount) {
					dialNumberDelayed = params;
				}
			}
		}
}

void CmainDlg::DialNumber(CString params)
{
	CString number;
	CString message;
	int i = params.Find(_T(" "));
	if (i != -1) {
		number = params.Mid(0, i);
		message = params.Mid(i + 1);
		message.Trim();
	}
	else {
		number = params;
	}
	number.Replace(_T("%20"), _T(" "));
	number.Replace(_T("%2B"), _T("+"));
	number.Trim();
	if (!number.IsEmpty()) {
		if (message.IsEmpty()) {
			CString numberAdd = number;
			pageDialer->DialedAdd(numberAdd);
			MakeCall(number, false, true);
		}
		else {
			messagesDlg->SendInstantMessage(NULL, message, number);
		}
	}
}

bool CmainDlg::MakeCall(CString number, bool hasVideo, bool fromCommandLine, bool noTransform)
{
	if (accountSettings.singleMode && mainDlg->messagesDlg->GetCallsCount()) {
		GotoTab(0);
	}
	else if (!pjsua_acc_is_valid(account) && !pjsua_acc_is_valid(account_local)) {
		Account dummy;
		pj_status_t status = accountSettings.AccountLoad(1, &dummy) ? PJSIP_EAUTHACCDISABLED : PJSIP_EAUTHACCNOTFOUND;
		MSIP::ShowErrorMessage(status);
	}
	else
	{
		if (MessagesOpen(number, true, noTransform)) {
			MessagesContact* messagesContact = messagesDlg->GetMessageContact();
			messagesContact->fromCommandLine = fromCommandLine;
			messagesDlg->Call(hasVideo);
			return true;
		}
	}
	return false;
}

bool CmainDlg::MessagesOpen(CString number, bool forCall, bool noTransform)
{
	CString commands;
	CString numberFormated = FormatNumber(number, &commands, noTransform);
	pj_status_t pj_status = msip_verify_sip_url(MSIP::StrToPj(numberFormated));
	if (pj_status == PJ_SUCCESS) {
		bool doNotShowMessagesWindow = false;
		if (forCall) {
			doNotShowMessagesWindow = accountSettings.singleMode ||
				(MACRO_SILENT && !mainDlg->IsWindowVisible());
		}
		MessagesContact* messagesContact = messagesDlg->AddTab(numberFormated, TRUE, NULL, NULL, doNotShowMessagesWindow, FALSE, number);
		if (messagesContact) {
			messagesContact->commands = commands;
			return true;
		}
	}
	else {
		MSIP::ShowErrorMessage(pj_status);
	}
	return false;
}

bool CmainDlg::AutoAnswer(pjsua_call_id call_id, bool force)
{
	bool allow = false;
	allow = !messagesDlg->GetCallsCount();
	if (allow) {
		bool play = false;
		if (!force) {
			if (accountSettings.localDTMF) {
				autoAnswerPlayCallId = call_id;
				onPlayerPlay(MSIP_SOUND_RINGIN2, 0);
				play = true;
			}
		}
		if (!play) {
			pjsua_call_info call_info;
			if (pjsua_var.state != PJSUA_STATE_RUNNING || pjsua_call_get_info(call_id, &call_info) != PJ_SUCCESS || (call_info.state != PJSIP_INV_STATE_INCOMING && call_info.state != PJSIP_INV_STATE_EARLY)) {
				return false;
			}
			call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
			if (user_data) {
				user_data->CS.Lock();
				user_data->autoAnswer = true;
				user_data->CS.Unlock();
			}
			mainDlg->PostMessage(UM_CALL_ANSWER, (WPARAM)call_id, (LPARAM)call_info.rem_vid_cnt);
		}
	}
	return allow;
}

pjsua_call_id CmainDlg::CurrentCallId()
{
	MessagesContact* messagesContact = messagesDlg->GetMessageContact();
	if (messagesContact) {
		return messagesContact->callId;
	}
	return -1;
}

void CmainDlg::ShortcutAction(Shortcut *shortcut, bool block, bool second)
{
	pjsua_call_id current_call_id;
	CString params;
	CString number = second && !shortcut->number2.IsEmpty() ? shortcut->number2 : shortcut->number;
	if (shortcut->type == MSIP_SHORTCUT_CALL) {
		mainDlg->MakeCall(number);
	}
	else if (shortcut->type == MSIP_SHORTCUT_VIDEOCALL) {
#ifdef _GLOBAL_VIDEO
		mainDlg->MakeCall(number, true);
#else
		mainDlg->MakeCall(number);
#endif
	}
	else if (shortcut->type == MSIP_SHORTCUT_MESSAGE) {
		mainDlg->MessagesOpen(number);
	}
	else if (shortcut->type == MSIP_SHORTCUT_DTMF) {
		mainDlg->pageDialer->DTMF(number);
	}
	else if (shortcut->type == MSIP_SHORTCUT_TRANSFER) {
		if (number.IsEmpty()) {
			OpenTransferDlg(mainDlg, MSIP_ACTION_TRANSFER);
		}
		else {
			messagesDlg->CallAction(MSIP_ACTION_TRANSFER, number);
		}
	}
	else if (shortcut->type == MSIP_SHORTCUT_ATTENDED_TRANSFER) {
		if (number.IsEmpty()) {
			OpenTransferDlg(mainDlg, MSIP_ACTION_ATTENDED_TRANSFER);
		}
		else {
			messagesDlg->CallAction(MSIP_ACTION_ATTENDED_TRANSFER, number);
		}
	}
	else if (shortcut->type == MSIP_SHORTCUT_CONFERENCE) {
		messagesDlg->CallAction(MSIP_ACTION_INVITE, number);
	}
	else if (shortcut->type == MSIP_SHORTCUT_RUNBATCH) {
		AfxMessageBox(_T(_GLOBAL_BUSINESS_FEATURE));
			}
	else if (shortcut->type == MSIP_SHORTCUT_CALL_URL || shortcut->type == MSIP_SHORTCUT_POP_URL) {
		AfxMessageBox(_T(_GLOBAL_BUSINESS_FEATURE));
	}
}

void CmainDlg::ShortcutsRemoveAll()
{
	for (int i = 0; i < shortcuts.GetCount(); i++) {
		Shortcut* shortcut = &shortcuts.GetAt(i);
		if (shortcut->presence) {
			shortcut->presence = false;
			mainDlg->UnsubscribeNumber(&shortcut->number);
		}
	}
	shortcuts.RemoveAll();
}

LRESULT CmainDlg::onPlayerPlay(WPARAM wParam, LPARAM lParam)
{
	CString filename;
	BOOL noLoop;
	BOOL inCall;
	if (wParam == MSIP_SOUND_CUSTOM) {
		filename = *(CString*)lParam;
		noLoop = FALSE;
		inCall = FALSE;
	} else if (wParam == MSIP_SOUND_CUSTOM_NOLOOP) {
		filename = *(CString*)lParam;
		noLoop = TRUE;
		inCall = FALSE;
	}
	else {
		switch (wParam) {
		case MSIP_SOUND_MESSAGE_IN:
			filename.Append(_T("msgin.wav"));
			noLoop = TRUE;
			inCall = FALSE;
			break;
		case MSIP_SOUND_MESSAGE_OUT:
			filename.Append(_T("msgout.wav"));
			noLoop = TRUE;
			inCall = FALSE;
			break;
		case MSIP_SOUND_HANGUP:
			filename.Append(_T("hangup.wav"));
			noLoop = TRUE;
			inCall = TRUE;
			break;
		case MSIP_SOUND_RINGTONE:
			filename.Append(_T("ringtone.wav"));
			noLoop = FALSE;
			inCall = FALSE;
			break;
		case MSIP_SOUND_RINGIN2:
			filename.Append(_T("ringing2.wav"));
			noLoop = TRUE;
			inCall = TRUE;
			break;
		case MSIP_SOUND_RINGING:
			filename.Append(_T("ringing.wav"));
			noLoop = TRUE;
			inCall = TRUE;
			break;
		default:
			noLoop = TRUE;
			inCall = FALSE;
		}
	}
	if (filename.Find('\\') == -1 && filename.Find('/') == -1) {
		filename = accountSettings.pathExe + _T("\\") + filename;
	}
	PlayerPlay(filename, noLoop, inCall);
	return 0;
}

LRESULT CmainDlg::onPlayerStop(WPARAM wParam, LPARAM lParam)
{
	PlayerStop();
	if (autoAnswerPlayCallId != PJSUA_INVALID_ID) {
		AutoAnswer(autoAnswerPlayCallId, true);
		autoAnswerPlayCallId = PJSUA_INVALID_ID;
	}
	return 0;
}

static PJ_DEF(pj_status_t) on_pjsua_wav_file_end_callback(pjmedia_port* media_port, void* args)
{
	mainDlg->PostMessage(UM_ON_PLAYER_STOP, 0, 0);
	return -1;//Here it is important to return value other than PJ_SUCCESS
}

void CmainDlg::PlayerPlay(CString filename, bool noLoop, bool inCall, bool isAA)
{
	PlayerStop();
	bool stopCallback = false;
	if (!filename.IsEmpty()) {
		pj_str_t file = MSIP::StrToPjStr(filename);
		pjsua_player_id player_id;
		if (pjsua_var.state == PJSUA_STATE_RUNNING && pjsua_player_create(&file, noLoop ? PJMEDIA_FILE_NO_LOOP : 0, &player_id) == PJ_SUCCESS) {
			pjmedia_port *player_media_port;
			if (pjsua_player_get_port(player_id, &player_media_port) == PJ_SUCCESS) {
				if (!player_eof_data) {
					pj_pool_t *pool = pjsua_pool_create("microsip_eof_data", 512, 512);
					player_eof_data = PJ_POOL_ZALLOC_T(pool, struct player_eof_data);
					player_eof_data->pool = pool;
				}
				player_eof_data->player_id = player_id;
				if (noLoop) {
					if (pjmedia_wav_player_set_eof_cb(player_media_port, player_eof_data, &on_pjsua_wav_file_end_callback) == PJ_SUCCESS) {
						stopCallback = true;
					}
				}
				if (
					(!tone_gen && pjsua_conf_get_active_ports() <= 2)
					||
					(tone_gen && pjsua_conf_get_active_ports() <= 3)
					) {
					msip_set_sound_device(inCall ? msip_audio_output : msip_audio_ring);
				}
				pjsua_conf_port_id conf_port_id = pjsua_player_get_conf_port(player_id);
				if (inCall) {
					pjsua_conf_adjust_rx_level(conf_port_id, 0.4);
				}
				else {
					pjsua_conf_adjust_rx_level(conf_port_id, (float)accountSettings.volumeRing / 100);
				}
				pjsua_conf_connect(conf_port_id, 0);
			}
		}
	}
	if (noLoop && !stopCallback) {
		onPlayerStop(NULL, NULL);
	}
}

void CmainDlg::PlayerStop()
{
	if (player_eof_data && player_eof_data->player_id != PJSUA_INVALID_ID) {
		if (pjsua_var.state != PJSUA_STATE_NULL) {
			pjsua_conf_disconnect(pjsua_player_get_conf_port(player_eof_data->player_id), 0);
			pjsua_player_destroy(player_eof_data->player_id);
			player_eof_data->player_id = PJSUA_INVALID_ID;
		}
		else {
			player_eof_data->player_id = PJSUA_INVALID_ID;
		}
	}
}

bool CmainDlg::CommandCallAnswer() {
	if (ringinDlgs.GetCount()) {
		RinginDlg *ringinDlg = ringinDlgs.GetAt(0);
		mainDlg->PostMessage(UM_CALL_ANSWER, (WPARAM)ringinDlg->call_id, (LPARAM)0);
		return true;
	}
	return false;
}

bool CmainDlg::CommandCallReject()
{
	if (ringinDlgs.GetCount()) {
		RinginDlg *ringinDlg = ringinDlgs.GetAt(ringinDlgs.GetCount()-1);
		ringinDlg->OnBnClickedDecline();
		return true;
	}
	return false;
}

LRESULT CmainDlg::onShellHookMessage(WPARAM wParam, LPARAM lParam)
{
	if (wParam == HSHELL_APPCOMMAND) {
		int nCmd = GET_APPCOMMAND_LPARAM(lParam);
		if (nCmd == APPCOMMAND_MEDIA_PLAY ||
			nCmd == APPCOMMAND_MEDIA_PLAY_PAUSE ||
			nCmd == APPCOMMAND_MEDIA_STOP) {
			if (ringinDlgs.GetCount()) {
				RinginDlg *ringinDlg = ringinDlgs.GetAt(0);
				if (nCmd == APPCOMMAND_MEDIA_STOP) {
					ringinDlg->OnBnClickedDecline();
				}
				else {
					ringinDlg->CallAccept(ringinDlg->remoteHasVideo);
				}
			}
			else {
				if (nCmd == APPCOMMAND_MEDIA_STOP) {
					messagesDlg->OnBnClickedEnd();
				}
				else {
					CButton* callButton = (CButton*)pageDialer->GetDlgItem(IDC_CALL);
					WINDOWINFO wndInfo;
					callButton->GetWindowInfo(&wndInfo);
					bool isButtonVisisble = wndInfo.dwStyle & WS_VISIBLE;
					if (isButtonVisisble && callButton->IsWindowEnabled()) {
						pageDialer->OnBnClickedCall();
					}
					else {
						messagesDlg->OnBnClickedHold();
					}
				}
			}
		}
		else if (nCmd == APPCOMMAND_MEDIA_PAUSE) {
			messagesDlg->OnBnClickedHold();
		}
	}
	return 0;
}

LRESULT CmainDlg::onCallAnswer(WPARAM wParam, LPARAM lParam)
{
	if (pjsua_var.state == PJSUA_STATE_RUNNING) {
		pjsua_call_id call_id = wParam;
		pjsua_call_info call_info;
		if (pjsua_call_get_info(call_id, &call_info) == PJ_SUCCESS) {
			if (call_info.role == PJSIP_ROLE_UAS && (call_info.state == PJSIP_INV_STATE_INCOMING || call_info.state == PJSIP_INV_STATE_EARLY)) {
				if (lParam < 0) {
					pjsua_call_answer(call_id, -lParam, NULL, NULL);
					return 0;
				}
				if (accountSettings.singleMode) {
					call_hangup_all_noincoming();
				}
				msip_set_sound_device(msip_audio_output);
#ifdef _GLOBAL_VIDEO
				if (lParam > 0) {
					createPreviewWin();
				}
#endif
				pjsua_call_setting call_setting;
				pjsua_call_setting_default(&call_setting);
				call_setting.vid_cnt = lParam > 0 ? 1 : 0;
				if (pjsua_call_answer2(call_id, &call_setting, 200, NULL, NULL) == PJ_SUCCESS) {
					callIdIncomingIgnore = MSIP::PjToStr(&call_info.call_id);
				}
				PlayerStop();
				bool restore = true;
				if (MACRO_SILENT) {
					restore = false;
				}

				call_user_data *user_data = (call_user_data *)pjsua_call_get_user_data(call_id);
				if (user_data) {
					user_data->CS.Lock();
					if (user_data->autoAnswer) {
						if (!accountSettings.bringToFrontOnIncoming) {
							restore = false;
							if (GetForegroundWindow()->GetTopLevelParent() != this) {
								SIPURI sipuri;
								MSIP::ParseSIPURI(MSIP::PjToStr(&call_info.remote_info, TRUE), &sipuri);
								BaloonPopup(Translate(_T("Auto Answer")), !sipuri.name.IsEmpty() ? sipuri.name : sipuri.user, NIIF_INFO);
							}
						}
					}
					user_data->CS.Unlock();
				}

				if (restore) {
					onTrayNotify(NULL, WM_LBUTTONUP);
				}
			}
		}
	}
	return 0;
}

LRESULT CmainDlg::onCallHangup(WPARAM wParam, LPARAM lParam)
{
	if (pjsua_var.state == PJSUA_STATE_RUNNING) {
		pjsua_call_id call_id = wParam;
		msip_call_hangup_fast(call_id);
	}
	return 0;
}

LRESULT CmainDlg::onTabIconUpdate(WPARAM wParam, LPARAM lParam)
{
	if (messagesDlg) {
		pjsua_call_id call_id = wParam;
		for (int i = 0; i < messagesDlg->tab->GetItemCount(); i++) {
			MessagesContact* messagesContact = messagesDlg->GetMessageContact(i);
			if (messagesContact->callId == call_id) {
				pjsua_call_info call_info;
				if (pjsua_call_get_info(call_id, &call_info) == PJ_SUCCESS) {
					messagesDlg->UpdateTabIcon(messagesContact, i, &call_info);
				}
				break;
			}
		}
	}
	return 0;
}

LRESULT CmainDlg::onSetPaneText(WPARAM wParam, LPARAM lParam)
{
	CString* pString = (CString*)lParam;
	ASSERT(pString != NULL);
	m_bar.SetPaneText(0, *pString);
	delete pString;
	return 0;
}

void CmainDlg::SetPaneText2(CString str)
{
	if (str.IsEmpty()) {
		m_bar.SetPaneInfo(IDS_STATUSBAR2, IDS_STATUSBAR2, SBPS_NOBORDERS, 0);
	}
	else {
		CSize size = m_bar.GetDC()->GetTextExtent(str);
		int width = size.cx * 0.85;
		m_bar.SetPaneInfo(IDS_STATUSBAR2, IDS_STATUSBAR2, SBPS_NORMAL, width);
	}
	m_bar.SetPaneText(IDS_STATUSBAR2, str);
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, IDS_STATUSBAR);
}


BOOL CmainDlg::CopyStringToClipboard(IN const CString & str)
{
	// Open the clipboard
	if (!OpenClipboard())
		return FALSE;

	// Empty the clipboard
	if (!EmptyClipboard())
	{
		CloseClipboard();
		return FALSE;
	}

	// Number of bytes to copy (consider +1 for end-of-string, and
	// properly scale byte size to sizeof(TCHAR))
	SIZE_T textCopySize = (str.GetLength() + 1) * sizeof(TCHAR);

	// Allocate a global memory object for the text
	HGLOBAL hTextCopy = GlobalAlloc(GMEM_MOVEABLE, textCopySize);
	if (hTextCopy == NULL)
	{
		CloseClipboard();
		return FALSE;
	}

	// Lock the handle, and copy source text to the buffer
	TCHAR * textCopy = reinterpret_cast<TCHAR *>(GlobalLock(
		hTextCopy));
	ASSERT(textCopy != NULL);
	StringCbCopy(textCopy, textCopySize, str.GetString());
	GlobalUnlock(hTextCopy);
	textCopy = NULL; // avoid dangling references

	// Place the handle on the clipboard
#if defined( _UNICODE )
	UINT textFormat = CF_UNICODETEXT;  // Unicode text
#else
	UINT textFormat = CF_TEXT;         // ANSI text
#endif // defined( _UNICODE )

	if (SetClipboardData(textFormat, hTextCopy) == NULL)
	{
		// Failed
		CloseClipboard();
		return FALSE;
	}

	// Release the clipboard
	CloseClipboard();

	// All right
	return TRUE;
}

void CmainDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
		if (nID == SC_CLOSE) {
			ShowWindow(SW_HIDE);
		}
		else {
			if (!accountSettings.singleMode) {
				if (nID == SC_RESTORE) {
					if (messagesDlg->tab->GetItemCount()) {
						messagesDlg->ShowWindow(SW_SHOW);
					}
				}
				if (nID == SC_MINIMIZE) {
					messagesDlg->ShowWindow(SW_HIDE);
				}
			}
			__super::OnSysCommand(nID, lParam);
		}

}

BOOL CmainDlg::OnQueryEndSession()
{
	return TRUE;
}

void CmainDlg::OnClose()
{
	DestroyWindow();
}

HBRUSH CmainDlg::OnCtlColor(CDC* pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH br = CBaseDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	return br;
}

void CmainDlg::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CPoint local = point;
	ScreenToClient(&local);
	CRect rect;
	GetClientRect(&rect);
	int height = 16;
	CDC *pDC = GetDC();
	if (pDC) {
		int dpiY = GetDeviceCaps(pDC->m_hDC, LOGPIXELSY);
		height = MulDiv(height, dpiY, 96);
		ReleaseDC(pDC);
	}

	if (rect.Height() - local.y <= height) {
		MainPopupMenu();
	}
	else {
		DefWindowProc(WM_CONTEXTMENU, NULL, MAKELPARAM(point.x, point.y));
	}
}

BOOL CmainDlg::OnDeviceChange(UINT nEventType, DWORD_PTR dwData)
{
	if (nEventType == DBT_DEVNODES_CHANGED) {
		if (pjsua_var.state == PJSUA_STATE_RUNNING) {
			if (dwData == 1) {
				PJ_LOG(3, (THIS_FILENAME, "OnDeviceStateChanged event, schedule refresh devices"));
			}
			else {
				PJ_LOG(3, (THIS_FILENAME, "WM_DEVICECHANGE received, schedule refresh devices"));
			}
			KillTimer(IDT_TIMER_SWITCH_DEVICES);
			SetTimer(IDT_TIMER_SWITCH_DEVICES, 1500, NULL);
		}
	}
	return FALSE;
}

void CmainDlg::OnSessionChange(UINT nSessionState, UINT nId)
{
	if (nSessionState == WTS_REMOTE_CONNECT || nSessionState == WTS_CONSOLE_CONNECT) {
		if (pj_ready) {
			PJ_LOG(3, (THIS_FILENAME, "WM_WTSSESSION_CHANGE received, schedule refresh devices"));
			KillTimer(IDT_TIMER_SWITCH_DEVICES);
			SetTimer(IDT_TIMER_SWITCH_DEVICES, 1500, NULL);
		}
	}
}

void CmainDlg::OnMove(int x, int y)
{
	if (IsWindowVisible() && !IsZoomed() && !IsIconic()) {
		CRect cRect;
		GetWindowRect(&cRect);
		accountSettings.mainX = cRect.left;
		accountSettings.mainY = cRect.top;
		AccountSettingsPendingSave();
	}
}

void CmainDlg::OnSize(UINT type, int w, int h)
{
	CBaseDialog::OnSize(type, w, h);
	if (this->IsWindowVisible() && type == SIZE_RESTORED) {
		CRect cRect;
		GetWindowRect(&cRect);
		accountSettings.mainW = cRect.Width();
		accountSettings.mainH = cRect.Height();
		AccountSettingsPendingSave();
	}
}

void CmainDlg::SetupJumpList()
{
	JumpList jl(_T(_GLOBAL_NAME_NICE));
	jl.AddTasks();
}

void CmainDlg::RemoveJumpList()
{
	JumpList jl(_T(_GLOBAL_NAME_NICE));
	jl.DeleteJumpList();
}

void CmainDlg::OnMenuWebsite()
{
	CString url = _T(_GLOBAL_MENU_WEBSITE);
	MSIP::OpenURL(url);
}

void CmainDlg::OnMenuHelp()
{
	OpenHelp();
}

void CmainDlg::OnMenuAddl()
{
}

void CmainDlg::OnMuteInput()
{
	pageDialer->OnBnClickedMuteInput();
}

void CmainDlg::OnMuteOutput()
{
	pageDialer->OnBnClickedMuteOutput();
}

LRESULT CmainDlg::onCustomLoaded(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT CmainDlg::onUsersDirectoryLoaded(WPARAM wParam, LPARAM lParam)
{
	//PJ_LOG(3, (THIS_FILENAME, "Users directory loaded"));
	URLGetAsyncData *response = (URLGetAsyncData *)wParam;
	int refresh = -1;
	if (response->statusCode == 200 && !response->body.IsEmpty()) {
		if (!response->body.IsEmpty()) {
			CArray<ContactWithFields*> contacts;
			ContactWithFields *contactWithFields;
			BOOL ok = FALSE;
			if (response->headers.Find(_T("Content-Type: text/csv")) != -1) {
				TCHAR path[MAX_PATH];
				if (GetTempPath(MAX_PATH, path)) {
					TCHAR filename[MAX_PATH];
					if (GetTempFileName(path, _T("csv"), 0, filename)) {
						CFile tmp;
						CFileException fileException;
						if (tmp.Open(filename, CFile::modeWrite, &fileException)) {
							tmp.Write(response->body, response->body.GetLength());
							tmp.Close();
							if (pageContacts->Import(filename, contacts)) {
								ok = TRUE;
							}
						}
						DeleteFile(filename);
					}
				}
			}
			else {
				// JSON
				Json::Value root;
				Json::Reader reader;
				bool parsedSuccess = reader.parse((LPCSTR)response->body,
					root,
					false);
				if (parsedSuccess) {
					//PJ_LOG(3, (THIS_FILENAME, "JSON foramt detected"));
					try {
						if (!root.isArray()) {
							if (root.isMember("refresh") && root["refresh"].isInt()) {
								refresh = root["refresh"].asInt();
							}
							if (root.isMember("items") && root["items"].isArray()) {
								root = root["items"];
							}
						}
						if (root.isArray()) {
							for (Json::Value::ArrayIndex i = 0; i != root.size(); i++) {
								contactWithFields = new ContactWithFields();
								contactWithFields->contact.directory = true;
								if (root[i]["name"].isString()) {
									contactWithFields->fields.AddTail(_T("name"));
									contactWithFields->contact.name = MSIP::Utf8DecodeUni(root[i]["name"].asCString());
								}
								if (root[i]["number"].isString()) {
									contactWithFields->fields.AddTail(_T("number"));
									contactWithFields->contact.number = MSIP::Utf8DecodeUni(root[i]["number"].asCString());
								}
								else if (root[i]["phone"].isString()) {
									contactWithFields->fields.AddTail(_T("number"));
									contactWithFields->contact.number = MSIP::Utf8DecodeUni(root[i]["phone"].asCString());
								}
								else if (root[i]["telephone"].isString()) {
									contactWithFields->fields.AddTail(_T("number"));
									contactWithFields->contact.number = MSIP::Utf8DecodeUni(root[i]["telephone"].asCString());
								}
								if (root[i]["firstname"].isString()) {
									contactWithFields->fields.AddTail(_T("firstname"));
									contactWithFields->contact.firstname = MSIP::Utf8DecodeUni(root[i]["firstname"].asCString());
								}
								if (root[i]["lastname"].isString()) {
									contactWithFields->fields.AddTail(_T("lastname"));
									contactWithFields->contact.lastname = MSIP::Utf8DecodeUni(root[i]["lastname"].asCString());
								}
								if (root[i]["phone"].isString()) {
									contactWithFields->fields.AddTail(_T("phone"));
									contactWithFields->contact.phone = MSIP::Utf8DecodeUni(root[i]["phone"].asCString());
								}
								if (root[i]["mobile"].isString()) {
									contactWithFields->fields.AddTail(_T("mobile"));
									contactWithFields->contact.mobile = MSIP::Utf8DecodeUni(root[i]["mobile"].asCString());
								}
								if (root[i]["email"].isString()) {
									contactWithFields->fields.AddTail(_T("email"));
									contactWithFields->contact.email = MSIP::Utf8DecodeUni(root[i]["email"].asCString());
								}
								if (root[i]["address"].isString()) {
									contactWithFields->fields.AddTail(_T("address"));
									contactWithFields->contact.address = MSIP::Utf8DecodeUni(root[i]["address"].asCString());
								}
								if (root[i]["city"].isString()) {
									contactWithFields->fields.AddTail(_T("city"));
									contactWithFields->contact.city = MSIP::Utf8DecodeUni(root[i]["city"].asCString());
								}
								if (root[i]["state"].isString()) {
									contactWithFields->fields.AddTail(_T("state"));
									contactWithFields->contact.state = MSIP::Utf8DecodeUni(root[i]["state"].asCString());
								}
								if (root[i]["zip"].isString()) {
									contactWithFields->fields.AddTail(_T("zip"));
									contactWithFields->contact.zip = MSIP::Utf8DecodeUni(root[i]["zip"].asCString());
								}
								if (root[i]["comment"].isString()) {
									contactWithFields->fields.AddTail(_T("comment"));
									contactWithFields->contact.comment = MSIP::Utf8DecodeUni(root[i]["comment"].asCString());
								}
								if (root[i]["id"].isString()) {
									contactWithFields->fields.AddTail(_T("id"));
									contactWithFields->contact.id = MSIP::Utf8DecodeUni(root[i]["id"].asCString());
								}
								if (root[i]["info"].isString()) {
									contactWithFields->fields.AddTail(_T("info"));
									contactWithFields->contact.info = MSIP::Utf8DecodeUni(root[i]["info"].asCString());
								}
								if (root[i]["presence"].isInt()) {
									contactWithFields->fields.AddTail(_T("presence"));
									contactWithFields->contact.presence = root[i]["presence"].asInt() ? 1 : 0;
								}
								if (root[i]["starred"].isInt()) {
									contactWithFields->fields.AddTail(_T("starred"));
									contactWithFields->contact.starred = root[i]["starred"].asInt() ? 1 : 0;
								}
								if (pageContacts->ContactPrepare(&contactWithFields->contact)) {
									contacts.Add(contactWithFields);
								}
								else {
									delete contactWithFields;
								}
							}
							ok = TRUE;
						}
					}
					catch (std::exception const& e) {
					}
				}
				else {
					// XML
					//PJ_LOG(3, (THIS_FILENAME, "XML foramt detected"));
					CMarkup xml;
					BOOL bResult = xml.SetDoc(MSIP::Utf8DecodeUni(response->body));
					if (bResult) {
						ok = TRUE;
						if (xml.FindElem(_T("contacts"))) {
							if (xml.FindAttrib(_T("refresh"))) {
								refresh = _wtoi(xml.GetAttrib(_T("refresh")));
							}
							while (xml.FindChildElem(_T("contact"))) {
								xml.IntoElem();
								contactWithFields = new ContactWithFields();
								contactWithFields->contact.directory = true;
								if (xml.FindAttrib(_T("name"))) {
									contactWithFields->fields.AddTail(_T("name"));
									contactWithFields->contact.name = xml.GetAttrib(_T("name"));
								}
								if (xml.FindAttrib(_T("number"))) {
									contactWithFields->fields.AddTail(_T("number"));
									contactWithFields->contact.number = xml.GetAttrib(_T("number"));
								}
								if (xml.FindAttrib(_T("firstname"))) {
									contactWithFields->fields.AddTail(_T("firstname"));
									contactWithFields->contact.firstname = xml.GetAttrib(_T("firstname"));
								}
								if (xml.FindAttrib(_T("lastname"))) {
									contactWithFields->fields.AddTail(_T("lastname"));
									contactWithFields->contact.lastname = xml.GetAttrib(_T("lastname"));
								}
								if (xml.FindAttrib(_T("phone"))) {
									contactWithFields->fields.AddTail(_T("phone"));
									contactWithFields->contact.phone = xml.GetAttrib(_T("phone"));
								}
								if (xml.FindAttrib(_T("mobile"))) {
									contactWithFields->fields.AddTail(_T("mobile"));
									contactWithFields->contact.mobile = xml.GetAttrib(_T("mobile"));
								}
								if (xml.FindAttrib(_T("email"))) {
									contactWithFields->fields.AddTail(_T("email"));
									contactWithFields->contact.email = xml.GetAttrib(_T("email"));
								}
								if (xml.FindAttrib(_T("address"))) {
									contactWithFields->fields.AddTail(_T("address"));
									contactWithFields->contact.address = xml.GetAttrib(_T("address"));
								}
								if (xml.FindAttrib(_T("city"))) {
									contactWithFields->fields.AddTail(_T("city"));
									contactWithFields->contact.city = xml.GetAttrib(_T("city"));
								}
								if (xml.FindAttrib(_T("state"))) {
									contactWithFields->fields.AddTail(_T("state"));
									contactWithFields->contact.state = xml.GetAttrib(_T("state"));
								}
								if (xml.FindAttrib(_T("zip"))) {
									contactWithFields->fields.AddTail(_T("zip"));
									contactWithFields->contact.zip = xml.GetAttrib(_T("zip"));
								}
								if (xml.FindAttrib(_T("comment"))) {
									contactWithFields->fields.AddTail(_T("comment"));
									contactWithFields->contact.comment = xml.GetAttrib(_T("comment"));
								}
								if (xml.FindAttrib(_T("id"))) {
									contactWithFields->fields.AddTail(_T("id"));
									contactWithFields->contact.id = xml.GetAttrib(_T("id"));
								}
								if (xml.FindAttrib(_T("info"))) {
									contactWithFields->fields.AddTail(_T("info"));
									contactWithFields->contact.info = xml.GetAttrib(_T("info"));
								}
								if (xml.FindAttrib(_T("presence"))) {
									contactWithFields->fields.AddTail(_T("presence"));
									CString rab = xml.GetAttrib(_T("presence"));
									contactWithFields->contact.presence = rab == _T("1");
								}
								if (xml.FindAttrib(_T("starred"))) {
									contactWithFields->fields.AddTail(_T("starred"));
									CString rab = xml.GetAttrib(_T("starred"));
									contactWithFields->contact.starred = rab == _T("1");
								}
								if (pageContacts->ContactPrepare(&contactWithFields->contact)) {
									contacts.Add(contactWithFields);
								}
								else {
									delete contactWithFields;
								}
								xml.OutOfElem();
							}
						}
						else if (xml.FindElem(_T("YealinkIPPhoneBook"))) {
							while (xml.FindChildElem(_T("Menu"))) {
								xml.IntoElem();
								while (xml.FindChildElem(_T("Unit"))) {
									xml.IntoElem();
									contactWithFields = new ContactWithFields();
									contactWithFields->contact.directory = true;
									if (xml.FindAttrib(_T("Name"))) {
										contactWithFields->fields.AddTail(_T("name"));
										contactWithFields->contact.name = xml.GetAttrib(_T("Name"));
									}
									if (xml.FindAttrib(_T("Phone1"))) {
										contactWithFields->fields.AddTail(_T("number"));
										contactWithFields->contact.number = xml.GetAttrib(_T("Phone1"));
									}
									if (xml.FindAttrib(_T("Phone2"))) {
										contactWithFields->fields.AddTail(_T("phone"));
										contactWithFields->contact.phone = xml.GetAttrib(_T("Phone2"));
									}
									if (xml.FindAttrib(_T("Phone3"))) {
										contactWithFields->fields.AddTail(_T("mobile"));
										contactWithFields->contact.mobile = xml.GetAttrib(_T("Phone3"));
									}
									if (pageContacts->ContactPrepare(&contactWithFields->contact)) {
										contacts.Add(contactWithFields);
									}
									else {
										delete contactWithFields;
									}
									xml.OutOfElem();
								}
								xml.OutOfElem();
							}
						}
						else {
							while (xml.FindChildElem(_T("entry")) || xml.FindChildElem(_T("DirectoryEntry"))) {
								xml.IntoElem();
								contactWithFields = new ContactWithFields();
								contactWithFields->contact.directory = true;
								if (xml.FindChildElem(_T("extension")) || xml.FindChildElem(_T("Telephone"))) {
									contactWithFields->fields.AddTail(_T("number"));
									contactWithFields->contact.number = xml.GetChildData();
									if (xml.FindChildElem(_T("extension")) || xml.FindChildElem(_T("Telephone"))) {
										contactWithFields->fields.AddTail(_T("phone"));
										contactWithFields->contact.phone = xml.GetChildData();
									}
									if (xml.FindChildElem(_T("extension")) || xml.FindChildElem(_T("Telephone"))) {
										contactWithFields->fields.AddTail(_T("mobile"));
										contactWithFields->contact.mobile = xml.GetChildData();
									}
									xml.ResetChildPos();
								}
								if (xml.FindChildElem(_T("name")) || xml.FindChildElem(_T("Name"))) {
									contactWithFields->fields.AddTail(_T("name"));
									contactWithFields->contact.name = xml.GetChildData();
									xml.ResetChildPos();
								}
								if (xml.FindChildElem(_T("info"))) {
									contactWithFields->fields.AddTail(_T("info"));
									contactWithFields->contact.info = xml.GetChildData();
									xml.ResetChildPos();
								}
								if (xml.FindChildElem(_T("presence"))) {
									contactWithFields->fields.AddTail(_T("presence"));
									contactWithFields->contact.presence = xml.GetChildData() == _T("1");
									xml.ResetChildPos();
								}
								if (xml.FindChildElem(_T("starred"))) {
									contactWithFields->fields.AddTail(_T("starred"));
									contactWithFields->contact.starred = xml.GetChildData() == _T("1");
									xml.ResetChildPos();
								}
								if (pageContacts->ContactPrepare(&contactWithFields->contact)) {
									contacts.Add(contactWithFields);
								}
								else {
									delete contactWithFields;
								}
								xml.OutOfElem();
							}
						}
					}
				}
			}
			if (ok) {
				pageContacts->ContactsAdd(&contacts, true);
				for (int i = 0; i < contacts.GetCount(); i++) {
					contactWithFields = contacts.GetAt(i);
					delete contactWithFields;
				}
				usersDirectoryLoaded = true;
				pageContacts->m_SortItemsExListCtrl.SortColumn(pageContacts->m_SortItemsExListCtrl.GetSortColumn(), pageContacts->m_SortItemsExListCtrl.IsAscending());
			}
		}
		if (refresh == -1) {
			response->headers.MakeLower();
			CString search = _T("\r\ncache-control:");
			int n = response->headers.Find(search);
			if (n > 0) {
				n = n + search.GetLength();
				int l = response->headers.Find(_T("\r\n"), n);
				if (l > 0) {
					response->headers = response->headers.Mid(n, l - n);
					search = _T("max-age=");
					n = response->headers.Find(search);
					if (n != -1) {
						response->headers = response->headers.Mid(n + search.GetLength());
						refresh = atoi(CStringA(response->headers));
					}
				}
			}
		}
	}
	if (refresh <= 0) {
		refresh = 3600;
	}
	else if (refresh < 60) {
		refresh = 60;
	}
	else if (refresh > 86400) {
		refresh = 86400;
	}
	SetTimer(IDT_TIMER_DIRECTORY, 1000 * refresh, NULL);

	//PJ_LOG(3, (THIS_FILENAME, "End UsersDirectoryLoad"));
	delete response;
	return 0;
}

void CmainDlg::UsersDirectoryLoad()
{
	KillTimer(IDT_TIMER_DIRECTORY);
	if (!accountSettings.usersDirectory.IsEmpty()) {
		//PJ_LOG(3, (THIS_FILENAME, "Users directory load"));
		CString url = accountSettings.usersDirectory;
		url.Replace(_T("%"), _T("*"));
		url.Replace(_T("*s"), _T("%s"));
		url.Format(url, accountSettings.account.username, accountSettings.account.password, get_account_server());
		url.Replace(_T("*"), _T("%"));
		url = msip_url_mask(url);
		//PJ_LOG(3, (THIS_FILENAME, "Begin UsersDirectoryLoad"));
		URLGetAsync(url, m_hWnd, UM_USERS_DIRECTORY
			, false
		);
	}
}

void CmainDlg::AccountSettingsPendingSave()
{
	KillTimer(IDT_TIMER_SAVE);
	SetTimer(IDT_TIMER_SAVE, 5000, NULL);
}

void CmainDlg::OnAccountChanged()
{
	TrayIconUpdateTip();
	pageDialer->RebuildButtons();
}

void CmainDlg::OpenTransferDlg(CWnd *pParent, msip_action action, pjsua_call_id call_id, Contact *selectedContact)
{
	if (mainDlg->transferDlg) {
		mainDlg->transferDlg->OnClose();
	}
	mainDlg->transferDlg = new Transfer(pParent);
	mainDlg->transferDlg->SetAction(action, call_id);
	mainDlg->transferDlg->LoadFromContacts(selectedContact);
	mainDlg->transferDlg->SetForegroundWindow();
}

void CmainDlg::OnCheckUpdates()
{
	updateCheckerShow = true;
	CheckUpdates();
}

void CmainDlg::CheckUpdates()
{
	CString url;
	url = _T("http://update.microsip.org/softphone-update.txt");
	url.AppendFormat(_T("?version=%s&client=%s"), _T(_GLOBAL_VERSION), CString(urlencode(Utf8EncodeUcs2(_T(_GLOBAL_NAME_NICE)))));
#ifndef _GLOBAL_VIDEO
	url.Append(_T("&lite=1"));
#endif
	URLGetAsync(url, m_hWnd, UM_UPDATE_CHECKER_LOADED);
}

LRESULT CmainDlg::OnUpdateCheckerLoaded(WPARAM wParam, LPARAM lParam)
{
	bool found = false;
	URLGetAsyncData *response = (URLGetAsyncData *)wParam;
	if (response->statusCode == 200) {
		if (!response->body.IsEmpty() && response->body.Left(4) == "http") {
			int pos = response->body.Find("\n");
			if (pos > 0) {
				CStringA url = response->body.Left(pos);
				url.Trim();
				bool allowed = false;
				DWORD dwServiceType;
				CString strServer;
				CString strObject;
				INTERNET_PORT nPort;
				if (AfxParseURL(CString(url), dwServiceType, strServer, strObject, nPort) && dwServiceType == AFX_INET_SERVICE_HTTPS && strServer.Right(13) == _T(".microsip.org")) {
					allowed = true;
				}
				int pos1 = response->body.Find("\n", pos + 1);
				if (allowed && pos1 > pos) {
					CStringA version = response->body.Mid(pos, pos1 - pos);
					version.Trim();
					CString info = MSIP::Utf8DecodeUni(response->body.Mid(pos1 + 1));
					info.Trim();
					CStringA our = _GLOBAL_VERSION;
					int count = version.Replace(".", ".");
					if (count < 4) {
						int i = count;
						while (i < 3) {
							version.Append(".0");
							i++;
						}
						count = our.Replace(".", ".");
						i = count;
						while (i < 3) {
							our.Append(".0");
							i++;
						}
						unsigned long ia = inet_addr(version.GetBuffer());
						if (ia != -1 && htonl(ia) > htonl(inet_addr(our.GetBuffer()))) {
							CString caption;
							caption.Format(_T("%s %s"), _T(_GLOBAL_NAME_NICE), Translate(_T("Update Available")));
							CString message = Translate(_T("Do you want to update now?"));
							if (!info.IsEmpty()) {
								message.AppendFormat(_T("\r\n\r\n%s"), info);
							}
							found = true;
							if (::MessageBox(this->m_hWnd, message, caption, MB_YESNO | MB_ICONQUESTION) == IDYES) {
								MSIP::OpenURL(MSIP::Utf8DecodeUni(url));
							}
						}
					}
				}
			}
		}
	}
	delete response;
	if (updateCheckerShow && !found) {
		MessageBox(_T("No new version found"), _T(""), MB_ICONINFORMATION);
	}
	updateCheckerShow = false;
	return 0;
}

#ifdef _GLOBAL_VIDEO
int CmainDlg::VideoCaptureDeviceId(CString name)
{
	unsigned count = PJMEDIA_VID_DEV_MAX_DEVS;
	pjmedia_vid_dev_info vid_dev_info[PJMEDIA_VID_DEV_MAX_DEVS];
	pjsua_vid_enum_devs(vid_dev_info, &count);
	for (unsigned i = 0; i < count; i++) {
		if (vid_dev_info[i].fmt_cnt && (vid_dev_info[i].dir == PJMEDIA_DIR_ENCODING || vid_dev_info[i].dir == PJMEDIA_DIR_ENCODING_DECODING)) {
			CString vidDevName = MSIP::PjStrToWideChar(vid_dev_info[i].name);
			if ((!name.IsEmpty() && name == vidDevName)
				||
				(name.IsEmpty() && accountSettings.videoCaptureDevice == vidDevName)) {
				return vid_dev_info[i].id;
			}
		}
	}
	return PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
}

void CmainDlg::createPreviewWin()
{
	if (!previewWin) {
		previewWin = new Preview(this);
	}
	previewWin->Start(VideoCaptureDeviceId());
}
#endif

void CmainDlg::SubsribeNumber(CString *number)
{
	if (!isSubscribed) {
		return;
	}
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	CString commands;
	CString numberFormated = FormatNumber(*number, &commands, true);

	pjsua_buddy_id ids[PJSUA_MAX_BUDDIES];
	unsigned count = PJSUA_MAX_BUDDIES;
	pjsua_enum_buddies(ids, &count);
	for (unsigned i = 0; i < count; i++) {
		CString* buddyNumber = (CString*)pjsua_buddy_get_user_data(ids[i]);
		if (*buddyNumber == numberFormated) {
			onBuddyState(ids[i], 0);
			return;
		}
	}
	CString numberFormatedPresence = numberFormated;
	pj_status_t status = msip_verify_sip_url(MSIP::StrToPj(numberFormatedPresence));
	if (status == PJ_SUCCESS) {
		pjsua_acc_id acc_id;
		pj_str_t pj_uri;
		if (SelectSIPAccount(numberFormatedPresence, acc_id, pj_uri)) {
			pjsua_buddy_id p_buddy_id;
			pjsua_buddy_config buddy_cfg;
			pjsua_buddy_config_default(&buddy_cfg);
			buddy_cfg.subscribe = PJ_TRUE;
			buddy_cfg.uri = pj_uri;
			buddy_cfg.user_data = (void*)(new CString(numberFormated));
			status = pjsua_buddy_add(&buddy_cfg, &p_buddy_id);
		}
		}
	if (status != PJ_SUCCESS) {
		CString str;
		str.Format(_T("%s\r\n%s"), Translate(_T("Presence Subscription")), *number);
		CString message = MSIP::GetErrorMessage(status);
		BaloonPopup(str, Translate(message.GetBuffer()), NIIF_INFO);
	}
}

void CmainDlg::UnsubscribeNumber(CString* number)
{
	if (!isSubscribed) {
		return;
	}
	if (pjsua_var.state != PJSUA_STATE_RUNNING) {
		return;
	}
	CString commands;
	CString numberFormated = FormatNumber(*number, &commands, true);

	pjsua_buddy_id ids[PJSUA_MAX_BUDDIES];
	unsigned count = PJSUA_MAX_BUDDIES;
	pjsua_enum_buddies(ids, &count);
	for (unsigned i = 0; i < count; i++) {
		CString* buddyNumber = (CString*)pjsua_buddy_get_user_data(ids[i]);
		if (*buddyNumber == numberFormated) {
			bool found1 = false;
			bool found2 = false;
			if (pageContacts->FindContact(numberFormated, true)) {
				found1 = true;
		}
			if (!found1) {
				for (int i = 0; i < shortcuts.GetCount(); i++) {
					Shortcut* shortcut = &shortcuts.GetAt(i);
					if (shortcut->presence) {
						CString commands;
						if (numberFormated == FormatNumber(shortcut->number, &commands, true)) {
							found2 = true;
							break;
						}
					}
				}
			}
			if (!found1 && !found2) {
				pjsua_buddy_del(ids[i]);
			}
			return;
		}
	}
}

void CmainDlg::Subscribe()
{
	if (isSubscribed) {
		return;
	}
	isSubscribed = true;
	pageContacts->PresenceSubscribe();
	pageDialer->PresenceSubscribe();
}

void CmainDlg::Unsubscribe()
{
	if (!isSubscribed) {
		return;
	}
	if (pjsua_var.state == PJSUA_STATE_RUNNING) {
		pjsua_buddy_id ids[PJSUA_MAX_BUDDIES];
		unsigned count = PJSUA_MAX_BUDDIES;
		pjsua_enum_buddies(ids, &count);
		for (unsigned i = 0; i < count; i++) {
			pjsua_buddy_del(ids[i]);
		}
	}
	pageContacts->PresenceReset();
	pageDialer->PresenceReset();
	isSubscribed = false;
}

