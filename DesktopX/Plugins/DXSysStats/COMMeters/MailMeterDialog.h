/*
 * SysStats Widget Framework
 * Copyright (C) 2002-2006 Paul Andrews
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// MailMeterDialog.h : Declaration of the MailMeterDialog

#ifndef __MAILMETERDIALOG_H_
#define __MAILMETERDIALOG_H_

#include "Dialog.h"

/////////////////////////////////////////////////////////////////////////////
// MailMeterDialog

class CMailMeter;

class MailMeterDialog : public Dialog
{
public:
	MailMeterDialog(CMailMeter *pMeter, WORD idd=IDD_MAILMETER);
	virtual ~MailMeterDialog();

protected:
	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

	CMailMeter *pMeter;

public:
	BEGIN_MSG_MAP(MailMeterDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		CHAIN_MSG_MAP(Dialog)
	END_MSG_MAP()
};
#endif // __MAILMETERDIALOG_H_
