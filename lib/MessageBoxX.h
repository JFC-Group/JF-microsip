/*
Module : MessageBoxX.h
Purpose: Defines the interface for a MFC wrapper class for the MessageBox API
Created: PJN / 18-04-2003

Copyright (c) 2003 - 2020 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


/////////////////////////////// Macros / Defines //////////////////////////////

#pragma once

#ifndef __MESSAGEBOXX_H__
#define __MESSAGEBOXX_H__

#ifndef CMESSAGEBOXX_EXT_CLASS
#define CMESSAGEBOXX_EXT_CLASS
#endif //#ifndef CMESSAGEBOXX_EXT_CLASS

/////////////////////////////// Includes //////////////////////////////////////

#ifndef NO_CMESSAGEBOXX_HYPERLINK
#include <afxlinkctrl.h>
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK

#define _NODISCARD

#include <array>


/////////////////////////////// Classes ///////////////////////////////////////

class CMESSAGEBOXX_EXT_CLASS CMessageBoxX : public CWnd
{
public: 
//Constructors / Destructors
  CMessageBoxX() noexcept;
  CMessageBoxX(_In_opt_z_ LPCTSTR lpszText, _In_opt_z_ LPCTSTR lpszCaption = nullptr, _In_ UINT nType = MB_OK, _In_ UINT nIDHelp = 0);
  CMessageBoxX(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpszCaption = nullptr, _In_ UINT nType = MB_OK, _In_ UINT nIDHelp = -1);

//Methods
  INT_PTR DoModal();

//Accessors / Mutators
  void SetCaption(_In_opt_z_ LPCTSTR lpszCaption);
  BOOL SetCaption(_In_ UINT nIDResource);
  void SetCaption(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1);
  void SetCaption(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_opt_z_ LPCTSTR lpsz2);
  void SetText(_In_opt_z_ LPCTSTR lpszText, _In_ UINT nIDHelp = 0);
  BOOL SetText(_In_ UINT nIDResource, _In_ UINT nIDHelp = -1);
  void SetText(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_ UINT nIDHelp = -1);
  void SetText(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_opt_z_ LPCTSTR lpsz2, _In_ UINT nIDHelp = -1);
  void SetUserIcon(_In_ UINT nIDResource, _In_ HINSTANCE hInstance = AfxGetResourceHandle());
  void SetUserIcon(_In_opt_z_ LPCTSTR lpszResourceName, _In_ HINSTANCE hInstance = AfxGetResourceHandle());
  void SetType(_In_ UINT nType) noexcept;
  void SetTimeout(_In_ int nTimeout, _In_ int nTimeoutButtonId = IDCANCEL, _In_ BOOL bModifyCaptionDuringCountdown = TRUE, _In_ BOOL bResetTimeoutOnUserActivity = FALSE) noexcept;
  void SetDisabledButton(_In_ int nTimeout, _In_ int nDisabledButtonId = IDOK, _In_ BOOL bModifyCaptionDuringCountdown = TRUE) noexcept;
  void SetCheckBox(_In_ BOOL bChecked, _In_opt_z_ LPCTSTR lpszText);
  void SetCheckBoxState(_In_ BOOL bChecked) noexcept { m_bCheckBoxState = bChecked; };
  _NODISCARD BOOL GetCheckBoxState() const noexcept { return m_bCheckBoxState; };
#ifndef NO_CMESSAGEBOXX_HYPERLINK
  void SetHyperlink(_In_ BOOL bHyperlink, _In_opt_z_ LPCTSTR lpszCaption, _In_opt_z_ LPCTSTR lpszURL);
  _NODISCARD BOOL GetHyperlink() const noexcept { return m_bHyperlink; };
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK
  void SetRightAlignButtons(_In_ BOOL bRightAlign) noexcept;
  void SetSelectableText(_In_ BOOL bSelectableText) noexcept;
  _NODISCARD BOOL GetSelectableText() const noexcept { return m_bSelectableText; };
  void SetModifiedButtonCaption(_In_ int nButtonIndex, _In_opt_z_ LPCTSTR pszNewCaption);
  _NODISCARD CString GetModifiedButtonCaption(_In_ int nButtonIndex);
  void SetAnimatedText(_In_ DWORD dwAnimationInterval, _In_ const CStringArray* pTexts, _In_ BOOL bLoop);
  void SetAnimatedCaption(_In_ DWORD dwAnimationInterval, _In_ const CStringArray* pCaptions, _In_ BOOL bLoop);

protected:
//Member variables
  CString m_sCaption;
  CString m_sText;
  UINT m_nHelpIDPrompt;
  LPCTSTR m_pszIcon;
  HINSTANCE m_hIconInstance;
  HWND m_hParentWnd;
  UINT m_nType;
  int m_nTimerTimeout;
  int m_nTimerButtonId;
  int m_nTimerCountdown;
  BOOL m_bModifyCaptionForTimerCountdown;
  BOOL m_bResetTimeoutOnUserActivity;
  int m_nDisabledTimeout;
  int m_nDisabledButtonId;
  BOOL m_bModifyCaptionForDisabledCountdown;
  int m_nDisabledCountdown;
  UINT_PTR m_nTimeoutDisabledButtonTimerID;
  UINT_PTR m_nAnimatedTextTimerID;
  UINT_PTR m_nAnimatedCaptionTimerID;
  DWORD m_dwTextAnimationInterval;
  const CStringArray* m_pAnimatedTexts;
  BOOL m_bAnimatedTextLoop;
  INT_PTR m_nCurrentAnimatedTextIndex;
  DWORD m_dwCaptionAnimationInterval;
  const CStringArray* m_pAnimatedCaptions;
  BOOL m_bAnimatedCaptionLoop;
  int m_nCurrentAnimatedCaptionIndex;
  BOOL m_bCheckBox;
  BOOL m_bCheckBoxState;
  CString m_sCheckBoxText;
  CButton m_wndCheckBox;
#ifndef NO_CMESSAGEBOXX_HYPERLINK
  BOOL m_bHyperlink;
  CString m_sHyperlinkCaption;
  CString m_sHyperlinkURL;
  CMFCLinkCtrl m_wndHyperlink;
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK
  BOOL m_bRightAlignButtons;
  BOOL m_bSelectableText;
  CEdit m_wndSelectableText;
  std::array<CString, 4> m_ModifiedButtonCaptions; //Contains the captions we applied to the modified buttons

//Methods
  virtual BOOL InstallTimeoutDisabledTimeout();
  virtual BOOL InstallAnimatedTextTimer();
  virtual BOOL InstallAnimatedCaptionTimer();
  virtual void DoTimeoutDisabledButtonUpdate();
  virtual void DoAnimatedTextUpdate();
  virtual void DoAnimatedCaptionUpdate();
  virtual CSize GetCheckBoxSize(_In_ CFont* pFont);
#ifndef NO_CMESSAGEBOXX_HYPERLINK
  virtual BOOL CreateHyperlink();
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK
  virtual BOOL CreateCheckBox();
  virtual void RightAlignButtons();
  virtual void ModifyButtonCaptions();
  virtual CString GetCurrentTimerCaption();
  virtual CString GetCurrentDisabledCaption();
  CRect GetRightMostButtonRect();
  CWnd* FindTextControl();
  CWnd* FindOurTextControl();
  CWnd* GetFirstButton();
  virtual BOOL OnInitDone(); //virtual function similiar to the CFileDialog method of the same name
  LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

//Message handlers
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg LRESULT OnInitDialog(WPARAM wParam, LPARAM lParam);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnDestroy();

  DECLARE_MESSAGE_MAP()
};

#endif //#ifndef __MESSAGEBOXX_H__
