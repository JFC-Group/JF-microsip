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

#include "const.h"

#define STR_SZ 256

#define _GLOBAL_WIDTH 162

//#ifndef _GLOBAL_RINGIN_WIDTH
#define _GLOBAL_RINGIN_WIDTH 180
//#endif

#define _GLOBAL_DIALER_WIDTH 162

#define _GLOBAL_ICON "res\\microsip.ico"
#define _GLOBAL_ICON_INACTIVE "res\\inactive.ico"

#define _GLOBAL_HEIGHT1 0
#define _GLOBAL_HEIGHT2 _GLOBAL_HEIGHT1+16

#define _GLOBAL_HEIGHT3 _GLOBAL_HEIGHT2+23

#define _GLOBAL_HEIGHT4 _GLOBAL_HEIGHT3

#define _GLOBAL_HEIGHT_FINAL _GLOBAL_HEIGHT4

#define _GLOBAL_HEIGHT 192 + _GLOBAL_HEIGHT_FINAL

#define _GLOBAL_TAB_WIDTH 47

#define _GLOBAL_ACCT_OFFSET_LEFT 0

#define IDD_CALLS_OFFSET_INITIAL _GLOBAL_HEIGHT - 17
#define IDD_CALLS_OFFSET_LISTVIEW1 IDD_CALLS_OFFSET_INITIAL
#define IDD_CALLS_OFFSET_LISTVIEW2 IDD_CALLS_OFFSET_LISTVIEW1

#define IDD_CALLS_OFFSET_LISTVIEW IDD_CALLS_OFFSET_LISTVIEW2+2


#define _GLOBAL_CODECS_ENABLED "PCMA/8000/1 PCMU/8000/1"

#define _GLOBAL_SETT_DENYINC_DEFAULT "button"
#define _GLOBAL_SETT_AA_DEFAULT "button"

#define _GLOBAL_BUSINESS_FEATURE "This feature is not available in the free version."
#define _GLOBAL_MENU_WEBSITE "http://www.microsip.org/"
#define _GLOBAL_MENU_HELP "http://www.microsip.org/help"
#define _GLOBAL_HELP_WEBSITE "http://www.microsip.org/help"

#define _GLOBAL_EC_DEFAULT "1"

#define _GLOBAL_NAME_NICE _GLOBAL_NAME

#define _GLOBAL_CALL_PICKUP "**"

#define _GLOBAL_SHORTCUTS
#define _GLOBAL_SHORTCUTS_QTY 8

#define MACRO_ENABLE_LOCAL_ACCOUNT (accountSettings.enableLocalAccount || !accountSettings.accountId)

#define MACRO_SILENT (accountSettings.silent || accountSettings.hidden)

#define _GLOBAL_SUBSCRIBE

#ifndef _GLOBAL_VIDEO
#endif

#define _GLOBAL_SETT_HIDDEN_VALUE "0"

#ifndef LVS_EX_AUTOSIZECOLUMNS
#define LVS_EX_AUTOSIZECOLUMNS 0x10000000
#endif

#ifndef _GLOBAL_VIDEO
#endif

#define _GLOBAL_DIALER_CALL_COLOR RGB(76, 217, 100)
#define _GLOBAL_DIALER_END_COLOR RGB(255, 59, 48)
