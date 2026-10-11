// UserEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "UserEditDlg.h"
#include "User.h"
#include "ParaLevelDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "Setting.h"
/////////////////////////////////////////////////////////////////////////////
// CUserEditDlg dialog

CUserEditDlg::CUserEditDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CUserEditDlg::IDD, pParent)
	, m_iLevel(0)
{
	//{{AFX_DATA_INIT(CUserEditDlg)
	m_sName = _T("");
	m_iList = -1;
	//}}AFX_DATA_INIT
	m_pUser = NULL;
	m_bColor = false;
	m_bDoc = false;
}


void CUserEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUserEditDlg)
	DDX_Control(pDX, IDC_LIST, m_wndList);
	DDX_Text(pDX, IDC_EDIT_NAME, m_sName);
	DDX_LBIndex(pDX, IDC_LIST, m_iList);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_CHECK_COLOR, m_bColor);
	DDX_Check(pDX, IDC_CHECK_DOC, m_bDoc);
	DDX_CBIndex(pDX, IDC_COMBO_LEVEL, m_iLevel);
}


BEGIN_MESSAGE_MAP(CUserEditDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_USER_PARAM_RANGE, &CUserEditDlg::OnUserParamRange)//参数范围
	//{{AFX_MSG_MAP(CUserEditDlg)
	ON_BN_CLICKED(IDC_BUTTON_ADD, OnButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_DELETE, OnButtonDelete)
	ON_LBN_SELCHANGE(IDC_LIST, OnSelchangeList)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_CHECK_COLOR, &CUserEditDlg::OnBnClickedCheckColor)
	ON_BN_CLICKED(IDC_CHECK_DOC, &CUserEditDlg::OnBnClickedCheckDoc)
	ON_BN_CLICKED(IDC_BUTTON_PARA_LEVEL, &CUserEditDlg::OnBnClickedButtonParaLevel)//参数等级
	ON_CBN_SELCHANGE(IDC_COMBO_LEVEL, &CUserEditDlg::OnCbnSelchangeComboLevel)
END_MESSAGE_MAP()


void CUserEditDlg::OnButtonAdd() 
{
	UpdateData();
	m_sName.TrimLeft();
	m_sName.TrimRight();
	m_sName.MakeLower();
	if( m_sName.IsEmpty() )
		return;
	if( m_sName == "admin" )
		return;

	CString text;
	int nCount = m_wndList.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		m_wndList.GetText( i, text );
		if( text == m_sName )
			return;
	}
	m_wndList.AddString( m_sName );
}

BOOL CUserEditDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	if (CWnd* pButton = GetDlgItem(IDC_BUTTON_USER_PARAM_RANGE))
		pButton->EnableWindow(static_cast<bool>(m_showParamRange));

	if( m_pUser )
	{
		CUser::ITEM* pItem, *pNewItem;
		int nCount = m_pUser->m_aItems.GetSize();
		for(int i = 0; i < nCount; i++)
		{
			pItem = (CUser::ITEM *)m_pUser->m_aItems[ i ];
			int index = m_wndList.AddString( pItem->sName );
			pNewItem = new CUser::ITEM;
			m_pUser->CopyItem( pNewItem, pItem );
			m_wndList.SetItemDataPtr( index, pNewItem );
		}
	}

	OnSelchangeList();
	
	return TRUE;  
}

void CUserEditDlg::OnButtonDelete() 
{
	UpdateData();
	
	if( m_iList != -1 )
	{
		CUser::ITEM* pItem = (CUser::ITEM *)m_wndList.GetItemDataPtr( m_iList );
		if( pItem )
			delete pItem;
		m_wndList.DeleteString( m_iList );
	}
}

void CUserEditDlg::OnOK() 
{
	if( m_pUser )
	{
		m_pUser->ResetItems();

		CString text;
		CUser::ITEM* pItem;
		int nCount = m_wndList.GetCount();
		for(int i = 0; i < nCount; i++)
		{
			m_wndList.GetText(i, text);
			pItem = (CUser::ITEM *)m_wndList.GetItemDataPtr(i);
			if( !pItem )
				pItem = new CUser::ITEM;
			else
				m_wndList.SetItemDataPtr(i, NULL);
			pItem->sName = text;

			m_pUser->m_aItems.Add( pItem );
		}
	}
	
	CDialog::OnOK();
}

void CUserEditDlg::OnSelchangeList() 
{
	UpdateData();

	if( m_iList != -1 )
	{
		CUser::ITEM* pItem = (CUser::ITEM *)m_wndList.GetItemDataPtr( m_iList );
		if( pItem )
		{
			UINT bit = 1;
			m_bColor = (pItem->nEditable & (bit << CUser::E_COLOR)) ? true : false;
			m_bDoc = (pItem->nEditable & (bit << CUser::E_DOC)) ? true : false;
			m_iLevel = pItem->nLevel;
		}
		else
		{
			m_bColor = false;
			m_bDoc = false;
			m_iLevel = 0;
		}

		UpdateData(false);
	}
	GetDlgItem(IDC_CHECK_COLOR)->EnableWindow(m_iList != -1);
	GetDlgItem(IDC_CHECK_DOC)->EnableWindow(m_iList != -1);
	GetDlgItem(IDC_COMBO_LEVEL)->EnableWindow(m_iList != -1);
}

BOOL CUserEditDlg::DestroyWindow() 
{
	CUser::ITEM* pItem;
	int nCount = m_wndList.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		pItem = (CUser::ITEM *)m_wndList.GetItemDataPtr(i);
		if( pItem )
			delete pItem;
	}
	
	return CDialog::DestroyWindow();
}


void CUserEditDlg::OnBnClickedCheckColor()
{
	UpdateData();

	if( m_iList != -1 )
	{
		CUser::ITEM* pItem = (CUser::ITEM *)m_wndList.GetItemDataPtr( m_iList );
		if( !pItem )
		{
			pItem = new CUser::ITEM;
			m_wndList.SetItemDataPtr( m_iList, pItem );
		}
		UINT bit = 1;
		pItem->nEditable = 0;
		if( m_bColor )
			pItem->nEditable |= (bit << CUser::E_COLOR);
		if( m_bDoc )
			pItem->nEditable |= (bit << CUser::E_DOC);
		pItem->nLevel = m_iLevel;
	}
}


void CUserEditDlg::OnBnClickedCheckDoc()
{
	OnBnClickedCheckColor();
}


void CUserEditDlg::OnBnClickedButtonParaLevel()
{
	if( !m_pUser )
		return;

	//g_AviData.m_pSetting->m_oLevelManagerControler.se
	//CSetting::m_oLevelManagerControler.LoadConfigFromFile(CLevelManagerControler::LAN_CHINESE);	
	//CSetting::m_oLevelManagerControler.SetIsAdmin(true);	
	//if (CSetting::m_softwareType == CSetting::Avi)
	{
		CSetting::m_oLevelManagerControler.ShowLevelManagerDlg(true);//现在只执行这个
		return;
	}

	// CParaLevelDlg dlg;
	// dlg.m_pParaLevel = m_pUser->GetParaLevel();
	// dlg.m_pParaLevel->m_bIsView = m_pUser->m_bIsView;
	// dlg.DoModal();
}


void CUserEditDlg::OnCbnSelchangeComboLevel()
{
	OnBnClickedCheckColor();
}
BOOL CUserEditDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			return TRUE;
		default:
			break;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

void CUserEditDlg::OnUserParamRange()
{
	if (static_cast<bool>(m_showParamRange))
		m_showParamRange(this);
}
