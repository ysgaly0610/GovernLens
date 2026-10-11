#if !defined(AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_)
#define AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CUserEditDlg dialog

#include <functional>

class CUser;

class CUserEditDlg : public CDialog
{
public:
	CUserEditDlg(CWnd* pParent = NULL);   

	CUser*	m_pUser;
	std::function<void(CWnd*)> m_showParamRange;
	afx_msg void OnUserParamRange();
// Dialog Data
	//{{AFX_DATA(CUserEditDlg)
	enum { IDD = IDD_USER_EDIT };
	CListBox	m_wndList;
	CString	m_sName;
	int		m_iList;
	//}}AFX_DATA
	//{{AFX_VIRTUAL(CUserEditDlg)
	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUserEditDlg)
	afx_msg void OnButtonAdd();
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonDelete();
	virtual void OnOK();
	afx_msg void OnSelchangeList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bColor;
	BOOL m_bDoc;
	afx_msg void OnBnClickedCheckColor();
	afx_msg void OnBnClickedCheckDoc();
	afx_msg void OnBnClickedButtonParaLevel();
	int m_iLevel;
	afx_msg void OnCbnSelchangeComboLevel();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};

#endif // !defined(AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_)
