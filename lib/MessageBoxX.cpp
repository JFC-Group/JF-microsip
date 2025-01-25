/*
Module : MessageBoxX.cpp
Purpose: Implementation for a MFC wrapper class for the MessageBox API
Created: PJN / 18-04-2003
History: PJN / 24-04-2006 1. Updated copyright details.
                          2. UpdateCaptionDueToTimer method has been renamed to UpdateUIDueToTimer
                          3. Now includes support for including a check box on the MessageBox. To achieve this
                          simply call SetCheckBox with the text to show on the check box and its initial 
                          checked state prior to calling DoModal to show the message box. Then when DoModal
                          returns simple call GetCheckBoxState to determine whether the check box was ticked
                          or not.
                          4. Now includes support for right aligning buttons instead of the default for the 
                          MessageBox API which is to have the buttons centered. The support for checkboxes
                          and right aligned buttons is inspired by the GSMessageBox article at 
                          http://www.gipsysoft.com/messagebox/.
                          5. Both SetTimeout and SetDisabledButton methods now include a "bModifyCaptionDuringCountdown"
                          which determines if they should cause the MessageBox caption to be modified during their
                          timer countdowns
                          6. Updated the sample app to allow most of the functionality of the code to be optionally
                          tested.
                          7. SetTimeout method now includes a bResetTimeoutOnUserActivity parameter.
                          8. Addition of GetCurrentTimerCaption() and GetCurrentDisabledCaption() virtual methods
                          which allow the timeout and disabled captions to be easily customized at runtime.
                          9. Fixed a number of compiler warnings when the code is compiled using /Wp64
         PJN / 02-07-2006 1. Code now uses newer C++ style casts instead of C style casts.
                          2. Class now allows you to put HTML text into the message box. This is provided by the 
                          QHTM HTML control provided at http://www.gipsysoft.com/qhtm/. You may need to purchase a 
                          license (depending on the type of application you are developing) as well as follow their 
                          instructions on deploying it if you want this functionality with CMessageBoxX. This functionality 
                          by default is not included in CMessageBoxX and is only available when you define the 
                          preprocessor macro "CMESSAGEBOXX_HTML_SUPPORT".
.                         3. Implemented support for selectable text
                          4. Button captions can now be modified via SetModifiedButtonCaption.
                          5. Added user defined animated text support.
                          6. Added user defined animated caption support.
         PJN / 05-11-2006 1. Minor update to stdafx.h of sample app to avoid compiler warnings in VC 2005.
                          2. Code now uses QHTM const for the class name
         PJN / 11-08-2007 1. Updated copyright details.
                          2. Check box text control is now clipped to the width of the main text control if its calculated 
                          width will exceed the message box client area. This was done following a report from Kyle Alons 
                          that this text can get clipped on Vista when you pick a longish piece of text as the check box 
                          text. You should ensure you pick a shortish piece of text to avoid this problem as well as test 
                          all your message boxes on all the operating systems you will be supporting your application on.
                          3. Addition of a CMESSAGEBOXX_EXT_CLASS to allow the code to be more easily added to an 
                          extension dll.
         PJN / 11-07-2008 1. Updated copyright details.
                          2. Updated sample app to clean compile on VC 2008
                          3. Removed VC 6 style classwizard comments from the code.
                          4. The code has now been updated to support VC 2005 or later only. 
                          5. Code now compiles cleanly using Code Analysis (/analyze)
                          6. Code now uses newer C++ style casts instead of C style casts.
                          7. Updated sample app to demonstrate support for user icons
                          8. Class now correctly handles the manifest contexts which the MessageBox API's can have
                          when you are using Common Control v6 or later
         PJN / 18-05-2014 1. Updated copyright details.
                          2. Updated the code to compile cleanly on VC 2012 and later. Further changes were required to
                          achieve this because MFC in Visual Studio 2012 has been reworked to use the SDK style fusion 
                          wrappers rather than those in the now defunct afxcomctl32.h header file.
                          3. Updated the SetCaption method to operate where AfxGetApp() returns NULL.
                          4. Updated the project settings to more modern default values.
         PJN / 17-11-2015 1. Verified the code compiles cleanly on VC 2015.
                          2. Updated the sample apps main icon.
                          3. Updated copyright details.
         PJN / 19-07-2016 1. Updated copyright details.
                          2. Tidied up the implementations of GetFirstButton, GetRightMostButtonRect,
                          RightAlignButtons, CreateCheckBox, FixUpPositionsToSuitQHTM, ModifyButtonCaptions & 
                          GetCheckBoxSize.
                          3. Fixed a bug in the creation of the selectable text control in CMessageBoxX::OnInitDialog.
                          Thanks to Petr Stejskal for reporting this issue.
                          4. Removed two calls to VERIFY in CMessageBoxX::OnInitDialog. Instead now failure to create
                          the selectable text control or the QHTM control results in the message box failing with
                          IDCANCEL.
                          5. Added a BOOL return value to SetCaption, SetText & SetHTMLText methods which internally 
                          call LoadString.
                          6. Removed support for QHTM control from the code as it looks like the control is no longer
                          being developed and is not available from the author's web site at http://gipsysoft.com
                          7. Tested code operates correctly on High DPI monitors.
                          8. Added SAL annotations to all the code.
                          9. Added support for adding a hyperlink to the messagebox. Thanks to Petr Stejskal for 
                          requesting this feature. Please note that you should not create a messagebox which includes
                          both a checkbox and a hyperlink as you will get the hyperlink control appearing over the 
                          white / gray boundary on the message box which will look ugly. If you really want a more
                          complicated dialog which supports hyperlinks and checkboxes at the same time then please
                          check out the TaskDialog Windows API or the author's own XTaskDialog code at 
                          http://www.naughter.com/xtaskdialog.html which provides an emulation of Task Dialogs on 
                          versions of Windows prior to Vista.
                          10. The selectable text control is now created with the WS_TABSTOP style thus allowing the
                          text to be tabbed into via the keyboard.
         PJN / 01-07-2018 1. Updated copyright details.
                          2. Fixed a number of C++ core guidelines compiler warnings. These changes mean that the code
                          will now only compile on VC 2017 or later.
         PJN / 16-09-2018 1. Fixed a number of compiler warnings when using VS 2017 15.8.4
         PJN / 22-04-2019 1. Updated copyright details
                          2. Updated the code to clean compile on VC 2019
         PJN / 09-09-2019 1. Fixed a number of compiler warnings when the code is compiled with VS 2019 Preview
         PJN / 18-12-2019 1. Fixed various Clang-Tidy static code analysis warnings in the code.
         PJN / 28-01-2020 1. Updated copyright details.
                          2. Fixed more Clang-Tidy static code analysis warnings in the code.
         PJN / 12-04-2020 1. Fixed more Clang-Tidy static code analysis warnings in the code.

Copyright (c) 2003 - 2020 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code.

*/


//////////////// Includes ////////////////////////////////////////////

#include "StdAfx.h"
#include "MessageBoxX.h"
#ifndef __AFXPRIV_H__
#pragma message("To avoid this message, please put afxpriv.h in your pre compiled header (normally stdafx.h)")
#include <afxpriv.h>
#endif //#ifndef __AFXPRIV_H__


//////////////// Macros / Defines ////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#endif //#ifdef _DEBUG

#ifndef MB_CANCELTRYCONTINUE
#define MB_CANCELTRYCONTINUE 0x00000006L
#endif //#ifndef MB_CANCELTRYCONTINUE

constexpr const UINT MESSAGEBOXX_EDIT_ID = 101;
constexpr const UINT MESSAGEBOXX_CHECKBOX_ID = 102;
constexpr const UINT MESSAGEBOXX_HYPERLINK_ID = 103;
constexpr const UINT_PTR MESSAGEBOXX_ANIMATED_TEXT_TIMER = 2;
constexpr const UINT_PTR MESSAGEBOXX_ANIMATED_CAPTION_TIMER = 3;
constexpr const UINT_PTR MESSAGEBOXX_TIMEOUT_DISABLED_TIMER = 4;


//////////////// Implementation //////////////////////////////////////

//Simple class to allow access to CWinApp::m_dwPromptContext
class CMessageBoxPublicWinApp : public CWinApp
{
public:
  DWORD* GetPromptContextAddress() noexcept
  {
    return &m_dwPromptContext;
  }
};


CMessageBoxX::CMessageBoxX() noexcept : m_nHelpIDPrompt(0), 
                                        m_pszIcon(nullptr),
                                        m_hIconInstance(nullptr),
                                        m_hParentWnd(nullptr),
                                        m_nType(0),
                                        m_nTimerTimeout(0),
                                        m_nTimerButtonId(0),
                                        m_nTimerCountdown(0),
                                        m_bModifyCaptionForTimerCountdown(TRUE),
                                        m_bResetTimeoutOnUserActivity(FALSE),
                                        m_nDisabledTimeout(0),
                                        m_nDisabledButtonId(0),
                                        m_bModifyCaptionForDisabledCountdown(TRUE),
                                        m_nDisabledCountdown(0),
                                        m_nTimeoutDisabledButtonTimerID(0),
                                        m_nAnimatedTextTimerID(0),
                                        m_nAnimatedCaptionTimerID(0),
                                        m_dwTextAnimationInterval(0),
                                        m_pAnimatedTexts(nullptr),
                                        m_bAnimatedTextLoop(FALSE),
                                        m_nCurrentAnimatedTextIndex(0),
                                        m_dwCaptionAnimationInterval(0),
                                        m_pAnimatedCaptions(nullptr),
                                        m_bAnimatedCaptionLoop(FALSE),
                                        m_nCurrentAnimatedCaptionIndex(0),
                                        m_bCheckBox(FALSE),
                                        m_bCheckBoxState(FALSE),
                                      #ifndef NO_CMESSAGEBOXX_HYPERLINK
                                        m_bHyperlink(FALSE),
                                      #endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK
                                        m_bRightAlignButtons(FALSE),
                                        m_bSelectableText(FALSE)
{
}

CMessageBoxX::CMessageBoxX(_In_opt_z_ LPCTSTR lpszText, _In_opt_z_ LPCTSTR lpszCaption, _In_ UINT nType, _In_ UINT nIDHelp) : CMessageBoxX()
{
  SetText(lpszText, nIDHelp);
  SetCaption(lpszCaption);
  SetType(nType);
}

CMessageBoxX::CMessageBoxX(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpszCaption, _In_ UINT nType, _In_ UINT nIDHelp) : CMessageBoxX()
{
  SetText(nIDResource, nIDHelp);
  SetCaption(lpszCaption);
  SetType(nType);
}

CString CMessageBoxX::GetCurrentTimerCaption()
{
  //What will be the return value from this function
  CString sCaption;

  if (m_nTimerTimeout && (m_nTimerCountdown <= 10) && m_bModifyCaptionForTimerCountdown)
  {
    //By default we modify the caption with a countdown if there is less than 10 seconds to go before the timeout
   	sCaption.Format(_T("%s (Closing in %d seconds)"), m_sCaption, m_nTimerCountdown);
  }
  else
    sCaption = m_sCaption; //No change required

  return sCaption;
}

CString CMessageBoxX::GetCurrentDisabledCaption()
{
  //What will be the return value from this function
  CString sCaption;

  if (m_nDisabledTimeout && m_bModifyCaptionForDisabledCountdown)
  {
    //By default we always modify the caption if we have a disabled button (derived classes of course are free
    //to modify this behaviour)
	sCaption.Format(_T("%s (%d seconds remaining)"), m_sCaption, m_nDisabledCountdown);
  }
  else
    sCaption = m_sCaption; //No Change required

  return sCaption;
}

#pragma warning(suppress: 26433 26440)
BEGIN_MESSAGE_MAP(CMessageBoxX, CWnd) //NOLINT(modernize-avoid-c-arrays)
  ON_WM_CREATE()
  ON_WM_TIMER()
  ON_MESSAGE(WM_INITDIALOG, &CMessageBoxX::OnInitDialog)
  ON_WM_DESTROY()
END_MESSAGE_MAP()

#pragma warning(suppress: 26434)
int CMessageBoxX::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  //Unhook window creation
  AfxUnhookWindowCreate();

  //let the base class do its thing
  return __super::OnCreate(lpCreateStruct);
}

void CMessageBoxX::SetCaption(_In_opt_z_ LPCTSTR lpszCaption)
{
  if (lpszCaption != nullptr)
    m_sCaption = lpszCaption;
  else
  {
    //Use the default CWinApp caption or if a CWinApp instance is not available then use the exe name.
    //This logic is similair to the MFC CWinApp MessageBox implementation.
    const CWinApp* pApp = AfxGetApp();
    if (pApp != nullptr)
#pragma warning(suppress: 26486)
      m_sCaption = pApp->m_pszAppName;
    else
    {
      GetModuleFileName(nullptr, m_sCaption.GetBuffer(_MAX_PATH), _MAX_PATH);
      m_sCaption.ReleaseBuffer();
    }
  }
}

BOOL CMessageBoxX::SetCaption(_In_ UINT nIDResource)
{
  if (!m_sCaption.LoadString(nIDResource))
  {
    TRACE(_T("CMessageBoxX::SetCaption, Failed to load message box caption string 0x%04x.\n"),	nIDResource);
    return FALSE;
  }

  return TRUE;
}

void CMessageBoxX::SetCaption(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1)
{
  AfxFormatString1(m_sCaption, nIDResource, lpsz1);
}

void CMessageBoxX::SetCaption(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_opt_z_ LPCTSTR lpsz2)
{
  AfxFormatString2(m_sCaption, nIDResource, lpsz1, lpsz2);
}

void CMessageBoxX::SetText(_In_opt_z_ LPCTSTR lpszText, _In_ UINT nIDHelp)
{
  m_sText = lpszText;
  m_nHelpIDPrompt = nIDHelp;
}

BOOL CMessageBoxX::SetText(_In_ UINT nIDResource, _In_ UINT nIDHelp)
{
  if (!m_sText.LoadString(nIDResource))
  {
    TRACE(_T("CMessageBoxX::SetText, Error: failed to load message box prompt string 0x%04x.\n"), nIDResource);
    return FALSE;
  }

#pragma warning(suppress: 26472)
  if (nIDHelp == static_cast<UINT>(-1))
    m_nHelpIDPrompt = nIDResource;
  else
    m_nHelpIDPrompt = nIDHelp;

  return TRUE;
}

void CMessageBoxX::SetText(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_ UINT nIDHelp)
{
  AfxFormatString1(m_sText, nIDResource, lpsz1);

#pragma warning(suppress: 26472)
  if (nIDHelp == static_cast<UINT>(-1))
    m_nHelpIDPrompt = nIDResource;
  else
    m_nHelpIDPrompt = nIDHelp;
}

void CMessageBoxX::SetText(_In_ UINT nIDResource, _In_opt_z_ LPCTSTR lpsz1, _In_opt_z_ LPCTSTR lpsz2, _In_ UINT nIDHelp)
{
  AfxFormatString2(m_sText, nIDResource, lpsz1, lpsz2);

#pragma warning(suppress: 26472)
  if (nIDHelp == static_cast<UINT>(-1))
    m_nHelpIDPrompt = nIDResource;
  else
    m_nHelpIDPrompt = nIDHelp;
}

void CMessageBoxX::SetUserIcon(_In_ UINT nIDResource, _In_ HINSTANCE hInstance)
{
  //Pass the buck to the other function
  SetUserIcon(MAKEINTRESOURCE(nIDResource), hInstance);
}

void CMessageBoxX::SetSelectableText(_In_ BOOL bSelectableText) noexcept
{
  m_bSelectableText = bSelectableText;
}

void CMessageBoxX::SetModifiedButtonCaption(_In_ int nButtonIndex, _In_opt_z_ LPCTSTR pszNewCaption)
{
  //Hive the value away
#pragma warning(suppress: 26482 26446)
  m_ModifiedButtonCaptions[nButtonIndex] = pszNewCaption;
}

CString CMessageBoxX::GetModifiedButtonCaption(_In_ int nButtonIndex)
{
#pragma warning(suppress: 26482 26446)
  return m_ModifiedButtonCaptions[nButtonIndex];
}

void CMessageBoxX::SetAnimatedText(_In_ DWORD dwAnimationInterval, _In_ const CStringArray* pTexts, _In_ BOOL bLoop)
{
  m_dwTextAnimationInterval = dwAnimationInterval;
  m_pAnimatedTexts = pTexts;
  m_bAnimatedTextLoop = bLoop;

  m_nCurrentAnimatedTextIndex = 0; //Reset the index value
  if (pTexts != nullptr) //Set the initial text to the first item in the array
    SetText((*pTexts)[0].GetString());
}

void CMessageBoxX::SetAnimatedCaption(_In_ DWORD dwAnimationInterval, _In_ const CStringArray* pCaptions, _In_ BOOL bLoop)
{
  m_dwCaptionAnimationInterval = dwAnimationInterval;
  m_pAnimatedCaptions = pCaptions;
  m_bAnimatedCaptionLoop = bLoop;
  m_nCurrentAnimatedCaptionIndex = 0; //Reset the index value
  if (pCaptions != nullptr) //Set the initial caption to the first item in the array
    SetCaption((*pCaptions)[0].GetString());
}

void CMessageBoxX::SetUserIcon(_In_opt_z_ LPCTSTR lpszResourceName, _In_ HINSTANCE hInstance)
{
  m_pszIcon = lpszResourceName;
  m_hIconInstance = hInstance;
  m_nType |= MB_USERICON;
}

void CMessageBoxX::SetType(_In_ UINT nType) noexcept
{
  m_nType = nType;
}

void CMessageBoxX::SetTimeout(_In_ int nTimeout, _In_ int nTimeoutButtonId, _In_ BOOL bModifyCaptionDuringCountdown, _In_ BOOL bResetTimeoutOnUserActivity) noexcept
{
  m_nTimerTimeout = nTimeout;
  m_nTimerButtonId = nTimeoutButtonId;
  m_nTimerCountdown = m_nTimerTimeout;
  m_bModifyCaptionForTimerCountdown = bModifyCaptionDuringCountdown;
  m_bResetTimeoutOnUserActivity = bResetTimeoutOnUserActivity;
}

void CMessageBoxX::SetDisabledButton(_In_ int nTimeout, _In_ int nDisabledButtonId, _In_ BOOL bModifyCaptionDuringCountdown) noexcept
{
  m_nDisabledTimeout = nTimeout;
  m_nDisabledButtonId = nDisabledButtonId;
  m_nDisabledCountdown = nTimeout;
  m_bModifyCaptionForDisabledCountdown = bModifyCaptionDuringCountdown;
}

CSize CMessageBoxX::GetCheckBoxSize(_In_ CFont* pFont)
{
  //What will be the return value from this method
  CSize size;

  //We need a device context to calculate the required size of the check box
  CDC* pDC = GetDC();
  if (pDC != nullptr)
  {
    //Get the size of the text
    CFont* pOldFont = pDC->SelectObject(pFont);
    size = pDC->GetTextExtent(m_sCheckBoxText);
    pDC->SelectObject(pOldFont);

    //Add in the size of a check box plus some space for a border
    CRect rMargin(0, 0, 5, 0);
    MapDialogRect(GetSafeHwnd(), &rMargin);
    size.cx += GetSystemMetrics(SM_CXMENUCHECK) + rMargin.Width();
    size.cy = max(size.cy, GetSystemMetrics(SM_CYMENUCHECK));

#pragma warning(suppress: 26486)
    ReleaseDC(pDC);
  }

  return size;
}

void CMessageBoxX::SetCheckBox(_In_ BOOL bChecked, _In_opt_z_ LPCTSTR lpszText)
{
  m_bCheckBox = TRUE;
  m_bCheckBoxState = bChecked;
  m_sCheckBoxText = lpszText;
}

#ifndef NO_CMESSAGEBOXX_HYPERLINK
BOOL CMessageBoxX::CreateHyperlink()
{
  //Determine the position of the hyperlink, In the process increasing the height of the message box to accommodate it
  const CWnd* pTextControl = FindOurTextControl();
#pragma warning(suppress: 26496)
  AFXASSUME(pTextControl != nullptr);
  CRect rectStatic;
  pTextControl->GetWindowRect(&rectStatic);
  ScreenToClient(&rectStatic);
  CPoint pt(rectStatic.left, rectStatic.bottom);

  //Also factor in the check box if it exists
  CRect rectCheckbox;
  if (m_wndCheckBox.GetSafeHwnd())
  {
    m_wndCheckBox.GetWindowRect(&rectCheckbox);
    ScreenToClient(&rectCheckbox);
    if (rectCheckbox.bottom > pt.y)
      pt.y = rectCheckbox.bottom;
  }

  CRect rMargin(0, 0, 0, 2);
  MapDialogRect(GetSafeHwnd(), &rMargin);
  const int nHyperlinkDeltaY = rMargin.Height();
  pt.y += nHyperlinkDeltaY;
  const CRect rectHyperlink(pt.x, pt.y, pt.x + 1, pt.y + 1); //The actual size of the hyperlink control does not really matter at this stage since we will be calling SizeToContent later on in this method

  //Get the position of the first button to see if we need to move the buttons down to accommodate the new 
  const CWnd* pButton = GetFirstButton();
#pragma warning(suppress: 26496)
  AFXASSUME(pButton != nullptr);
  CRect rectButton;
  pButton->GetWindowRect(&rectButton);
  ScreenToClient(&rectButton);

  //See if we need to move all the buttons down a bit
  if (rectButton.top < (rectHyperlink.bottom + nHyperlinkDeltaY))
  {
    //First we need to increase the height of the message box by the extra height
    const int nExtraHeight = rectHyperlink.bottom + nHyperlinkDeltaY - rectButton.top;
    CRect rectWindow;
    GetWindowRect(&rectWindow);
    CRect rectNewWindow(rectWindow);
    rectNewWindow.bottom += nExtraHeight;
    SetWindowPos(nullptr, rectNewWindow.left, rectNewWindow.top, rectNewWindow.Width(), rectNewWindow.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    //Now move all the buttons on the message box down by the extra height
    CWnd* pChildWnd = GetWindow(GW_CHILD);
    while (pChildWnd != nullptr)
    {
      HWND hChildWnd = pChildWnd->GetSafeHwnd();
      CString sClass;
      const int nGCN = GetClassName(hChildWnd, sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
      sClass.ReleaseBuffer();
      if (nGCN)
      {
        if (sClass.CompareNoCase(WC_BUTTON) == 0)
        {
          //move the control down
          pChildWnd->GetWindowRect(&rectButton);
          ScreenToClient(&rectButton);
          rectButton.OffsetRect(0, nExtraHeight);
          pChildWnd->SetWindowPos(nullptr, rectButton.left, rectButton.top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
        }
      }
      pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
    }
  }

  //Create the hyperlink
  if (!m_wndHyperlink.Create(m_sHyperlinkCaption, WS_TABSTOP | WS_CHILD | WS_VISIBLE, rectHyperlink, this, MESSAGEBOXX_HYPERLINK_ID))
    return FALSE;
  m_wndHyperlink.SetURL(m_sHyperlinkURL);
  m_wndHyperlink.SizeToContent(FALSE, FALSE);

  return TRUE;
}

void CMessageBoxX::SetHyperlink(_In_ BOOL bHyperlink, _In_opt_z_ LPCTSTR lpszCaption, _In_opt_z_ LPCTSTR lpszURL)
{
  m_bHyperlink = bHyperlink;
  m_sHyperlinkCaption = lpszCaption;
  m_sHyperlinkURL = lpszURL;
}
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK

void CMessageBoxX::SetRightAlignButtons(_In_ BOOL bRightAlign) noexcept
{
  m_bRightAlignButtons = bRightAlign;
}

CWnd* CMessageBoxX::FindTextControl()
{
  //Find the static control which houses the MessageBox text
  CWnd* pWndStatic = GetWindow(GW_CHILD);
  while (pWndStatic)
  {
    const int nID = pWndStatic->GetDlgCtrlID();
    if (nID >= 0xFFFF)
      return pWndStatic;
    pWndStatic = pWndStatic->GetWindow(GW_HWNDNEXT);
  }

  return nullptr;
}

CWnd* CMessageBoxX::FindOurTextControl()
{
  if (IsWindow(m_wndSelectableText.GetSafeHwnd()))
    return &m_wndSelectableText;
  else
    return FindTextControl();
}

CWnd* CMessageBoxX::GetFirstButton()
{
  //Find the first button on the message box
  CWnd* pChildWnd = GetWindow(GW_CHILD);
  while (pChildWnd != nullptr)
  {
    CString sClass;
    const int nGCN = GetClassName(pChildWnd->GetSafeHwnd(), sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
    sClass.ReleaseBuffer();
    if (nGCN)
    {
      if (sClass.CompareNoCase(WC_BUTTON) == 0)
#pragma warning(suppress: 26487)
        return pChildWnd;
    }
    pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
  }

  return nullptr;
}

CRect CMessageBoxX::GetRightMostButtonRect()
{
  //What will be the return value from this function
  CRect rectButton;

  //Find the first most button
  CWnd* pChildWnd = GetWindow(GW_CHILD);
  constexpr const int nRightMostXValue = INT_MIN;
  while (pChildWnd != nullptr)
  {
    CString sClass;
    const int nGCN = GetClassName(pChildWnd->GetSafeHwnd(), sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
    sClass.ReleaseBuffer();
    if (nGCN)
    {
      if (sClass.CompareNoCase(WC_BUTTON) == 0)
      {
        CRect rectThisButton;
        pChildWnd->GetWindowRect(rectThisButton);
        if (rectThisButton.right > nRightMostXValue)
          rectButton = rectThisButton;
      }
    }
    pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
  }

  return rectButton;
}

void CMessageBoxX::RightAlignButtons()
{
  //Get the right most button position
  const CRect rectRightButton(GetRightMostButtonRect());

  //Work out how much we need to move each button to get them right aligned
  CRect rectDialogUnits(0, 0, 9, 0);
  MapDialogRect(GetSafeHwnd(), &rectDialogUnits);
  CRect rectWindow;
  GetWindowRect(&rectWindow);
  const int nDeltaX = rectWindow.right - rectDialogUnits.Width() - rectRightButton.right;
  if (nDeltaX)
  {
    //Now move all the buttons on the message box to get them right aligned
    CWnd* pChildWnd = GetWindow(GW_CHILD);
    while (pChildWnd != nullptr)
    {
      CString sClass;
      const int nGCN = GetClassName(pChildWnd->GetSafeHwnd(), sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
      sClass.ReleaseBuffer();
      if (nGCN)
      {
        if (sClass.CompareNoCase(WC_BUTTON) == 0)
        {
          //Aha we have found a button, move it over horizontally!
          CRect rectButton;
          pChildWnd->GetWindowRect(&rectButton);
          ScreenToClient(&rectButton);
          rectButton.OffsetRect(nDeltaX, 0);
          pChildWnd->SetWindowPos(nullptr, rectButton.left, rectButton.top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
        }
      }
      pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
    }
  }
}

BOOL CMessageBoxX::CreateCheckBox()
{
  //Get the size of the check boxes to create
  CFont* pFont = GetFont();
  const CSize sizeCheckBox(GetCheckBoxSize(pFont));

  //Determine the position of the check box, In the process increasing the height of the message box to accommodate it
  const CWnd* pTextControl = FindOurTextControl();
#pragma warning(suppress: 26496)
  AFXASSUME(pTextControl != nullptr);
  CRect rectStatic;
  pTextControl->GetWindowRect(&rectStatic);
  ScreenToClient(&rectStatic);
  CPoint pt(rectStatic.left, rectStatic.bottom);
  CRect rMargin(0, 0, 0, 3);
  MapDialogRect(GetSafeHwnd(), &rMargin);
  const int nCheckBoxDeltaY = rMargin.Height();
  pt.y += nCheckBoxDeltaY;
  CRect rectCheckBox(pt.x, pt.y, pt.x + sizeCheckBox.cx, pt.y + sizeCheckBox.cy);
  
  //Clip the check box to the text if necessary
  if (rectCheckBox.right > rectStatic.right)
    rectCheckBox.right = rectStatic.right;

  //Get the position of the first button to see if we need to move the buttons
  //down to accommodate the new checkbox
  const CWnd* pButton = GetFirstButton();
#pragma warning(suppress: 26496)
  AFXASSUME(pButton != nullptr);
  CRect rectButton;
  pButton->GetWindowRect(&rectButton);
  ScreenToClient(&rectButton);

  //See if we need to move all the buttons down a bit
  if (rectButton.top < (rectCheckBox.bottom + nCheckBoxDeltaY))
  {
    //First we need to increase the height of the message box by the extra height
    const int nExtraHeight = rectCheckBox.bottom + nCheckBoxDeltaY - rectButton.top;
    CRect rectWindow;
    GetWindowRect(&rectWindow);  
    CRect rectNewWindow(rectWindow);
    rectNewWindow.bottom += nExtraHeight;
    SetWindowPos(nullptr, rectNewWindow.left, rectNewWindow.top, rectNewWindow.Width(), rectNewWindow.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    //Now move all the buttons on the message box down by the extra height
    CWnd* pChildWnd = GetWindow(GW_CHILD);
    while (pChildWnd != nullptr)
    {
      CString sClass;
      const int nGCN = GetClassName(pChildWnd->GetSafeHwnd(), sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
      sClass.ReleaseBuffer();
      if (nGCN)
      {
        if (sClass.CompareNoCase(WC_BUTTON) == 0)
        {
          //move the control down
          pChildWnd->GetWindowRect(&rectButton);
          ScreenToClient(&rectButton);
          rectButton.OffsetRect(0, nExtraHeight);
          pChildWnd->SetWindowPos(nullptr, rectButton.left, rectButton.top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
        }
      }
      pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
    }
  }

  //Create the checkbox
  if (!m_wndCheckBox.Create(m_sCheckBoxText, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rectCheckBox, this, MESSAGEBOXX_CHECKBOX_ID))
    return FALSE;
  m_wndCheckBox.SetFont(pFont);
  if (m_bCheckBoxState)
    m_wndCheckBox.SetCheck(1);

  return TRUE;
}

void CMessageBoxX::ModifyButtonCaptions()
{
  //Iterate thro all the button controls, the order in which they are returned here corresponds to the index in the "m_sModifiedButtonCaptions" array
  CWnd* pChildWnd = GetWindow(GW_CHILD);
  size_t nCurrentButtonIndex = 0;
  while (pChildWnd)
  {
    CString sClass;
    const int nGCN = GetClassName(pChildWnd->GetSafeHwnd(), sClass.GetBuffer(_MAX_PATH), _MAX_PATH);
    sClass.ReleaseBuffer();
    if (nGCN)
    {
      //Have we found a button?
      if (sClass.CompareNoCase(WC_BUTTON) == 0)
      {
        ASSERT(nCurrentButtonIndex < m_ModifiedButtonCaptions.size());

        //Yes, then modify its caption if required
#pragma warning(suppress: 26482 26446)
        if (m_ModifiedButtonCaptions[nCurrentButtonIndex].GetLength())
          pChildWnd->SetWindowText(m_ModifiedButtonCaptions[nCurrentButtonIndex]);

        //Increment the button index counter
        ++nCurrentButtonIndex;
      }
    }
    pChildWnd = pChildWnd->GetWindow(GW_HWNDNEXT);
  }
}

LRESULT CMessageBoxX::OnInitDialog(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
  //Call the default window proc
  const LRESULT lRet = Default();

  //install the timer and disable the button if necessary
  if (m_nTimerTimeout || m_nDisabledTimeout)
  {
    //Install a timer if we need to timeout or a disabled button
    InstallTimeoutDisabledTimeout();

    //Do we need to disable the relevent button
    if (m_nDisabledTimeout)
    {
      CWnd* pWnd = GetDlgItem(m_nDisabledButtonId);
#pragma warning(suppress: 26496)
      AFXASSUME(pWnd != nullptr);
      pWnd->EnableWindow(FALSE);
    }
  }

  //Should we do text animation?
  if (m_dwTextAnimationInterval)
    InstallAnimatedTextTimer();

  //Should we do caption animation?
  if (m_dwCaptionAnimationInterval)
    InstallAnimatedCaptionTimer();

  //Update the button captions
  ModifyButtonCaptions();

  //Right align the buttons if necessary
  if (m_bRightAlignButtons)
    RightAlignButtons();

  //Should we create the text as selectable
  if (m_bSelectableText)
  {
    //Hide the message text
    CWnd* pWndText = FindTextControl();
#pragma warning(suppress: 26496)
    AFXASSUME(pWndText != nullptr);
    pWndText->ShowWindow(SW_HIDE);

    //And in its place create the edit control
    CRect rectText;
    pWndText->GetWindowRect(&rectText);
    ScreenToClient(&rectText);

    //Create the control
    rectText.InflateRect(2, 0, 2, 0); //We need this extra space on the left and right because the edit control uses an internal horizontal margin 
                                      //of a few pixels for the drawing of its text
    if (!m_wndSelectableText.Create(WS_TABSTOP | WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE, rectText, this, MESSAGEBOXX_EDIT_ID))
    {
      TRACE(_T("CMessageBoxX::OnInitDialog, Error: failed to create selectable text control. GetLastError:0x%08X\n"), GetLastError());
      EndDialog(GetSafeHwnd(), IDCANCEL);
    }
    else
    {
      //Set its text and font
      CString sEditText(m_sText);
      sEditText.Replace(_T("\r\n"), _T("MSGBOXX_EOL"));
      sEditText.Replace(_T("\n"), _T("\r\n"));
      sEditText.Replace(_T("MSGBOXX_EOL"), _T("\r\n"));
      m_wndSelectableText.SetWindowText(sEditText);
      m_wndSelectableText.SetFont(GetFont());
    }
  }

  //Create the check box if necessary. Note it is important that we create the checkbox after the right align code 
  //above so as that code does not include the checkbox in its calculation logic!
  if (m_bCheckBox)
    CreateCheckBox();

#ifndef NO_CMESSAGEBOXX_HYPERLINK
  //Create the hyperlink if necessary
  if (m_bHyperlink)
    CreateHyperlink();
#endif //#ifndef NO_CMESSAGEBOXX_HYPERLINK

  //Call the C++ virtual OnInitDone implementation
  OnInitDone();

  return lRet;
}

#pragma warning(suppress: 26440)
BOOL CMessageBoxX::OnInitDone()
{
  //Derived classes may want to do something more interesting
  return TRUE;
}

BOOL CMessageBoxX::InstallAnimatedTextTimer()
{
  //validate our parameters
  ASSERT(m_nAnimatedTextTimerID == 0);

  //Install the timeout / disabled button timer
  m_nAnimatedTextTimerID = SetTimer(MESSAGEBOXX_ANIMATED_TEXT_TIMER, m_dwTextAnimationInterval, nullptr);

  return (m_nAnimatedTextTimerID != 0);
}

BOOL CMessageBoxX::InstallAnimatedCaptionTimer()
{
  //validate our parameters
  ASSERT(m_nAnimatedCaptionTimerID == 0);

  //Install the timeout / disabled button timer
  m_nAnimatedCaptionTimerID = SetTimer(MESSAGEBOXX_ANIMATED_CAPTION_TIMER, m_dwCaptionAnimationInterval, nullptr);

  return (m_nAnimatedCaptionTimerID != 0);
}

BOOL CMessageBoxX::InstallTimeoutDisabledTimeout()
{
  //Validate our parameters
  ASSERT(m_nTimeoutDisabledButtonTimerID == 0);

  //Install the timeout / disabled button timer
  m_nTimeoutDisabledButtonTimerID = SetTimer(MESSAGEBOXX_TIMEOUT_DISABLED_TIMER, 1000, nullptr);

  return (m_nTimeoutDisabledButtonTimerID != 0);
}

#pragma warning(suppress: 26434)
void CMessageBoxX::OnTimer(UINT_PTR nIDEvent)
{
  //Is it our timer?
  if (nIDEvent == m_nTimeoutDisabledButtonTimerID)
    DoTimeoutDisabledButtonUpdate();
  else if (nIDEvent == m_nAnimatedTextTimerID)
    DoAnimatedTextUpdate();
  else if (nIDEvent == m_nAnimatedCaptionTimerID)
    DoAnimatedCaptionUpdate();
  else	
  {
    //No!, then just let the base class do its thing
    __super::OnTimer(nIDEvent);
  }
}

void CMessageBoxX::DoTimeoutDisabledButtonUpdate()
{
  //Decrement the relevent counters
  if (m_nTimerTimeout)
    --m_nTimerCountdown;
  if (m_nDisabledTimeout)
    --m_nDisabledCountdown;

  //Should we re-enable the relevent button
  if (m_nDisabledTimeout && (m_nDisabledCountdown == 0)) 
  {
    CWnd* pWnd = GetDlgItem(m_nDisabledButtonId);
#pragma warning(suppress: 26496)
    AFXASSUME(pWnd != nullptr);
    pWnd->EnableWindow(TRUE);

    //Reset the caption (if we do not have a timeout currently changing it)
    const BOOL bUpdateCaption = (m_bModifyCaptionForDisabledCountdown && ((m_nTimerTimeout && (m_nTimerCountdown > 10)) || (!m_nTimerTimeout)));
    if (bUpdateCaption)
      SetWindowText(m_sCaption);

    //Reset the disabled variables
    m_nDisabledTimeout = 0;
    m_nDisabledButtonId = 0;

    //Kill the timer (if we do not have a auto close pending)
    if (m_nTimerTimeout == 0)
    {
      KillTimer(m_nTimeoutDisabledButtonTimerID);
      m_nTimeoutDisabledButtonTimerID = 0;
    }
  }

  //Should we do the timeout?
  if (m_nTimerTimeout && (m_nTimerCountdown == 0))
  {
    //Kill the timer (since we are about to close!)
    KillTimer(m_nTimeoutDisabledButtonTimerID);
    m_nTimeoutDisabledButtonTimerID = 0;

    //Force the message box to close
    const CWnd* pButton = GetDlgItem(m_nTimerButtonId);
#pragma warning(suppress: 26496)
    AFXASSUME(pButton != nullptr);
#pragma warning(suppress: 26490)
    PostMessage(WM_COMMAND, MAKEWORD(m_nTimerButtonId, BN_CLICKED), reinterpret_cast<LPARAM>(pButton->GetSafeHwnd())); 

    //Reset the timer variables
    m_nTimerTimeout = 0;
    m_nTimerButtonId = 0;
  }
  else
  {
    //We didn't re enable a button and we didn't auto close the dialog, so see if we need to update the caption
    CString sNewCaption(GetCurrentTimerCaption());
    if (sNewCaption != m_sCaption)
      SetWindowText(sNewCaption);
    else
    {
      sNewCaption = GetCurrentDisabledCaption();
      if (sNewCaption != m_sCaption)
        SetWindowText(sNewCaption);
    }
  }
}

void CMessageBoxX::DoAnimatedTextUpdate()
{
  //Validate our parameters
#pragma warning(suppress: 26496)
  AFXASSUME(m_pAnimatedTexts != nullptr);

  const INT_PTR nSize = m_pAnimatedTexts->GetSize();
  ++m_nCurrentAnimatedTextIndex;

  //Have we come to the end of the animation array?
  BOOL bDoUpdate = FALSE;
  if (m_nCurrentAnimatedTextIndex == nSize)
  {
    if (m_bAnimatedTextLoop)
    {
      m_nCurrentAnimatedTextIndex = 0;
      bDoUpdate = TRUE;
    }
    else
    {
      //Kill the timer since we have no need for it anymore  
      KillTimer(m_nAnimatedTextTimerID);
      m_nAnimatedTextTimerID = 0;
    }
  }
  else
    bDoUpdate = TRUE;

  if (bDoUpdate)
  {
    //Lets update the text
    CWnd* pWndText = FindOurTextControl();
#pragma warning(suppress: 26496)
    AFXASSUME(pWndText != nullptr);
    pWndText->SetWindowText(m_pAnimatedTexts->GetAt(m_nCurrentAnimatedTextIndex));
  }
}

void CMessageBoxX::DoAnimatedCaptionUpdate()
{
  //Validate our parameters
#pragma warning(suppress: 26496)
  AFXASSUME(m_pAnimatedCaptions != nullptr);

  const INT_PTR nSize = m_pAnimatedCaptions->GetSize();
  ++m_nCurrentAnimatedCaptionIndex;

  //Have we come to the end of the animation array?
  BOOL bDoUpdate = FALSE;
  if (m_nCurrentAnimatedCaptionIndex == nSize)
  {
    if (m_bAnimatedCaptionLoop)
    {
      m_nCurrentAnimatedCaptionIndex = 0;
      bDoUpdate = TRUE;
    }
    else
    {
      //Kill the timer since we have no need for it anymore  
      KillTimer(m_nAnimatedCaptionTimerID);
      m_nAnimatedCaptionTimerID = 0;
    }
  }
  else
    bDoUpdate = TRUE;

  if (bDoUpdate)
  {
    //Lets update the caption
    SetWindowText(m_pAnimatedCaptions->GetAt(m_nCurrentAnimatedCaptionIndex));
  }
}

INT_PTR CMessageBoxX::DoModal()
{
  //Get the CWinApp* pointer
#pragma warning(suppress: 26466)
  auto pApp = static_cast<CMessageBoxPublicWinApp*>(AfxGetApp());

  //disable windows for modal dialog
  if (pApp != nullptr)
    pApp->DoEnableModeless(FALSE);
  HWND hWndTop = nullptr;
  HWND hWnd = CWnd::GetSafeOwner_(nullptr, &hWndTop);

  //re-enable the parent window, so that focus is restored 
  //correctly when the dialog is dismissed.
  if (hWnd != hWndTop)
    ::EnableWindow(hWnd, TRUE);

  //set help context if possible
  DWORD* pdwContext = nullptr;

  //Note we only use WM_HELPPROMPTADDR if the window belongs to the current process
  DWORD dwWndPid = 0;
  GetWindowThreadProcessId(hWnd, &dwWndPid);
  if ((hWnd != nullptr) && (dwWndPid == GetCurrentProcessId()))
  {
    //use app-level context or frame level context
    LRESULT lResult = ::SendMessage(hWnd, WM_HELPPROMPTADDR, 0, 0);
    if (lResult != 0)
#pragma warning(suppress: 26490)
      pdwContext = reinterpret_cast<DWORD*>(lResult);
  }
  //for backward compatibility use app context if possible
  if ((pdwContext == nullptr) && (pApp != nullptr))
    pdwContext = pApp->GetPromptContextAddress();

  DWORD dwOldPromptContext = 0;
  if (pdwContext != nullptr)
  {
    //save old prompt context for restoration later
#pragma warning(suppress: 26489)
    dwOldPromptContext = *pdwContext;
    if (m_nHelpIDPrompt != 0)
#pragma warning(suppress: 26489)
      *pdwContext = HID_BASE_PROMPT + m_nHelpIDPrompt;
  }

  //determine icon based on type specified
  if ((m_nType & MB_ICONMASK) == 0)
  {
    switch (m_nType & MB_TYPEMASK)
    {
      case MB_OK: //deliberate fallthrough
      case MB_OKCANCEL:
      {
        m_nType |= MB_ICONEXCLAMATION;
        break;
      }
      case MB_YESNO: //deliberate fallthrough
      case MB_YESNOCANCEL:
      {
        m_nType |= MB_ICONQUESTION;
        break;
      }
      case MB_ABORTRETRYIGNORE: //deliberate fallthrough
      case MB_RETRYCANCEL:
      {
        //No default icon for these types, since they are rarely used.
        //The caller should specify the icon.
        break;
      }
    }
  }

#ifdef _DEBUG
  if ((m_nType & MB_ICONMASK) == 0)
    TRACE(_T("CMessageBoxX::DoModal, Warning: no icon specified for message box.\n"));
#endif //#ifdef _DEBUG

  //Hook the creation of the MessageBox
  AfxHookWindowCreate(this);

  //Display the message box
  INT_PTR nResult = 0;
  if (m_nType & MB_USERICON)
  {
    MSGBOXPARAMS params;
    params.cbSize = sizeof(params);
    params.hwndOwner = hWnd;
    params.hInstance = m_hIconInstance;
    params.lpszText = m_sText;
    params.lpszCaption = m_sCaption;
    params.dwStyle = m_nType;
    params.lpszIcon = m_pszIcon;
    params.dwContextHelpId = 0; //We always use WM_HELP
    params.lpfnMsgBoxCallback = nullptr;
    params.dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    nResult = ::MessageBoxIndirect(&params);
  }
  else
    nResult = ::MessageBox(hWnd, m_sText, m_sCaption, m_nType);

  //restore prompt context if possible
  if (pdwContext != nullptr)
#pragma warning(suppress: 26489)
    *pdwContext = dwOldPromptContext;

  //re-enable windows
  if (hWndTop != nullptr)
    ::EnableWindow(hWndTop, TRUE);
  if (pApp)
    pApp->DoEnableModeless(TRUE);

  return nResult;
}

LRESULT CMessageBoxX::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  //What will be the return value
  LRESULT lResult = 0;

  //Prevent button click notifications from the disabled button if we are using a disabled button and it is outstanding
  if ((message == WM_COMMAND) && (m_nDisabledTimeout != 0) && (HIWORD(wParam) == BN_CLICKED) && (LOWORD(wParam) == m_nDisabledButtonId))
    lResult = 0;
  else
  {
    //Pass the buck to the default implementation
    lResult = __super::DefWindowProc(message, wParam, lParam);

    //Reset the timer if the timer is active and we are configred to reset on user activity
    if (m_nTimerTimeout && m_bResetTimeoutOnUserActivity && ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || ((message >= WM_KEYFIRST) && (message <= WM_KEYLAST))))
    {
      //reset the caption if it is modified
      if (m_bModifyCaptionForTimerCountdown && (m_nTimerCountdown <= 10))
        SetWindowText(m_sCaption);
    
      //And most importantly reset the coundown timer value
      m_nTimerCountdown = m_nTimerTimeout;
    }
  }

  return lResult;
}

#pragma warning(suppress: 26434)
void CMessageBoxX::OnDestroy()
{
  //Get the current check box state if we have one
  if (m_bCheckBox)
    m_bCheckBoxState = (m_wndCheckBox.GetCheck() != 0);

  //Kill the animations timers
  if (m_nAnimatedTextTimerID != 0)
  {
    KillTimer(m_nAnimatedTextTimerID);
    m_nAnimatedTextTimerID = 0;
  }
  if (m_nAnimatedCaptionTimerID != 0)
  {
    KillTimer(m_nAnimatedCaptionTimerID);
    m_nAnimatedCaptionTimerID = 0;
  }

  //Let the base class do its thing
  __super::OnDestroy();
}
