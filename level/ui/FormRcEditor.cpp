#include "stdafx.h"
#include "FormRcEditor.h"
#include "LevelAuditIgnore.h"
#include "LevelLocalize.h"
#include "RcCore.h" // 用于 rceditor::gbIsLevelAdmin 和 资源结构定义

#include <vector>
#include <algorithm>
#include <functional>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define DESIGNSURFACEWND_NO_MESSAGE_MAP
//#include "DesignSurfaceWnd.h"

#ifndef WM_APP_DESIGNSURFACE
#define WM_APP_DESIGNSURFACE (WM_APP + 210)
#endif
#ifndef WM_APP_SYNC_TREE_CHECKS
#define WM_APP_SYNC_TREE_CHECKS (WM_APP + 211)
#endif

IMPLEMENT_DYNAMIC(CFormRcEditor, ParamRange::CFormNoRcBase)

namespace
{
    struct FlagDef
    {
        LPCTSTR name;
        DWORD mask;
        DWORD value;
    };

    static const DWORD_PTR PID_DLG_CAPTION = 1001;
    static const DWORD_PTR PID_DLG_STYLE_RAW = 1002;
    static const DWORD_PTR PID_DLG_EXSTYLE_RAW = 1003;
    static const DWORD_PTR PID_DLG_NONRES_INLEVEL = 1101;

    static const DWORD_PTR PID_CTRL_CLASS = 2001;
    static const DWORD_PTR PID_CTRL_TEXT = 2002;
    static const DWORD_PTR PID_CTRL_RECT = 2003;
    static const DWORD_PTR PID_CTRL_STYLE_RAW = 2004;
    static const DWORD_PTR PID_CTRL_EXSTYLE_RAW = 2005;
    static const DWORD_PTR PID_CTRL_NONRES_MULTILINE = 2101;
    static const DWORD_PTR PID_CTRL_NONRES_DISABLED_CURLEVEL = 2102;

    static const DWORD_PTR PID_FLAG_BASE = 3000; // + index
    static const DWORD_PTR PID_FLAGEX_BASE = 4000; // + index
    // 工具栏属性 ID
    static const DWORD_PTR PID_TB_BTN_ID = 9001;
    static const DWORD_PTR PID_TB_BTN_TEXT = 9002;
    static const DWORD_PTR PID_TB_BTN_LEVELMASK_RAW = 9003;
    static const DWORD_PTR PID_TB_FLAG_BASE = 9200; // per-level flags for toolbar buttons
    static const DWORD_PTR PID_TB_NONRES_INLEVEL = 9004; // toolbar-level In Level Manager flag

    // （保存/恢复的实现已移至文件作用域，放置在匿名命名空间之外。）

    // UpdateBadgeUiFromSelection 的实现放在文件下方，避免重复定义。
    // 静态对话框（RC 资源）查找：仅访问 mapLangAndDialogInfo，避免与动态窗口发生 ID 冲突。
    static std::shared_ptr<rceditor::SDialogInfo> GetStaticDialog(const std::shared_ptr<rceditor::SSingleResourceInfo>& res, UINT dlgId)
    {
        if (!res)
            return nullptr;
        auto it = res->mapLangAndDialogInfo.find(dlgId);
        return (it != res->mapLangAndDialogInfo.end()) ? it->second : nullptr;
    }

    // 动态窗口查找：仅访问 mapDynamicDialogInfo，保证与静态窗口完全分离（即便 ID 相同）。
    static std::shared_ptr<rceditor::SDialogInfo> GetDynamicDialog(const std::shared_ptr<rceditor::SSingleResourceInfo>& res, UINT dlgId)
    {
        if (!res)
            return nullptr;
        auto it = res->mapDynamicDialogInfo.find(dlgId);
        return (it != res->mapDynamicDialogInfo.end()) ? it->second : nullptr;
    }
}

// 实际的 UpdateBadgeUiFromSelection 实现在文件末尾，避免与之前的声明位置冲突。

// 等级界面钩子：基类提供默认空实现，派生类可覆盖以实现具体行为
void CFormRcEditor::CreateLevelUiControls()
{
    // 基类不创建等级管理相关控件；由需要的派生类实现
}

void CFormRcEditor::OnLevelUiVisibilityChanged(bool /*visible*/)
{
    // 基类不响应可见性变化；派生类可覆盖以作出反应
}

void CFormRcEditor::LayoutLevelUiControls(const CRect& /*rcCheck*/, const CRect& /*rcCombo*/)
{
    // 基类未实现布局；派生类负责布局等级 UI 控件
}

void CFormRcEditor::HandleCheckAllClicked()
{
    // 基类不处理“全部选中”行为，派生类实现具体逻辑
}

void CFormRcEditor::HandleBadgeSelChange()
{
    // 基类不处理徽章选择变化，派生类实现具体逻辑
}

BEGIN_MESSAGE_MAP(CFormRcEditor, ParamRange::CFormNoRcBase)
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_DESTROY()
    ON_WM_GETMINMAXINFO()
    ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_RCED, &CFormRcEditor::OnTvnSelchangedTree)
    ON_NOTIFY(TVN_ITEMCHANGED, IDC_TREE_RCED, &CFormRcEditor::OnTvnItemchangedTree)
    ON_NOTIFY(NM_CLICK, IDC_TREE_RCED, &CFormRcEditor::OnNMClickTree)
    //ON_REGISTERED_MESSAGE(AFX_WM_PROPERTY_CHANGED, OnPropertyChanged)
    ON_EN_CHANGE(IDC_EDIT_SEARCH_RCED, &CFormRcEditor::OnSearchChanged)
    ON_BN_CLICKED(IDC_BTN_SEARCH_RCED, &CFormRcEditor::OnSearchClicked)
    ON_BN_CLICKED(IDC_BTN_SEARCH_RCED, &CFormRcEditor::OnSearchClicked)
    ON_BN_CLICKED(IDC_CHECK_ALL_RCED, &CFormRcEditor::OnCheckAllClicked)
    ON_MESSAGE(WM_APP_DESIGNSURFACE, &CFormRcEditor::OnDesignSurfaceNotify)
    ON_MESSAGE(WM_APP_SYNC_TREE_CHECKS, &CFormRcEditor::OnPostSyncTreeChecks)
    ON_CBN_SELCHANGE(IDC_COMBO_BADGE_RCED, &CFormRcEditor::OnBadgeSelChange)
    ON_COMMAND(ID_RCED_FILE_OPEN, &CFormRcEditor::OnFileOpen)
    ON_COMMAND(ID_RCED_FILE_SAVE, &CFormRcEditor::OnFileSave)
    ON_COMMAND(ID_RCED_FILE_SAVE_AS, &CFormRcEditor::OnFileSaveAs)
    ON_COMMAND(ID_RCED_FILE_EXIT, &CFormRcEditor::OnFileExit)
END_MESSAGE_MAP()

//（OnNMClickTree 的实现位于文件后面；保持单一定义。）

static void InitToolbarCommonControlsOnce()
{
    static bool inited = false;
    if (inited)
        return;
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    // 确保已初始化 treeview 和 toolbar 所需的公共控件类
    icc.dwICC = ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    inited = true;
}
    // 额外注释：初始化工具栏控件所需的公共控件类

static CRect GetWorkAreaForWindow(CWnd* wnd)
{
    CRect work;
    work.SetRectEmpty();

    HMONITOR hMon = nullptr;
    if (wnd && wnd->GetSafeHwnd())
        hMon = ::MonitorFromWindow(wnd->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST);
    if (!hMon)
        hMon = ::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (hMon && ::GetMonitorInfo(hMon, &mi))
    {
        work = mi.rcWork; // 已扣除任务栏等停靠栏
        return work;
    }

    // fallback：获取主显示器的工作区作为后备
    RECT rc{};
    if (::SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0))
        work = rc;
    return work;
}

CFormRcEditor::CFormRcEditor(CWnd* pParent /*=nullptr*/)
    : ParamRange::CFormNoRcBase(pParent)
{
    CRect rect(0, 0, 600, 800);
    CStringW caption = GetInitialCaption();
    DLGTEMPLATE* pTemplate = CreateTemplate(
        WS_POPUPWINDOW | DS_MODALFRAME | WS_CAPTION | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME,
        rect, caption);
    if (!InitModalIndirect(pTemplate, pParent))
    {
        return;
    }
}

CFormRcEditor::~CFormRcEditor()
{
    // 子窗口将在对话框销毁时由 MFC 自动销毁。
    // 不要在此处 `delete` 派生自 `CWnd` 的子窗口以避免重复销毁或断言。
    m_surface = nullptr;

    if (m_font.GetSafeHandle() != NULL)
        m_font.DeleteObject();

    // 菜单句柄通常会在窗口销毁时由系统释放；这里仅做 Detach，避免对已释放句柄调用 DestroyMenu。
    if (m_menu.GetSafeHmenu())
        m_menu.Detach();
    if (m_menuFile.GetSafeHmenu())
        m_menuFile.Detach();
    if (m_menuEdit.GetSafeHmenu())
        m_menuEdit.Detach();
}

void CFormRcEditor::OnDestroy()
{
    OnBeforeDialogClose();
    m_closing = true;
    // 在销毁期间阻止属性网格发送晚到的通知。
    // 不要在此处调用 `RemoveAll()`：在 WM_DESTROY 期间属性网格可能已经处于自身的销毁过程，MFC 可能断言。
    m_inPropUpdate = true;

    // 确保自定义分配的 surface 窗口只销毁一次。
    if (m_surface)
    {
        if (m_surface->GetSafeHwnd() && ::IsWindow(m_surface->GetSafeHwnd()))
            m_surface->DestroyWindow();
        delete m_surface;
        m_surface = nullptr;
    }

    // 先让窗口不再持有菜单，再由我们销毁，避免析构或系统重复销毁。
    if (GetSafeHwnd() && ::IsWindow(m_hWnd))
        SetMenu(nullptr);

    if (m_menu.GetSafeHmenu())
    {
        m_menu.DestroyMenu();
        // 子菜单句柄属于根菜单结构，根菜单 DestroyMenu 后这里必须 Detach 避免悬空句柄。
        m_menuFile.Detach();
        m_menuEdit.Detach();
    }

    CDialog::OnDestroy();
}

void CFormRcEditor::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    if (lpMMI)
    {
        CRect work = GetWorkAreaForWindow(this);
        if (!work.IsRectEmpty())
        {
            lpMMI->ptMaxSize.x = work.Width();
            lpMMI->ptMaxSize.y = work.Height();
            lpMMI->ptMaxTrackSize.x = work.Width();
            lpMMI->ptMaxTrackSize.y = work.Height();
        }
    }

    CDialog::OnGetMinMaxInfo(lpMMI);
    if (lpMMI && UseParameterEditorStyle())
    {
        const CRect work = GetWorkAreaForWindow(this);
        lpMMI->ptMinTrackSize.x = (std::min)(StylePx(1000), work.Width());
        lpMMI->ptMinTrackSize.y = (std::min)(StylePx(580), work.Height());
    }
}

BOOL CFormRcEditor::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetWindowCaption(GetInitialCaption());

    if (!m_initialized)
    {
        CString csFontName = UseParameterEditorStyle() ? _T("Microsoft YaHei UI") : _T("Times New Roman");
        if (UseParameterEditorStyle() && gbCurNeedShowLan == 4) csFontName = _T("Malgun Gothic");
        if (UseParameterEditorStyle() && gbCurNeedShowLan == 5) csFontName = _T("Leelawadee UI");
        if (UseParameterEditorStyle() && gbCurNeedShowLan == 7) csFontName = _T("Microsoft JhengHei UI");
        m_font.CreateFont(UseParameterEditorStyle() ? -StylePx(16) : 16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, csFontName);
        SetFont(&m_font);
        if (UseParameterEditorStyle()) EnableParameterEditorStyle();

        CreateMenus();
        CreateControls();
        CreateToolbar();
        OnAfterControlsCreated();

        m_initialized = true;
    }

    // 初始尺寸：默认占当前显示器工作区的 80%，并居中；不覆盖任务栏、不超出屏幕。
    {
        CRect work = GetWorkAreaForWindow(this);
        if (!work.IsRectEmpty())
        {
            const int workW = work.Width();
            const int workH = work.Height();
            int w = (int)(workW * 0.80);
            int h = (int)(workH * 0.80);
            w = std::max(900, std::min(w, workW));
            h = std::max(600, std::min(h, workH));

            int x = work.left + (workW - w) / 2;
            int y = work.top + (workH - h) / 2;
            SetWindowPos(nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    LayoutControls();
    RebuildTree();
    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
    return TRUE;
}

void CFormRcEditor::SetDesignSurfaceLevelConfigMode(bool enable)
{
    if (m_surface)
        m_surface->SetLevelManagerConfigMode(enable);

    // Toggle level manager UI visibility
    m_showLevelUi = enable;
    if (m_checkAll.GetSafeHwnd())
        m_checkAll.ShowWindow(enable ? SW_SHOW : SW_HIDE);
    if (m_comboBadge.GetSafeHwnd())
        m_comboBadge.ShowWindow(enable ? SW_SHOW : SW_HIDE);
    // Show checkboxes in tree when level manager UI is enabled
    // Use style update here: adding/removing TVS_CHECKBOXES requires recreating
    // the control in some environments. A robust approach is to set the state
    // image mask when building the tree; we will ensure the tree was created
    // without TVS_CHECKBOXES by default and derived classes may choose to
    // recreate it with checkboxes if needed. For Level Manager we recreate.
    if (enable && m_tree.GetSafeHwnd())
    {
        // Recreate tree with checkboxes enabled. Do not attempt to preserve
        // expanded/selection state; keep behavior simple and reliable.
        CRect rc;
        m_tree.GetWindowRect(&rc);
        ScreenToClient(&rc);
        DWORD style = m_tree.GetStyle();
        m_tree.DestroyWindow();
        m_tree.Create(style | TVS_CHECKBOXES | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, rc, this, IDC_TREE_RCED);
        m_tree.SetFont(&m_font);
        if (m_initialized)
            RebuildTree();
    }
    else if (!enable && m_tree.GetSafeHwnd())
    {
        // Recreate tree without checkboxes. Simplicity: rebuild tree, no state preservation.
        CRect rc;
        m_tree.GetWindowRect(&rc);
        ScreenToClient(&rc);
        DWORD style = m_tree.GetStyle();
        m_tree.DestroyWindow();
        m_tree.Create((style & ~TVS_CHECKBOXES) | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, rc, this, IDC_TREE_RCED);
        m_tree.SetFont(&m_font);
        if (m_initialized)
            RebuildTree();
    }
}

void CFormRcEditor::SetWindowCaption(const CStringW& caption)
{
    if (!GetSafeHwnd())
        return;
    // MFC wrapper SetWindowText expects LPCTSTR; forward to CWnd::SetWindowText
    CWnd::SetWindowText(rceditor::CStringConverter::ConvertToT(caption));
}

// 注意：`CreateTemplate` 的实现已移至拆分后的基类 `CFormRcBase.cpp`。

void CFormRcEditor::CreateControls()
{
    CRect rc(0, 0, 0, 0);

    if (!m_editSearch.GetSafeHwnd())
    {
        m_editSearch.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rc, this, IDC_EDIT_SEARCH_RCED);
        m_editSearch.SetFont(&m_font);
        m_editSearch.SetWindowText(_T(""));
    }

    if (!m_btnSearch.GetSafeHwnd())
    {
        m_btnSearch.Create(ML(UI_SEARCH(gbCurNeedShowLan)), WS_CHILD | WS_VISIBLE | WS_TABSTOP | (UseParameterEditorStyle() ? BS_OWNERDRAW : BS_PUSHBUTTON), rc, this, IDC_BTN_SEARCH_RCED);
        m_btnSearch.SetFont(&m_font);
    }

    if (!m_tree.GetSafeHwnd())
    {
        // 创建树控件（初始不带 TVS_CHECKBOXES），随后通过 ModifyStyle 添加复选框样式，
        // 与 CLevelManagerViewer 的实现方式一致，确保系统提供的 state image 用于复选框显示。
        m_tree.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            rc, this, IDC_TREE_RCED);
        // 动态添加复选框样式以确保复选框图像正常显示
        m_tree.ModifyStyle(0, TVS_CHECKBOXES);
        m_tree.SetFont(&m_font);
    }

    // 等级管理控件由派生类在需要时创建。

    if (!m_surface)
    {
        m_surface = new rceditor::CRcDesignSurfaceWnd();
        m_surface->Create(this, IDC_SURFACE_RCED, rc);
    }

    // 创建属性列表控件
    if (!m_propList.GetSafeHwnd())
    {
        m_propList.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            rc, this, IDC_PROPLIST_RCED);
        m_propList.SetFont(&m_font);
        m_propList.InitColumns(ML(PROPERTY_GRID_HEADER_LEFT(gbCurNeedShowLan)), ML(PROPERTY_GRID_HEADER_RIGHT(gbCurNeedShowLan)));
        // 设置布尔值本地化字符串
        m_propList.SetBoolLabels(ML(UI_PROPLIST_YES(gbCurNeedShowLan)), ML(UI_PROPLIST_NO(gbCurNeedShowLan)));
        
        // 设置属性变更回调
        m_propList.SetPropertyChangedCallback([this](int nIndex, const CPropertyListCtrl::SPropertyItem& item) {
            OnPropertyListChanged(nIndex, item);
        });
    }
}

int CFormRcEditor::CalcComboBestWidth(CComboBox& combo) const
{
    if (!combo.GetSafeHwnd())
        return 60;

    CDC* dc = combo.GetDC();
    if (!dc)
        return 60;
    HFONT hFont = (HFONT)m_font.GetSafeHandle();
    HGDIOBJ oldFont = nullptr;
    if (hFont)
        oldFont = ::SelectObject(dc->GetSafeHdc(), hFont);

    int maxW = 0;
    const int count = combo.GetCount();
    for (int i = 0; i < count; ++i)
    {
        CString s;
        combo.GetLBText(i, s);
        CSize sz = dc->GetTextExtent(s);
        maxW = std::max(maxW, (int)sz.cx);
    }

    // 为下拉箭头和内边距预留空间
    maxW += 28;

    if (oldFont)
        ::SelectObject(dc->GetSafeHdc(), oldFont);
    combo.ReleaseDC(dc);
    return std::max(60, maxW);
}

int CFormRcEditor::CalcComboDroppedHeight(CComboBox& combo, int visibleItems) const
{
    if (!combo.GetSafeHwnd())
        return 120;

    visibleItems = std::max(1, std::min(visibleItems, 20));
    // `GetItemHeight(-1)` 可能返回编辑框高度；对于 DROPDOWNLIST 我们使用项高度 0。
    int itemH = combo.GetItemHeight(0);
    if (itemH <= 0)
        itemH = 18;

    const int border = ::GetSystemMetrics(SM_CYBORDER);
    const int listExtra = std::max(2, border * 2 + 2);
    int count = combo.GetCount();
    if (count <= 0)
        count = visibleItems;
    int items = std::min(count, visibleItems);
    return itemH * items + listExtra;
}

void CFormRcEditor::UpdateBadgeComboMetrics()
{
    if (!m_comboBadge.GetSafeHwnd())
        return;

    int dropW = CalcComboBestWidth(m_comboBadge);
    m_comboBadge.SendMessage(CB_SETDROPPEDWIDTH, (WPARAM)dropW, 0);

    COMBOBOXINFO cbi{};
    cbi.cbSize = sizeof(cbi);
    int visibleItems = 8;
    int targetDropH = CalcComboDroppedHeight(m_comboBadge, visibleItems);

    if (::GetComboBoxInfo(m_comboBadge.GetSafeHwnd(), &cbi))
    {
        CRect rcCombo;
        m_comboBadge.GetWindowRect(&rcCombo);
        // 列表框矩形为屏幕坐标
        int comboH = rcCombo.Height();
        int totalH = comboH + targetDropH;
        int totalW = std::max(rcCombo.Width(), dropW);
        // 保持窗口位置，仅调整大小以影响下拉高度。
        ::SetWindowPos(m_comboBadge.GetSafeHwnd(), nullptr, 0, 0, totalW, totalH,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CFormRcEditor::CreateMenus()
{
    if (m_menu.GetSafeHmenu())
        return;

    m_menu.CreateMenu();
    m_menuFile.CreatePopupMenu();

    auto aa = ML(UI_OPEN(gbCurNeedShowLan));
    m_menuFile.AppendMenu(MF_STRING, ID_RCED_FILE_OPEN, ML(UI_OPEN(gbCurNeedShowLan)));
    m_menuFile.AppendMenu(MF_SEPARATOR);
    m_menuFile.AppendMenu(MF_STRING, ID_RCED_FILE_SAVE, ML(UI_SAVE(gbCurNeedShowLan)));
    m_menuFile.AppendMenu(MF_STRING, ID_RCED_FILE_SAVE_AS, ML(UI_SAVE_AS(gbCurNeedShowLan)));
    m_menuFile.AppendMenu(MF_SEPARATOR);
    m_menuFile.AppendMenu(MF_STRING, ID_RCED_FILE_EXIT, ML(UI_EXIT(gbCurNeedShowLan)));

    m_menu.AppendMenu(MF_POPUP, (UINT_PTR)m_menuFile.GetSafeHmenu(), ML(UI_FILE_MENU(gbCurNeedShowLan)));

    SetMenu(&m_menu);
    DrawMenuBar();
}

void CFormRcEditor::CreateToolbar()
{
    if (m_toolbar.GetSafeHwnd())
        return;

    InitToolbarCommonControlsOnce();

    CRect rc(0, 0, 0, 0);
    m_toolbar.Create(WS_CHILD | WS_VISIBLE | CCS_TOP | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, rc, this, IDC_TOOLBAR_RCED);
    m_toolbar.SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    // 工具栏背景：使用透明样式，让父窗口绘制的灰条透出来
    m_toolbar.ModifyStyle(0, TBSTYLE_TRANSPARENT);

    // 文本按钮（不依赖 .rc 位图资源）
    // 构建一个以单个 NUL 分隔并以双 NUL 结尾的字符串缓冲区
    // 以满足 TB_ADDSTRING 一次添加多个字符串时的要求。
    std::vector<CString> labelList = {
        ML(UI_OPEN(gbCurNeedShowLan)), ML(UI_SAVE(gbCurNeedShowLan)), ML(UI_SAVE_AS(gbCurNeedShowLan)),
    };

    CString s;
    for (const auto& lbl : labelList)
    {
        s += lbl;
        // 追加单个 NUL 分隔符（嵌入一个 NUL 字符）
        s += (TCHAR)'\0';
    }
    // 最终以双 NUL 结尾
    s += (TCHAR)'\0';

    int strBase = (int)m_toolbar.SendMessage(TB_ADDSTRING, 0, (LPARAM)(LPCTSTR)s);

    TBBUTTON btns[4]{};
    int bi = 0;
    auto addBtn = [&](UINT cmd, int strIndex)
    {
        btns[bi].iBitmap = I_IMAGENONE;
        btns[bi].idCommand = cmd;
        btns[bi].fsState = TBSTATE_ENABLED;
        btns[bi].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        btns[bi].iString = strBase + strIndex;
        ++bi;
    };

    addBtn(ID_RCED_FILE_OPEN, 0);
    addBtn(ID_RCED_FILE_SAVE, 1);
    addBtn(ID_RCED_FILE_SAVE_AS, 2);

    m_toolbar.SendMessage(TB_ADDBUTTONS, (WPARAM)bi, (LPARAM)&btns[0]);
    m_toolbar.SendMessage(TB_AUTOSIZE, 0, 0);
}

BOOL CFormRcEditor::OnEraseBkgnd(CDC* pDC)
{
    if (!pDC)
        return TRUE;

    CRect client;
    GetClientRect(&client);

    // 先填充对话框默认底色
    COLORREF face = ::GetSysColor(COLOR_3DFACE);
    pDC->FillSolidRect(&client, face);

    // 再刷工具栏条（含左右 margin）
    const int margin = 6;
    const int toolbarH = 28;
    CRect rcToolbar(margin, margin, client.right - margin, margin + toolbarH);
    pDC->FillSolidRect(&rcToolbar, RGB(240, 240, 240));
    // 分隔线
    pDC->FillSolidRect(rcToolbar.left, rcToolbar.bottom - 1, rcToolbar.Width(), 1, RGB(210, 210, 210));

    return TRUE;
}

void CFormRcEditor::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (!m_initialized)
        return;
    LayoutControls();
}

void CFormRcEditor::LayoutControls()
{
    CRect client;
    GetClientRect(&client);

    const int margin = 6;
    const int treeW = 260;
    const int propW = 360;
    const int split = 6;
    const int toolbarH = 28;
    const int searchH = 24;
    const int bottomBarH = 28;

    CRect rcToolbar(margin, margin, client.right - margin, margin + toolbarH);
    CRect rcBody(margin, rcToolbar.bottom + split, client.right - margin, client.bottom - margin);

    if (m_toolbar.GetSafeHwnd())
        m_toolbar.MoveWindow(rcToolbar);

    CRect rcLeft(rcBody.left, rcBody.top, rcBody.left + treeW, rcBody.bottom);
    CRect rcSearchEdit(rcLeft.left, rcLeft.top, rcLeft.right - 70 - split, rcLeft.top + searchH);
    CRect rcSearchBtn(rcSearchEdit.right + split, rcLeft.top, rcLeft.right, rcLeft.top + searchH);
    CRect rcTree(rcLeft.left, rcSearchEdit.bottom + split, rcLeft.right, rcLeft.bottom);
    CRect rcProp(rcBody.right - propW, rcBody.top, rcBody.right, rcBody.bottom);
    CRect rcCenter(rcTree.right + split, rcBody.top, rcProp.left - split, rcBody.bottom);
    CRect rcSurface(rcCenter.left, rcCenter.top, rcCenter.right, std::max(rcCenter.top, rcCenter.bottom - bottomBarH - split));
    CRect rcBottom(rcCenter.left, rcSurface.bottom + split, rcCenter.right, rcCenter.bottom);
    CRect rcCheck(rcBottom.left, rcBottom.top, rcBottom.left + 80, rcBottom.bottom);
    int comboW = 80;
    if (m_comboBadge.GetSafeHwnd())
        comboW = std::min(140, std::max(80, CalcComboBestWidth(m_comboBadge)));
    CRect rcCombo(rcCheck.right + split, rcBottom.top, rcCheck.right + split + comboW, rcBottom.bottom);

    if (m_editSearch.GetSafeHwnd())
        m_editSearch.MoveWindow(rcSearchEdit);
    if (m_btnSearch.GetSafeHwnd())
        m_btnSearch.MoveWindow(rcSearchBtn);

    if (m_tree.GetSafeHwnd())
        m_tree.MoveWindow(rcTree);
    if (m_surface && m_surface->GetSafeHwnd())
        m_surface->MoveWindow(rcSurface);
    // 让派生类布局等级 UI 控件
    LayoutLevelUiControls(rcCheck, rcCombo);
    
    // 属性列表控件布局：占用右侧属性区域
    if (m_propList.GetSafeHwnd())
    {
        m_propList.MoveWindow(rcProp);
    }
}

bool CFormRcEditor::FuzzyMatchNoCase(const CString& haystack, const CString& needle)
{
    CString h = haystack;
    CString n = needle;
    h.MakeLower();
    n.MakeLower();
    return h.Find(n) >= 0;
}

// 只改变等级管理的显示层级，节点仍使用原来的模块和资源 ID。
HTREEITEM CFormRcEditor::GetLevelAuditGroup(HTREEITEM root, const CString& filePath)
{
    for (HTREEITEM item = m_tree.GetChildItem(root); item; item = m_tree.GetNextSiblingItem(item))
    {
        auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
        if (data && data->kind == TreeNodeData::NK_Category && data->dlgId == 0xF000)
            return item;
    }

    // 优先使用当前语言资源里的 IDS_DOC_SETTING，缺项沿用现有 ML 六语文案。
    CStringW title(ML(LEVEL_GROUP_DOCUMENT(gbCurNeedShowLan)));
    auto res = m_openedResources.find(filePath);
    if (res != m_openedResources.end() && res->second)
        for (const auto& block : res->second->mapLangAndStringTableInfo)
        {
            if (!block.second) continue;
            auto text = block.second->mapIDAndString.find(185);
            if (text != block.second->mapIDAndString.end() && !text->second.IsEmpty())
            {
                title = text->second;
                break;
            }
        }
    auto data = std::make_unique<TreeNodeData>();
    data->kind = TreeNodeData::NK_Category;
    data->dlgId = 0xF000;
    data->ctrlId = 0;
    data->filePath = filePath;
    HTREEITEM group = m_tree.InsertItem(CString(title), root, TVI_FIRST);
    m_tree.SetItemData(group, reinterpret_cast<DWORD_PTR>(data.get()));
    m_tree.SetCheck(group, FALSE); // 业务分组支持整组勾选，状态由子窗口模型汇总。
    m_treeNodeData.push_back(std::move(data));
    return group;
}

CString CFormRcEditor::GetLevelAuditDialogText(const CString& filePath,
    const rceditor::SDialogInfo& dlg) const
{
    CString text(dlg.csCaption);
    if (m_showLevelUi && !dlg.m_blIsDynamic && IsLevelAuditModule(filePath, _T("ImageAVI")))
    {
        if (dlg.nIDD == 260)
            text += ML(LEVEL_DOC_BLIND_CONDITION(gbCurNeedShowLan));
        else if (dlg.nIDD == 253)
            text += ML(LEVEL_DOC_HOLE_CONDITION(gbCurNeedShowLan));
    }
    return text;
}

HTREEITEM CFormRcEditor::GetLevelAuditParent(HTREEITEM root, HTREEITEM defaultParent,
    const CString& filePath, const rceditor::SDialogInfo& dlg)
{
    if (!m_showLevelUi || !IsLevelAuditDocSettingDialog(filePath, dlg))
        return defaultParent;
    HTREEITEM group = GetLevelAuditGroup(root, filePath);
    const auto page = dlg.m_blIsDynamic ? nullptr : GetLevelAuditDocPage(dlg.nIDD);
    if (page && page->parentId)
    {
        auto res = m_openedResources.find(filePath);
        if (res != m_openedResources.end() && res->second)
        {
            auto parent = res->second->mapLangAndDialogInfo.find(page->parentId);
            if (parent != res->second->mapLangAndDialogInfo.end() && parent->second &&
                !ShouldIgnoreLevelAuditDialog(filePath, *parent->second))
                return InsertDialogTreeItem(root, group, filePath, *parent->second);
        }
    }
    // 旧资源若缺少父页，仍保留子窗口入口。
    return group;
}

HTREEITEM CFormRcEditor::InsertDialogTreeItem(HTREEITEM root, HTREEITEM defaultParent,
    const CString& filePath, const rceditor::SDialogInfo& dlg)
{
    HTREEITEM existing = FindTreeItemForDialog(dlg.nIDD, filePath);
    if (existing)
        return existing;
    HTREEITEM parent = GetLevelAuditParent(root, defaultParent, filePath, dlg);
    CString text = GetLevelAuditDialogText(filePath, dlg);
    if (!m_showLevelUi)
        text.Format(_T("%s (%u)"), CString(dlg.csCaption).GetString(), dlg.nIDD);
    auto data = std::make_unique<TreeNodeData>();
    data->kind = TreeNodeData::NK_Dialog;
    data->dlgId = dlg.nIDD;
    data->ctrlId = 0;
    data->filePath = filePath;
    HTREEITEM item = m_tree.InsertItem(text, parent, TVI_LAST);
    m_tree.SetItemData(item, reinterpret_cast<DWORD_PTR>(data.get()));
    m_treeNodeData.push_back(std::move(data));
    m_tree.SetCheck(item, dlg.m_blIsInLevelManager ? TRUE : FALSE);
    return item;
}

void CFormRcEditor::RebuildTree()
{
    if (!m_tree.GetSafeHwnd())
        return;
	m_searchMatchedIndices.clear();
	m_searchMatchedDlgId = 0;

    // 保留展开/选择状态以避免在 UI 更新时出现意外的折叠/展开。
    // 按身份（filePath + dlgId）保存，而不是 HTREEITEM。
    std::vector<std::pair<CString, bool>> rootExpanded;
    struct SExp { int kind; UINT id; CString filePath; };
    std::vector<SExp> expandedNodes;
    // 帮助函数：查询给定 kind/id/file 的节点是否之前已展开
    auto isExpanded = [&](int kind, UINT id, const CString& fp)->bool
    {
        for (const auto& e : expandedNodes)
        {
            if (e.kind == kind && e.id == id && e.filePath.CompareNoCase(fp) == 0)
                return true;
        }
        return false;
    };
    CString selFilePath;
    UINT selDlgId = 0;
    int selKind = -1;
    // Save roots and their expansion
    for (HTREEITEM hRoot = m_tree.GetRootItem(); hRoot; hRoot = m_tree.GetNextSiblingItem(hRoot))
    {
        TreeNodeData* rd = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hRoot));
        if (!rd)
            continue;
        CString fp = rd->filePath;
        // TVIS_EXPANDED flag via GetItemState
        UINT st = m_tree.GetItemState(hRoot, TVIS_EXPANDED);
        bool expanded = (st & TVIS_EXPANDED) != 0;
        rootExpanded.push_back(std::make_pair(fp, expanded));
        // record selected dialog if selection is under this root
        HTREEITEM sel = m_tree.GetSelectedItem();
        if (sel)
        {
            TreeNodeData* sd = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(sel));
            if (sd)
            {
                selFilePath = sd->filePath;
                selDlgId = sd->dlgId;
                selKind = (int)sd->kind;
            }
        }
        // record expanded state for all descendant nodes under this root
        std::function<void(HTREEITEM)> recordRec;
        recordRec = [&](HTREEITEM hNode)
        {
            for (HTREEITEM it = hNode; it; it = m_tree.GetNextSiblingItem(it))
            {
                TreeNodeData* dd = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(it));
                if (!dd)
                    continue;
                UINT st2 = m_tree.GetItemState(it, TVIS_EXPANDED);
                if ((st2 & TVIS_EXPANDED) != 0)
                {
                    expandedNodes.push_back(SExp{ (int)dd->kind, dd->dlgId, fp });
                }
                // recurse into children
                HTREEITEM child = m_tree.GetChildItem(it);
                if (child)
                    recordRec(child);
            }
        };
        recordRec(m_tree.GetChildItem(hRoot));
    }
    
    m_inTreeUpdate = true;
    m_tree.DeleteAllItems();
    m_treeNodeData.clear();
    // 如果没有任何打开文件，显示单占位 root
    if (m_openedResources.empty())
    {
        CString rootText = ML(UI_NO_FILE(gbCurNeedShowLan));
        auto rootNd = std::make_unique<TreeNodeData>();
        rootNd->dlgId = 0;
        rootNd->ctrlId = (UINT)~0u;
        rootNd->filePath.Empty();
        TreeNodeData* pRootNd = rootNd.get();
        m_treeNodeData.push_back(std::move(rootNd));

        HTREEITEM hRoot = m_tree.InsertItem(rootText, TVI_ROOT, TVI_LAST);
        m_tree.SetItemData(hRoot, (DWORD_PTR)pRootNd);
        // 文件根节点不需要checkbox
        m_tree.SetItemState(hRoot, 0, TVIS_STATEIMAGEMASK);
        // 文件根节点不需要checkbox
        m_tree.SetItemState(hRoot, 0, TVIS_STATEIMAGEMASK);
        m_tree.Expand(hRoot, TVE_EXPAND);
        m_inTreeUpdate = false;
        return;
    }

    // 遍历所有已打开的文件，每个文件一棵子树
    // 对根使用稳定排序以避免树顺序不确定
    std::vector<CString> files;
    files.reserve(m_openedResources.size());
    for (const auto& kv : m_openedResources)
        files.push_back(kv.first);
    std::sort(files.begin(), files.end(), [](const CString& a, const CString& b) { return a.CompareNoCase(b) < 0; });
    for (const auto& filePath : files)
    {
        const size_t nodesBeforeFile = m_treeNodeData.size();
        auto resIt = m_openedResources.find(filePath);
        if (resIt == m_openedResources.end())
            continue;
        auto res = resIt->second;
        if (!res)
            continue;

        // 根节点文本：只显示文件名
        CString rootText = filePath;
        int slash = rootText.ReverseFind(_T('\\'));
        if (slash >= 0)
            rootText = rootText.Mid(slash + 1);
        int slash2 = rootText.ReverseFind(_T('/'));
        if (slash2 >= 0)
            rootText = rootText.Mid(slash2 + 1);

        // 根节点
        auto rootNd = std::make_unique<TreeNodeData>();
        rootNd->kind = TreeNodeData::NK_FileRoot;
        rootNd->dlgId = 0;
        rootNd->ctrlId = (UINT)~0u;
        rootNd->filePath = filePath;
        TreeNodeData* pRootNd = rootNd.get();
        m_treeNodeData.push_back(std::move(rootNd));

        HTREEITEM hRoot = m_tree.InsertItem(rootText, TVI_ROOT, TVI_LAST);
        m_tree.SetItemData(hRoot, (DWORD_PTR)pRootNd);
        m_tree.SetItemState(hRoot, 0, TVIS_STATEIMAGEMASK);

        CString filter = m_searchText;
        filter.Trim();

        // Create category nodes (Dialogs/Menus/Toolbars/DynamicWnds) if present
        HTREEITEM hCatDialogs = nullptr;
        HTREEITEM hCatMenus = nullptr;
        HTREEITEM hCatToolbars = nullptr;
        HTREEITEM hCatDynamics = nullptr;

        // Dialogs category
        if (!res->mapLangAndDialogInfo.empty())
        {
            auto catNd = std::make_unique<TreeNodeData>();
            catNd->kind = TreeNodeData::NK_Category;
            catNd->dlgId = 0;
            catNd->ctrlId = (UINT)~0u;
            catNd->filePath = filePath;
            TreeNodeData* pCatNd = catNd.get();
            m_treeNodeData.push_back(std::move(catNd));

            hCatDialogs = m_tree.InsertItem(ML(UI_DIALOGS(gbCurNeedShowLan)), hRoot, TVI_LAST);
            m_tree.SetItemData(hCatDialogs, (DWORD_PTR)pCatNd);
            // 类型节点不需要checkbox
            m_tree.SetItemState(hCatDialogs, 0, TVIS_STATEIMAGEMASK);
        }

        // Menus category: only create when not in filtered search; in search mode we'll add only matching menus
        // 在等级管理器模式下不显示菜单信息（只显示对话框和工具栏）
        if (!m_showLevelUi && filter.IsEmpty() && !res->mapLangAndMenuInfo.empty())
        {
            auto catNd = std::make_unique<TreeNodeData>();
            catNd->kind = TreeNodeData::NK_Category;
            catNd->dlgId = 0;
            catNd->ctrlId = (UINT)~0u;
            catNd->filePath = filePath;
            TreeNodeData* pCatNd = catNd.get();
            m_treeNodeData.push_back(std::move(catNd));

            hCatMenus = m_tree.InsertItem(ML(UI_MENUS(gbCurNeedShowLan)), hRoot, TVI_LAST);
            m_tree.SetItemData(hCatMenus, (DWORD_PTR)pCatNd);
            // 类型节点不需要checkbox
            m_tree.SetItemState(hCatMenus, 0, TVIS_STATEIMAGEMASK);
        }

        // Toolbars category: only create when not in filtered search
        const bool hasVisibleToolbars = std::any_of(res->mapLangAndToolbarInfo.begin(),
            res->mapLangAndToolbarInfo.end(), [&](const auto& entry) {
                return entry.second && (!m_showLevelUi ||
                    !ShouldIgnoreLevelAuditToolbar(filePath, entry.first));
            });
        if (filter.IsEmpty() && hasVisibleToolbars)
        {
            auto catNd = std::make_unique<TreeNodeData>();
            catNd->kind = TreeNodeData::NK_Category;
            catNd->dlgId = TreeNodeData::ToolbarCategoryId;
            catNd->ctrlId = 0;
            catNd->filePath = filePath;
            TreeNodeData* pCatNd = catNd.get();
            m_treeNodeData.push_back(std::move(catNd));

            hCatToolbars = m_tree.InsertItem(ML(UI_TOOLBARS(gbCurNeedShowLan)), hRoot, TVI_LAST);
            m_tree.SetItemData(hCatToolbars, (DWORD_PTR)pCatNd);
            if (m_showLevelUi)
                m_tree.SetCheck(hCatToolbars, FALSE); // 由可配置子项汇总
            else
                m_tree.SetItemState(hCatToolbars, 0, TVIS_STATEIMAGEMASK);
        }

        // dialogs for this resource (static dialogs only)
        if (res && !res->mapLangAndDialogInfo.empty())
        {
            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dialogs;
            dialogs.reserve(res->mapLangAndDialogInfo.size());
            for (const auto& kv2 : res->mapLangAndDialogInfo)
                dialogs.push_back(kv2);
            std::sort(dialogs.begin(), dialogs.end(), [&](const auto& a, const auto& b)
            {
                const UINT ao = m_showLevelUi ? GetLevelAuditDocPageOrder(filePath, a.second.get()) : ~0u;
                const UINT bo = m_showLevelUi ? GetLevelAuditDocPageOrder(filePath, b.second.get()) : ~0u;
                return ao != bo ? ao < bo : a.first < b.first;
            });

            for (const auto& kv2 : dialogs)
            {
                const UINT dlgId = kv2.first;
                auto dlg = kv2.second;
                if (!dlg || (m_showLevelUi && ShouldIgnoreLevelAuditDialog(filePath, *dlg)))
                    continue;

                CString dlgText;
                if (m_showLevelUi)
                    dlgText = GetLevelAuditDialogText(filePath, *dlg);
                else
                    dlgText.Format(_T("%s (%u)"), CString(dlg->csCaption).GetString(), dlgId);
                if (!filter.IsEmpty())
                {
                    CString idStr;
                    idStr.Format(_T("%u"), dlgId);
                    if (!FuzzyMatchNoCase(dlgText, filter) && !FuzzyMatchNoCase(idStr, filter))
                        continue;
                }

                InsertDialogTreeItem(hRoot, hCatDialogs ? hCatDialogs : hRoot, filePath, *dlg);
            }
        }

        // dynamic dialogs: place under a separate DynamicWnds category
        if (!res->mapDynamicDialogInfo.empty())
        {
            auto catNd = std::make_unique<TreeNodeData>();
            catNd->kind = TreeNodeData::NK_Category;
            catNd->dlgId = 0;
            catNd->ctrlId = (UINT)~0u;
            catNd->filePath = filePath;
            TreeNodeData* pCatNd = catNd.get();
            m_treeNodeData.push_back(std::move(catNd));

            hCatDynamics = m_tree.InsertItem(ML(u8"DynamicWnds"), hRoot, TVI_LAST);
            m_tree.SetItemData(hCatDynamics, (DWORD_PTR)pCatNd);
            // 类型节点不需要checkbox
            m_tree.SetItemState(hCatDynamics, 0, TVIS_STATEIMAGEMASK);

            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dyns;
            dyns.reserve(res->mapDynamicDialogInfo.size());
            for (const auto& kv2 : res->mapDynamicDialogInfo)
                dyns.push_back(kv2);
            std::sort(dyns.begin(), dyns.end(), [](const auto& a, const auto& b){ return a.first < b.first; });

            for (const auto& kv2 : dyns)
            {
                const UINT dlgId = kv2.first;
                auto dlg = kv2.second;
                if (!dlg || (m_showLevelUi && ShouldIgnoreLevelAuditDialog(filePath, *dlg))) continue;
                CString dlgText;
                if (m_showLevelUi)
                    dlgText = CString(dlg->csCaption);
                else
                    dlgText.Format(_T("%s (%u)"), CString(dlg->csCaption).GetString(), dlgId);
                if (!filter.IsEmpty())
                {
                    CString idStr; idStr.Format(_T("%u"), dlgId);
                    if (!FuzzyMatchNoCase(dlgText, filter) && !FuzzyMatchNoCase(idStr, filter))
                        continue;
                }

                auto nd = std::make_unique<TreeNodeData>();
                nd->kind = TreeNodeData::NK_DynamicWnd;
                nd->dlgId = dlgId;
                nd->ctrlId = 0;
                nd->filePath = filePath;
                TreeNodeData* pNd = nd.get();
                m_treeNodeData.push_back(std::move(nd));

                HTREEITEM hDlg = m_tree.InsertItem(dlgText, GetLevelAuditParent(hRoot, hCatDynamics, filePath, *dlg), TVI_LAST);
                m_tree.SetItemData(hDlg, (DWORD_PTR)pNd);
                m_tree.SetCheck(hDlg, dlg->m_blIsInLevelManager ? TRUE : FALSE);
                if (isExpanded((int)TreeNodeData::NK_DynamicWnd, dlgId, filePath))
                    m_tree.Expand(hDlg, TVE_EXPAND);

                // Note: dynamic dialog controls are not shown as child tree nodes.
                // Dynamic dialogs are represented as a toolbar-like preview on the design surface
                // and their per-control editing is handled via the surface/properties, so
                // we intentionally omit adding child nodes here to keep the tree flat.
            }
        }

        // Menus: add each menu resource under Menus category
        if (hCatMenus)
        {
            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SMenuInfo>>> menus;
            menus.reserve(res->mapLangAndMenuInfo.size());
            for (const auto& kv2 : res->mapLangAndMenuInfo)
                menus.push_back(kv2);
            std::sort(menus.begin(), menus.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
            for (const auto& kv2 : menus)
            {
                UINT menuId = kv2.first;
                auto menu = kv2.second;
                if (!menu) continue;
                CString txt = CString(menu->csText);
                if (txt.IsEmpty())
                {
                    txt.Format(ML(UI_MENU_FMT(gbCurNeedShowLan)), menuId);
                }
                CString nodeText; // 此行保持不变
                nodeText.Format(_T("%s (%u)"), txt, menuId); // 此行保持不变
                auto nd = std::make_unique<TreeNodeData>();
                nd->kind = TreeNodeData::NK_Menu;
                nd->dlgId = menuId;
                nd->ctrlId = 0;
                nd->filePath = filePath;
                TreeNodeData* pNd = nd.get();
                m_treeNodeData.push_back(std::move(nd));

                HTREEITEM hMenu = m_tree.InsertItem(nodeText, hCatMenus, TVI_LAST);
                m_tree.SetItemData(hMenu, (DWORD_PTR)pNd);
                if (!expandedNodes.empty())
                {
                    if (isExpanded((int)TreeNodeData::NK_Menu, menuId, filePath))
                        m_tree.Expand(hMenu, TVE_EXPAND);
                }
                // Insert submenu items recursively
                std::function<void(const std::shared_ptr<rceditor::SMenuInfo>&, HTREEITEM)> InsertMenuItems;
                InsertMenuItems = [&](const std::shared_ptr<rceditor::SMenuInfo>& mi, HTREEITEM parent)
                {
                    if (!mi)
                        return;
                    for (const auto& sub : mi->vecSubMenu)
                    {
                        if (!sub)
                            continue;
                        CString subText = CString(sub->csText);
                        if (subText.IsEmpty())
                            subText.Format(ML(UI_ITEM_FMT(gbCurNeedShowLan)), sub->nID);
                        CString subNode;
                        subNode.Format(_T("%s (%u)"), subText, sub->nID);
                        auto sNd = std::make_unique<TreeNodeData>();
                        sNd->kind = TreeNodeData::NK_Menu;
                        sNd->dlgId = sub->nID; // use nID to identify menu item
                        sNd->ctrlId = 0;
                        sNd->filePath = filePath;
                        TreeNodeData* psNd = sNd.get();
                        m_treeNodeData.push_back(std::move(sNd));

                        HTREEITEM hItem = m_tree.InsertItem(subNode, parent, TVI_LAST);
                        m_tree.SetItemData(hItem, (DWORD_PTR)psNd);
                        if (!expandedNodes.empty())
                        {
                            if (isExpanded((int)TreeNodeData::NK_Menu, sub->nID, filePath))
                                m_tree.Expand(hItem, TVE_EXPAND);
                        }

                        // recurse
                        if (!sub->vecSubMenu.empty())
                            InsertMenuItems(sub, hItem);
                    }
                };
                // Populate top-level menu's children
                InsertMenuItems(menu, hMenu);
            }
        }

        // Toolbars: add each toolbar resource under Toolbars category
        if (hCatToolbars)
        {
            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SToolbarInfo>>> tbs;
            tbs.reserve(res->mapLangAndToolbarInfo.size());
            for (const auto& kv2 : res->mapLangAndToolbarInfo)
                tbs.push_back(kv2);
            std::sort(tbs.begin(), tbs.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
            for (const auto& kv2 : tbs)
            {
                UINT tbId = kv2.first;
                auto tb = kv2.second;
                if (!tb || (m_showLevelUi && ShouldIgnoreLevelAuditToolbar(filePath, tbId))) continue;
                CString nodeText;
                if (m_showLevelUi)
                    nodeText = GetLevelAuditToolbarText(filePath, *tb, ML(UI_TOOLBARS(gbCurNeedShowLan)));
                else
                    nodeText.Format(_T("Toolbar (%u)"), tbId);
                auto nd = std::make_unique<TreeNodeData>();
                nd->kind = TreeNodeData::NK_Toolbar;
                nd->dlgId = tbId;
                nd->ctrlId = 0;
                nd->filePath = filePath;
                TreeNodeData* pNd = nd.get();
                m_treeNodeData.push_back(std::move(nd));

                HTREEITEM hTb = m_tree.InsertItem(nodeText, hCatToolbars, TVI_LAST);
                m_tree.SetItemData(hTb, (DWORD_PTR)pNd);
                if (!expandedNodes.empty())
                {
                    if (isExpanded((int)TreeNodeData::NK_Toolbar, tbId, filePath))
                        m_tree.Expand(hTb, TVE_EXPAND);
                }
				// reflect whether toolbar participates in level manager
				m_tree.SetCheck(hTb, tb->m_blIsInLevelManager ? TRUE : FALSE);

            }
        }
        // If filter active, append additional matches (menus/toolbars) under this root
        if (!filter.IsEmpty())
        {
            CString filterLower = filter;
            filterLower.MakeLower();
            bool extra = AppendSearchMatchesForResource(res, filePath, filterLower, hRoot);
            // if no children were added for this file, remove the root and its node data
            if (!m_tree.GetChildItem(hRoot))
            {
                m_tree.DeleteItem(hRoot);
                // remove any TreeNodeData objects allocated for this file
                while (m_treeNodeData.size() > nodesBeforeFile)
                    m_treeNodeData.pop_back();
                continue;
            }
        }
        
        // 分组后移除已没有子项的分类，并恢复文档设定分组的展开状态。
        if (m_showLevelUi)
            for (HTREEITEM item = m_tree.GetChildItem(hRoot); item; )
            {
                HTREEITEM next = m_tree.GetNextSiblingItem(item);
                auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
                if (data && data->kind == TreeNodeData::NK_Category)
                {
                    if (!m_tree.GetChildItem(item))
                        m_tree.DeleteItem(item);
                    else if (data->dlgId == 0xF000 && (expandedNodes.empty() ||
                        isExpanded(TreeNodeData::NK_Category, 0xF000, filePath)))
                        m_tree.Expand(item, TVE_EXPAND);
                }
                item = next;
            }

        // 仅在有过滤器时展开文件根以显示匹配；否则保持根折叠，让用户手动展开/折叠。
        bool expandedRoot = false;
        for (const auto& p : rootExpanded)
        {
            if (p.first.CompareNoCase(filePath) == 0)
            {
                expandedRoot = p.second;
                break;
            }
        }
        if (expandedRoot)
            m_tree.Expand(hRoot, TVE_EXPAND);
        else
        {
            // No saved state: expand by default on first load when no prior selection, or when filter active
            if (m_lastSelectedTreeItem == nullptr && filter.IsEmpty())
                m_tree.Expand(hRoot, TVE_EXPAND);
            else if (!filter.IsEmpty())
                m_tree.Expand(hRoot, TVE_EXPAND);
        }
    }

    // 等所有子节点加入后，再恢复主 Tab 的展开状态。
    for (const auto& node : expandedNodes)
    {
        HTREEITEM item = FindTreeItemForNode(node.kind, node.id, node.filePath);
        if (item) m_tree.Expand(item, TVE_EXPAND);
    }

    // 如果没有先前记录的选择，则默认选中第一棵根下的第一个对话子节点
    if (selDlgId == 0 && m_lastSelectedTreeItem == nullptr)
    {
        HTREEITEM root = m_tree.GetRootItem();
        if (root)
        {
            HTREEITEM child = m_tree.GetChildItem(root);
            if (child)
            {
                m_tree.SelectItem(child);
                m_lastSelectedTreeItem = child;
            }
        }
    }
    else if (!selFilePath.IsEmpty() && selDlgId != 0)
    {
        // 尝试恢复先前的选择
        HTREEITEM restore = nullptr;
        if (selKind != -1)
            restore = FindTreeItemForNode(selKind, selDlgId, selFilePath);

        if (!restore)
            restore = FindTreeItemForDialog(selDlgId, selFilePath);
        if (!restore)
            restore = FindTreeItemForDialog(selDlgId);
        if (restore)
        {
            m_tree.SelectItem(restore);
            m_lastSelectedTreeItem = restore;
        }
    }

    // 确保树上复选框与模型数据一致（尤其是初次从 JSON 读取后构建树时，用户无需点击即可看到状态）。
    const bool syncCheckboxes = m_showLevelUi || ((m_tree.GetStyle() & TVS_CHECKBOXES) != 0);
    if (syncCheckboxes && GetSafeHwnd())
        PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);

    m_inTreeUpdate = false;
#if 0 //! 旧版本单文件树，新版本多文件树无问题后删除
	CString rootText = _T("<no file>");
	if (m_pe)
	{
		// show only file name in root
		CString path = m_pe->GetResourceInfo().mapLangAndDialogInfo.empty() ? _T("") : _T("");
		UNREFERENCED_PARAMETER(path);
	}
	// If we have a loaded module path, use it; otherwise keep placeholder.
	// `PEFileEditor` only stores it internally; reuse last open path from dialog.
	if (!m_currentIniPath.IsEmpty())
		rootText = m_currentIniPath;

	int slash = rootText.ReverseFind(_T('\\'));
	if (slash >= 0)
		rootText = rootText.Mid(slash + 1);
	int slash2 = rootText.ReverseFind(_T('/'));
	if (slash2 >= 0)
		rootText = rootText.Mid(slash2 + 1);

	auto rootNd = std::make_unique<TreeNodeData>();
	rootNd->dlgId = 0;
	rootNd->ctrlId = (UINT)~0u;
	TreeNodeData* pRootNd = rootNd.get();
	m_treeNodeData.push_back(std::move(rootNd));

	HTREEITEM hRoot = m_tree.InsertItem(rootText, TVI_ROOT, TVI_LAST);
	m_tree.SetItemData(hRoot, (DWORD_PTR)pRootNd);

	CString filter = m_searchText;
	filter.Trim();

	if (m_pRes)
	{
		// DialogId -> SDialogInfo
		std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dialogs;
		dialogs.reserve(m_pRes->mapLangAndDialogInfo.size());
		for (const auto& kv : m_pRes->mapLangAndDialogInfo)
			dialogs.push_back(kv);
		std::sort(dialogs.begin(), dialogs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		for (const auto& kv : dialogs)
		{
			const UINT dlgId = kv.first;
			auto dlg = kv.second;
			if (!dlg)
				continue;

			CString dlgText;
			dlgText.Format(_T("%s (%u)"), CString(dlg->csCaption).GetString(), dlgId);
			if (!filter.IsEmpty())
			{
				CString idStr;
				idStr.Format(_T("%u"), dlgId);
				if (!FuzzyMatchNoCase(dlgText, filter) && !FuzzyMatchNoCase(idStr, filter))
					continue;
			}

			auto nd = std::make_unique<TreeNodeData>();
			nd->dlgId = dlgId;
			nd->ctrlId = 0;
			TreeNodeData* pNd = nd.get();
			m_treeNodeData.push_back(std::move(nd));

			HTREEITEM hDlg = m_tree.InsertItem(dlgText, hRoot, TVI_LAST);
			m_tree.SetItemData(hDlg, (DWORD_PTR)pNd);
			m_tree.SetCheck(hDlg, dlg->m_blIsInLevelManager ? TRUE : FALSE);

			// Controls under dialog
			if (dlg && dlg->vecCtrlInfo.size() > 0)
			{
				for (const auto& ctrl : dlg->vecCtrlInfo)
				{
					if (!ctrl)
						continue;
					CString ct;
					CString text = CString(ctrl->csText);
					text.Replace(_T("\r"), _T(" "));
					text.Replace(_T("\n"), _T(" "));
					text.Trim();
					if (text.GetLength() > 20)
						text = text.Left(20) + _T("...");
					if (!text.IsEmpty())
						ct.Format(_T("%s (%u) \"%s\""), CString(ctrl->csClassName).GetString(), ctrl->nIDC, text.GetString());
					else
						ct.Format(_T("%s (%u)"), CString(ctrl->csClassName).GetString(), ctrl->nIDC);
					if (!filter.IsEmpty())
					{
						CString idStr;
						idStr.Format(_T("%u"), ctrl->nIDC);
						if (!FuzzyMatchNoCase(ct, filter) && !FuzzyMatchNoCase(idStr, filter) && !FuzzyMatchNoCase(CString(ctrl->csText), filter))
							continue;
					}

					auto cnd = std::make_unique<TreeNodeData>();
					cnd->dlgId = dlgId;
					cnd->ctrlId = ctrl->nIDC;
					TreeNodeData* pcnd = cnd.get();
					m_treeNodeData.push_back(std::move(cnd));

					HTREEITEM hCtrl = m_tree.InsertItem(ct, hDlg, TVI_LAST);
					m_tree.SetItemData(hCtrl, (DWORD_PTR)pcnd);
					// Control checkbox: reflect whether this control enables current level.
					// If dialog itself is not in level manager, keep unchecked.
					BOOL ctrlChecked = FALSE;
					if (dlg->m_blIsInLevelManager)
					{
						int levelIdx = std::max(0, m_curLevel - 1);
						ctrlChecked = ctrl->GetLevel(levelIdx) ? TRUE : FALSE;
					}
                    m_tree.SetItemState(hCtrl, INDEXTOSTATEIMAGEMASK(ctrlChecked ? 2 : 1), TVIS_STATEIMAGEMASK);
				}
			}
		}
	}

	m_tree.Expand(hRoot, TVE_EXPAND);
	m_inTreeUpdate = false;
#endif
}

LRESULT CFormRcEditor::OnPostSyncTreeChecks(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    if (!m_tree.GetSafeHwnd())
        return 0;

    const bool syncCheckboxes = m_showLevelUi || ((m_tree.GetStyle() & TVS_CHECKBOXES) != 0);
    if (!syncCheckboxes)
        return 0;

    const int levelIdx = std::max(0, std::min(7, m_curLevel - 1));
    auto getResByFile = [&](const CString& fp) -> std::shared_ptr<rceditor::SSingleResourceInfo>
    {
        auto it = m_openedResources.find(fp);
        if (it != m_openedResources.end())
            return it->second;
        return m_pRes;
    };

    std::function<void(HTREEITEM)> syncNode = [&](HTREEITEM hItem)
    {
        for (; hItem; hItem = m_tree.GetNextSiblingItem(hItem))
        {
            auto* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
            if (data)
            {
                auto resPtr = getResByFile(data->filePath);
                if (resPtr)
                {
                    switch (data->kind)
                    {
                    case TreeNodeData::NK_Dialog:
                    {
                        auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                        if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                            m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
                        break;
                    }
                    case TreeNodeData::NK_Control:
                    {
                        auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                        if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                        {
                            auto dlg = itDlg->second;
                            BOOL checked = FALSE;
                            if (dlg->m_blIsInLevelManager)
                            {
                                if (m_isSearchMode)
                                {
                                    int idxEncoded = (int)data->ctrlId - 1;
                                    if (idxEncoded >= 0 && idxEncoded < (int)dlg->vecCtrlInfo.size())
                                    {
                                        auto c = dlg->vecCtrlInfo[idxEncoded];
                                        if (c && (c->uiLevelMask & (1u << levelIdx)))
                                            checked = TRUE;
                                    }
                                }
                                else
                                {
                                    for (auto& c : dlg->vecCtrlInfo)
                                    {
                                        if (!c) continue;
                                        if (c->nIDC == data->ctrlId)
                                        {
                                            if (c->uiLevelMask & (1u << levelIdx))
                                                checked = TRUE;
                                            break;
                                        }
                                    }
                                }
                            }
                            m_tree.SetCheck(hItem, checked);
                        }
                        break;
                    }
                    case TreeNodeData::NK_DynamicWnd:
                    {
                        auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
                        if (itDlg != resPtr->mapDynamicDialogInfo.end() && itDlg->second)
                            m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
                        break;
                    }
                    case TreeNodeData::NK_DynamicItem:
                    {
                        auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
                        if (itDlg != resPtr->mapDynamicDialogInfo.end() && itDlg->second)
                        {
                            auto dlg = itDlg->second;
                            size_t idx = data->ctrlId;
                            BOOL checked = FALSE;
                            if (idx < dlg->vecCtrlInfo.size() && dlg->vecCtrlInfo[idx])
                            {
                                if (dlg->vecCtrlInfo[idx]->uiLevelMask & (1u << levelIdx))
                                    checked = TRUE;
                            }
                            m_tree.SetCheck(hItem, checked);
                        }
                        break;
                    }
                    case TreeNodeData::NK_Toolbar:
                    {
                        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                        if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                            m_tree.SetCheck(hItem, itTb->second->m_blIsInLevelManager ? TRUE : FALSE);
                        break;
                    }
                    case TreeNodeData::NK_ToolbarItem:
                    {
                        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                        if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                        {
                            auto tb = itTb->second;
                            size_t idx = data->ctrlId;
                            BOOL checked = FALSE;
                            if (idx < tb->vecLevelMask.size() && (tb->vecLevelMask[idx] & (1u << levelIdx)))
                                checked = TRUE;
                            m_tree.SetCheck(hItem, checked);
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            HTREEITEM child = m_tree.GetChildItem(hItem);
            if (child)
                syncNode(child);
        }
    };

    m_inTreeUpdate = true;
    for (HTREEITEM hRoot = m_tree.GetRootItem(); hRoot; hRoot = m_tree.GetNextSiblingItem(hRoot))
    {
        syncNode(hRoot);
    }
    SyncLevelTreeParentChecks();
    m_inTreeUpdate = false;
    m_tree.Invalidate(FALSE);
    return 0;
}

bool CFormRcEditor::OpenFile(const CString& path)
{
    if (path.IsEmpty())
        return false;

    // 若已打开则直接激活
    auto itEd = m_openedEditors.find(path);
    if (itEd != m_openedEditors.end())
    {
        SetActiveFile(path);
        return true;
    }

    // 创建并解析
    auto pe = std::make_shared<rceditor::PEFileEditor>(path);
    auto err = pe->ReadPEInfo();
    if (err != rceditor::PEFileEditor::ERR_SUCCESS)
    {
        AfxMessageBox(ML(UI_MSG_OPEN_PARSE_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
        return false;
    }

    // 保存到集合（持有资源）
    m_openedEditors[path] = pe;
    m_openedResources[path] = std::make_shared<rceditor::SSingleResourceInfo>(pe->GetResourceInfo());


#ifdef _DEBUG
    // 测试用：在加载文件后注入两个动态窗口，每个包含5个控件，便于交互测试
    if (!m_openedResources.empty())
    {
        auto it = m_openedResources.begin();
        if (it != m_openedResources.end() && it->second)
        {
            auto res = it->second;
            // 创建两个动态窗口
            for (int w = 0; w < 2; ++w)
            {
                auto dyn = std::make_shared<rceditor::SDialogInfo>();
                dyn->nIDD = (w == 0) ? 9001u : 9002u;
                dyn->m_blIsDynamic = true;
                dyn->m_blIsInLevelManager = true;
                if (w == 0)
                    dyn->csCaption = L"DebugDynamicWnd1";
                else
                    dyn->csCaption = L"DebugDynamicWnd2";

                // 五个控件
                for (int i = 0; i < 5; ++i)
                {
                    auto ctrl = std::make_shared<rceditor::SCtrlInfo>();
                    ctrl->nIDC = (w == 0) ? (1000u + (UINT)i + 1u) : (2000u + (UINT)i + 1u);
                    CStringW txt;
                    txt.Format(L"DynWnd%d Ctrl%d", w + 1, i + 1);
                    ctrl->csText = txt;
                    ctrl->uiLevelMask = 0; // 默认第1级可见
                    ctrl->m_blIsAdded = true;
					ctrl->csClassName = L"Button";
                    dyn->vecCtrlInfo.push_back(ctrl);
                }

                // 插入到资源的动态窗口集合
                res->mapDynamicDialogInfo[dyn->nIDD] = dyn;
            }

            // 刷新 UI
            RebuildTree();
            UpdatePropertyGrid();
            UpdateBadgeUiFromSelection();
            if (m_surface)
                m_surface->Invalidate(FALSE);
        }
    }
#endif



    // 激活
    SetActiveFile(path);
    return true;
}

void CFormRcEditor::SetActiveFile(const CString& path)
{
    auto itEd = m_openedEditors.find(path);
    auto itRes = m_openedResources.find(path);
    if (itEd == m_openedEditors.end() || itRes == m_openedResources.end())
        return;

    m_pe = itEd->second;
    m_pRes = itRes->second;
    m_currentIniPath = path;

    // 更新窗口标题（可根据需要显示文件名）
    CString fileName = path;
    int slash = fileName.ReverseFind(_T('\\'));
    if (slash >= 0) fileName = fileName.Mid(slash + 1);
    SetWindowCaption(CStringW(fileName));

    // 绑定 designer surface 到活动资源的第一个 dialog（如果有）
    if (m_surface && m_pRes && !m_pRes->mapLangAndDialogInfo.empty())
    {
        // Prefer language-mapped dialogs; if none present, fall back to dynamic dialogs so
        // dynamically created dialogs are treated as peers to static dialogs.
        if (!m_pRes->mapLangAndDialogInfo.empty())
            m_surface->SetDialogInfo(m_pRes->mapLangAndDialogInfo.begin()->second);
        else if (!m_pRes->mapDynamicDialogInfo.empty())
            m_surface->SetDialogInfo(m_pRes->mapDynamicDialogInfo.begin()->second);
        m_surface->SetCurrentLevel(m_curLevel);
        m_surface->ClearSelection();
        // Ensure any previously shown toolbar preview is cleared when switching active file/dialog
        m_surface->ClearToolbarInfo();
    }

    // 刷新树、属性、Badge
    RebuildTree();
    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
}

HTREEITEM CFormRcEditor::FindTreeItemForDialog(UINT dlgId, const CString& filePath /*= CString()*/) const
{
    return FindTreeItemForNode(TreeNodeData::NK_Dialog, dlgId, filePath);
}

HTREEITEM CFormRcEditor::FindTreeItemForNode(int kind, UINT id, const CString& filePath /*= CString()*/) const
{
    // 子窗口现在位于主 Tab 下，查找必须覆盖任意层级。
    std::function<HTREEITEM(HTREEITEM)> find = [&](HTREEITEM first)->HTREEITEM
    {
        for (HTREEITEM item = first; item; item = m_tree.GetNextSiblingItem(item))
        {
            auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
            if (data && (int)data->kind == kind && data->dlgId == id &&
                (filePath.IsEmpty() || data->filePath.CompareNoCase(filePath) == 0))
                return item;
            HTREEITEM child = find(m_tree.GetChildItem(item));
            if (child) return child;
        }
        return nullptr;
    };
    return find(m_tree.GetRootItem());
}

HTREEITEM CFormRcEditor::FindTreeItemForControl(UINT dlgId, UINT ctrlId, const CString& filePath /*= CString()*/) const
{
    // 紧凑树中不包含控件；若匹配 filePath 返回对应对话节点
    if (!filePath.IsEmpty())
        return FindTreeItemForDialog(dlgId, filePath);
    return FindTreeItemForDialog(dlgId);
}

void CFormRcEditor::SyncTreeSelectionFromSurface()
{
    if (m_inTreeUpdate)
        return;
    if (!m_surface || !m_tree.GetSafeHwnd())
        return;
    // If the design surface is currently showing a toolbar preview and the toolbar
    // has a selection, do not sync tree selection from the surface. This prevents
    // the tree from jumping back to a dialog when the user explicitly selected a
    // toolbar node or toolbar item. DynamicWnds are also rendered via toolbar path.
    if (m_surface->IsToolbarSelected())
        return;

    // If the user currently has a non-dialog node selected in the tree
    // (e.g. a Menu/Toolbar root), preserve that explicit selection and
    // do not let the surface sync override it.
    HTREEITEM curSel = m_tree.GetSelectedItem();
    if (curSel)
    {
        TreeNodeData* curData = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(curSel));
        if (curData && curData->kind != TreeNodeData::NK_Dialog && curData->kind != TreeNodeData::NK_Control)
            return;
    }

    auto dlg = m_surface->GetDialogInfo();
    if (!dlg)
        return;

    HTREEITEM target = nullptr;
    // 优先匹配与活动资源文件相符的对话节点，以避免多文件树中的歧义
    CString activeFile = m_currentIniPath;
    if (!activeFile.IsEmpty())
        target = FindTreeItemForDialog(dlg->nIDD, activeFile);
    if (!target)
        target = FindTreeItemForDialog(dlg->nIDD);

    if (target && target != m_lastSelectedTreeItem)
    {
        m_inTreeUpdate = true;
        m_tree.SelectItem(target);
        m_lastSelectedTreeItem = target;
        m_inTreeUpdate = false;
    }
}

void CFormRcEditor::UpdatePropertyGrid()
{
    // 属性网格已移除，直接使用属性列表控件
    UpdatePropertyList();
}

// 显示菜单信息（属性网格已移除，此函数保留为空实现以便兼容）
void ShowMenuInfoInPropertyGrid(const std::shared_ptr<rceditor::SMenuInfo>& menu)
{
    UNREFERENCED_PARAMETER(menu);
    // 属性网格已移除，菜单信息通过属性列表显示
}

void CFormRcEditor::RebuildPropertyGridForSelection()
{
    // 属性网格已移除，直接使用属性列表控件
    UpdatePropertyList();
}


void CFormRcEditor::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_closing)
	{
		*pResult = 0;
		return;
	}
	if (m_inTreeUpdate)
	{
		*pResult = 0;
		return;
	}

	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	HTREEITEM hItem = pNMTreeView->itemNew.hItem;
	if (!hItem)
	{
		*pResult = 0;
		return;
	}

    TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
    if (data)
    {
        // record last selected tree item to preserve user selection across rebuilds
        m_lastSelectedTreeItem = hItem;
        // Toolbar item node (child of a toolbar) selection: show toolbar and select item(s)
        if (data->kind == TreeNodeData::NK_ToolbarItem)
        {
            // switch active file/resource
            auto itRes = m_openedResources.find(data->filePath);
            if (itRes != m_openedResources.end() && itRes->second)
            {
                m_pRes = itRes->second;
                //m_pe = m_openedEditors[data->filePath];
                //对不存在的项mp会自动增加有记录但无对象的空项(点击操作触发），加载json时当成正常对象使用导致闪退
                // 修复1.不自动插入记录，2.使用json前确认有对象
                auto itEditor = m_openedEditors.find(data->filePath);
                m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                m_currentIniPath = data->filePath;
            }
            // bind toolbar and set selection
            if (m_pRes)
            {
                auto itTb = m_pRes->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != m_pRes->mapLangAndToolbarInfo.end() && itTb->second)
                {
                    auto tb = itTb->second;
                    if (m_surface)
                    {
                        m_surface->SetToolbarInfo(tb);
                        // determine ctrl state at time of selection
                        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        int idx = (int)data->ctrlId;
                        if (!ctrlDown)
                        {
                            m_surface->SetSelectedToolbarIndices(std::vector<int>{idx});
                        }
                        else
                        {
                            std::vector<int> cur = m_surface->GetSelectedToolbarIndices();
                            auto it = std::find(cur.begin(), cur.end(), idx);
                            if (it == cur.end())
                                cur.push_back(idx);
                            else
                                cur.erase(it);
                            m_surface->SetSelectedToolbarIndices(cur);
                        }
                        m_surface->Invalidate(FALSE);
                    }
                }
            }
            UpdatePropertyGrid();
            UpdateBadgeUiFromSelection();
            if (pResult) *pResult = 0;
            return;
        }
        if (data->kind == TreeNodeData::NK_Category && m_showLevelUi)
        {
            m_activeDynamicDialog.reset();
            m_activeDynamicFilePath.Empty();
            if (m_surface)
            {
                m_surface->ClearToolbarInfo();
                m_surface->SetDialogInfo(nullptr);
            }
        }
        else if (data->kind == TreeNodeData::NK_FileRoot)
        {
            if (!data->filePath.IsEmpty())
                SetActiveFile(data->filePath);
        }
        else if (data->kind == TreeNodeData::NK_Dialog || data->kind == TreeNodeData::NK_Control)
        {
            // 选择 dialog 或 control：要确保 surface 绑定到对应文件的资源
            auto itRes = m_openedResources.find(data->filePath);
            if (itRes != m_openedResources.end() && itRes->second)
            {
                m_pRes = itRes->second; // 切换模型指向（也把文件设为活动）
                auto itEditor = m_openedEditors.find(data->filePath);
                m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                m_currentIniPath = data->filePath;
                // 绑定 dialog
                auto dlg = GetStaticDialog(m_pRes, data->dlgId);
                if (dlg && m_surface)
                {
                    m_activeDynamicDialog.reset();
                    m_activeDynamicFilePath.Empty();
                    m_surface->SetDialogInfo(dlg);
                    m_surface->SetCurrentLevel(m_curLevel);
                    m_surface->ClearSelection();
                    // Clear any active toolbar view when switching to dialog/control
                    m_surface->ClearToolbarInfo();
                }
            }
        }
        else if (data->kind == TreeNodeData::NK_DynamicWnd || data->kind == TreeNodeData::NK_DynamicItem)
        {
            // Dynamic window: display controls in toolbar-like manner (10 per row handled by design surface)
            auto itRes = m_openedResources.find(data->filePath);
            if (itRes != m_openedResources.end() && itRes->second)
            {
                m_pRes = itRes->second;
                auto itEditor = m_openedEditors.find(data->filePath);
                m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                m_currentIniPath = data->filePath;
            }

            auto resPtr = m_pRes;
            auto dlg = GetDynamicDialog(resPtr, data->dlgId);
            m_activeDynamicDialog = dlg;
            m_activeDynamicFilePath = data->filePath;
            if (dlg && m_surface)
            {
                // Build a transient toolbar info from dynamic dialog controls
                auto tb = std::make_shared<rceditor::SToolbarInfo>();
                tb->nToolbarID = (int)dlg->nIDD;
                tb->m_blIsInLevelManager = dlg->m_blIsInLevelManager;
                for (auto& c : dlg->vecCtrlInfo)
                {
                    if (!c) continue;
                    tb->vecItemIDs.push_back((uint16_t)c->nIDC);
                    tb->vecItemText.push_back(c->csText);
                    tb->vecLevelMask.push_back(c->uiLevelMask);
                }
                m_surface->SetToolbarInfo(tb);
                if (data->kind == TreeNodeData::NK_DynamicItem)
                {
                    int idx = (int)data->ctrlId;
                    m_surface->SetSelectedToolbarIndices(std::vector<int>{ idx });
                }
                else
                {
                    m_surface->SetSelectedToolbarIndices({});
                }
                m_surface->Invalidate(FALSE);
                // Clear dialog binding to avoid conflicting renders
                m_surface->SetDialogInfo(dlg);
            }
        }
        else if (data->kind == TreeNodeData::NK_Menu)
        {
            // 在属性列表显示菜单信息
            // Locate the menu object in the resource and show it
            if (!data->filePath.IsEmpty())
            {
                auto itRes = m_openedResources.find(data->filePath);
                if (itRes != m_openedResources.end() && itRes->second)
                {
                    auto res = itRes->second;
                    auto itMenu = res->mapLangAndMenuInfo.find(data->dlgId);
                    if (itMenu != res->mapLangAndMenuInfo.end())
                    {
                        // 菜单信息通过属性列表显示
                        ShowMenuInfoInPropertyGrid(itMenu->second);
                        // Ensure any toolbar preview is cleared when showing menu info
                        if (m_surface)
                            m_surface->ClearToolbarInfo();
                    }
                }
            }
        }
        else if (data->kind == TreeNodeData::NK_Toolbar)
        {
            // Select toolbar: show its properties and render it on the design surface
            // 工具栏信息通过属性列表显示

            // Bind toolbar to design surface for preview
            if (!data->filePath.IsEmpty())
            {
                auto itRes = m_openedResources.find(data->filePath);
                if (itRes != m_openedResources.end() && itRes->second)
                {
                    auto res = itRes->second;
                    auto itTb = res->mapLangAndToolbarInfo.find(data->dlgId);
                    if (itTb != res->mapLangAndToolbarInfo.end())
                    {
                        m_activeDynamicDialog.reset();
                        m_activeDynamicFilePath.Empty();
                        if (m_surface)
                        {
                            m_surface->SetToolbarInfo(itTb->second);
                            // When showing toolbar, ensure dialog contents are not left visible
                            m_surface->Invalidate(FALSE);
                        }
                    }
                    else
                    {
                        if (m_surface)
                            m_surface->ClearToolbarInfo();
                    }
                }
            }
        }
    }
	// 刷新属性/Badge
	UpdatePropertyGrid();
	UpdateBadgeUiFromSelection();
	*pResult = 0;
#if 0  //! 旧版本单文件树，新版本多文件树无问题后删除
	TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
	if (data && m_surface && m_pRes)
	{
		if (data->ctrlId == (UINT)~0u)
		{
			// root node: switch to first dialog
			if (!m_pRes->mapLangAndDialogInfo.empty())
				m_surface->SetDialogInfo(m_pRes->mapLangAndDialogInfo.begin()->second);
			m_surface->SetCurrentLevel(m_curLevel);
			m_surface->ClearSelection();
		}
		else
		{
			auto dlg = GetCanonicalDialog(m_pRes, data->dlgId);
			if (dlg)
			{
				m_surface->SetDialogInfo(dlg);
				m_surface->SetCurrentLevel(m_curLevel);
                m_surface->ClearSelection();
			}
		}
	}
	UpdatePropertyGrid();
	UpdateBadgeUiFromSelection();
	*pResult = 0;
#endif
}

void CFormRcEditor::OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	// Use `NM_CLICK` + HitTest(state icon) to handle checkbox changes reliably.
	// This avoids affecting node selection switching and avoids duplicate work when rebuilding tree.
	if (pResult)
		*pResult = 0;
}

void CFormRcEditor::OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	if (pResult)
		*pResult = 0;
    if (m_closing)
        return;
    if (m_inTreeUpdate)
        return;

	// 获取点击位置（严格参照 CLevelManagerViewer::OnNMClickTreeSettings 的实现）
	CPoint pt;
	::GetCursorPos(&pt);
	m_tree.ScreenToClient(&pt);

	UINT flags = 0;
	HTREEITEM hItem = m_tree.HitTest(pt, &flags);
    
    // 如果没有命中任何项，则无需处理
    if (!hItem)
        return;

    // 如果点击不在状态图标上，则确保视觉复选框与模型状态匹配并返回。
    // 这样可以防止点击标签/区域时切换复选框状态。
    if ((flags & TVHT_ONITEMSTATEICON) == 0)
    {
        TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
        if (!data)
            return;

        // Determine resource for this node (do not change m_pRes here)
        std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
        auto itRes = m_openedResources.find(data->filePath);
        if (itRes != m_openedResources.end())
            resPtr = itRes->second;
        if (!resPtr)
            resPtr = m_pRes;

        if (resPtr)
        {
            if (data->kind == TreeNodeData::NK_Dialog)
            {
                auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                    m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_Toolbar)
            {
                auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                    m_tree.SetCheck(hItem, itTb->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_Control)
            {
                auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                {
                    auto dlg = itDlg->second;
                    int levelIdx = std::max(0, m_curLevel - 1);
                    BOOL checked = FALSE;
                    if (m_isSearchMode)
                    {
                        int idxEncoded = (int)data->ctrlId - 1;
                        if (idxEncoded >= 0 && idxEncoded < (int)dlg->vecCtrlInfo.size())
                        {
                            auto c = dlg->vecCtrlInfo[idxEncoded];
                            if (c && dlg->m_blIsInLevelManager && c->GetLevel(levelIdx))
                                checked = TRUE;
                        }
                    }
                    else
                    {
                        UINT storedId = data->ctrlId;
                        for (auto& c : dlg->vecCtrlInfo)
                        {
                            if (!c) continue;
                            if (c->nIDC == storedId)
                            {
                                if (dlg->m_blIsInLevelManager && c->GetLevel(levelIdx))
                                    checked = TRUE;
                                break;
                            }
                        }
                    }
                    m_tree.SetCheck(hItem, checked);
                }
            }
            else if (data->kind == TreeNodeData::NK_DynamicWnd)
            {
                auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapDynamicDialogInfo.end() && itDlg->second)
                    m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_DynamicItem)
            {
                auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapDynamicDialogInfo.end() && itDlg->second)
                {
                    auto dlg = itDlg->second;
                    size_t idx = data->ctrlId;
                    int levelIdx = std::max(0, std::min(7, m_curLevel - 1));
                    BOOL checked = FALSE;
                    if (idx < dlg->vecCtrlInfo.size())
                    {
                        auto c = dlg->vecCtrlInfo[idx];
                        if (c && (c->uiLevelMask & (1u << levelIdx)))
                            checked = TRUE;
                    }
                    m_tree.SetCheck(hItem, checked);
                }
            }
            else if (data->kind == TreeNodeData::NK_ToolbarItem)
            {
                auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                {
                    auto tb = itTb->second;
                    size_t idx = data->ctrlId;
                    int levelIdx = std::max(0, std::min(7, m_curLevel - 1));
                    BOOL checked = FALSE;
                    if (idx < tb->vecLevelMask.size() && (tb->vecLevelMask[idx] & (1u << levelIdx)) != 0)
                        checked = TRUE;
                    m_tree.SetCheck(hItem, checked);
                }
            }
        }

        return;
    }

    TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
    if (!data)
        return;
    if (data->kind == TreeNodeData::NK_FileRoot || data->kind == TreeNodeData::NK_Category)
        return; // 根节点和分类节点不处理

    // 非管理员权限不允许修改勾选状态（严格参照 CLevelManagerViewer）
    if (!rceditor::gbIsLevelAdmin)
    {
        // 非管理员时，恢复原来的状态
        // 这里需要根据节点类型获取模型中的状态
        std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
        auto itRes = m_openedResources.find(data->filePath);
        if (itRes != m_openedResources.end())
            resPtr = itRes->second;
        if (!resPtr)
            resPtr = m_pRes;
        
        if (resPtr)
        {
            if (data->kind == TreeNodeData::NK_Dialog)
            {
                auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                    m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_Toolbar)
            {
                auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                    m_tree.SetCheck(hItem, itTb->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
        }
        return;
    }

	std::shared_ptr<rceditor::SDialogInfo> dlg;

    // Determine the resource this tree item belongs to. Do not change m_pRes here
    // because clicking a checkbox should not necessarily switch the active file.
    std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
    auto itRes = m_openedResources.find(data->filePath);
    if (itRes != m_openedResources.end())
        resPtr = itRes->second;
    if (!resPtr)
        resPtr = m_pRes; // fallback to current active resource
    if (!resPtr)
        return;

    // 进入 NM_CLICK 时，树控件仍报告旧的勾选状态，所以直接取反
    // （严格参照 CLevelManagerViewer::OnNMClickTreeSettings 的实现）
	BOOL bChecked = !m_tree.GetCheck(hItem);  // 进入时状态还是上一次的，所以直接取反
	const bool newChecked = (bChecked != FALSE);





    // Dialogs/controls carry level-manager semantics
    if (data->kind == TreeNodeData::NK_Dialog || data->kind == TreeNodeData::NK_Control)
    {
        dlg = GetStaticDialog(resPtr, data->dlgId);
        if (!dlg)
            return;

        if (data->kind == TreeNodeData::NK_Dialog)
        {
            // dialog node toggled
            dlg->m_blIsInLevelManager = newChecked;
			m_tree.SetCheck(hItem, newChecked);
        }
        else // control
        {
            // Control node: checked => enable current level bit; unchecked => ignore (keep existing).
			if (!newChecked)
			{
				m_tree.SetCheck(hItem, TRUE);
				return;
			}
			m_tree.SetCheck(hItem, TRUE);
            if (!dlg->m_blIsInLevelManager)
                dlg->m_blIsInLevelManager = true;

            const UINT storedId = data->ctrlId;
            // Interpret storedId according to current tree mode to avoid accidental nIDC collisions.
            const int levelIdx = std::max(0, m_curLevel - 1);
            if (m_isSearchMode)
            {
                // In search tree we encode index as (idx+1)
                int idxEncoded = (int)storedId - 1;
                if (idxEncoded >= 0 && idxEncoded < (int)dlg->vecCtrlInfo.size())
                {
                    auto& c = dlg->vecCtrlInfo[idxEncoded];
                    if (c)
                        c->uiLevelMask |= (1ULL << levelIdx);
                }
            }
            else
            {
                // In normal tree storedId is the real control nIDC
                for (auto& c : dlg->vecCtrlInfo)
                {
                    if (!c) continue;
                    if (c->nIDC == storedId)
                    {
                        c->uiLevelMask |= (1ULL << levelIdx);
                        break;
                    }
                }
            }
        }
    }
    else if (data->kind == TreeNodeData::NK_DynamicWnd)
    {
        auto dlgDyn = GetDynamicDialog(resPtr, data->dlgId);
        if (!dlgDyn)
            return;
        dlgDyn->m_blIsInLevelManager = newChecked;
        m_tree.SetCheck(hItem, newChecked);

        if (m_activeDynamicDialog && m_activeDynamicDialog->nIDD == dlgDyn->nIDD)
            m_activeDynamicDialog->m_blIsInLevelManager = newChecked;

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        return;
    }
    else if (data->kind == TreeNodeData::NK_DynamicItem)
    {
        auto dlgDyn = GetDynamicDialog(resPtr, data->dlgId);
        if (!dlgDyn)
            return;
        size_t idxItem = data->ctrlId;
        if (idxItem >= dlgDyn->vecCtrlInfo.size())
            return;
        auto ctrl = dlgDyn->vecCtrlInfo[idxItem];
        if (!ctrl)
            return;

        const int levelIdx = std::max(0, m_curLevel - 1);
        const UINT bit = (1u << levelIdx);
        if (newChecked)
            ctrl->uiLevelMask |= bit;
        else
            ctrl->uiLevelMask &= ~bit;
        m_tree.SetCheck(hItem, newChecked);

        if (m_activeDynamicDialog && m_activeDynamicDialog->nIDD == dlgDyn->nIDD)
        {
            if (idxItem < m_activeDynamicDialog->vecCtrlInfo.size() && m_activeDynamicDialog->vecCtrlInfo[idxItem])
                m_activeDynamicDialog->vecCtrlInfo[idxItem]->uiLevelMask = ctrl->uiLevelMask;
        }
        if (m_surface && m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == (int)dlgDyn->nIDD)
        {
            auto tb = m_surface->GetActiveToolbar();
            if (tb)
            {
                if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
                    tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0);
                if (idxItem < tb->vecLevelMask.size())
                    tb->vecLevelMask[idxItem] = ctrl->uiLevelMask;
                tb->m_blIsInLevelManager = dlgDyn->m_blIsInLevelManager;
                m_surface->SetToolbarInfo(tb);
                m_surface->Invalidate(FALSE);
            }
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        return;
    }
    else if (data->kind == TreeNodeData::NK_Toolbar)
    {
        // Toolbar node toggled
        if (!resPtr)
            return;
        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
        if (itTb == resPtr->mapLangAndToolbarInfo.end() || !itTb->second)
            return;
        auto tb = itTb->second;
		tb->m_blIsInLevelManager = newChecked;
		m_tree.SetCheck(hItem, newChecked);

        // If this toolbar is currently shown on surface, update it
        if (m_surface && m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == tb->nToolbarID)
        {
            m_surface->SetToolbarInfo(tb);
            m_surface->Invalidate(FALSE);
        }

        // refresh UI
        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        return;
    }
    else if (data->kind == TreeNodeData::NK_ToolbarItem)
    {
        // Toggle per-item participation for toolbar in level manager (also reused for dynamic items rendered as toolbar)
        if (!resPtr)
            return;
        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
        std::shared_ptr<rceditor::SToolbarInfo> tb;
        bool fromDynamic = false;
        if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
        {
            tb = itTb->second;
        }
        else if (m_activeDynamicDialog && m_activeDynamicDialog->nIDD == data->dlgId)
        {
            // Build a transient toolbar info from dynamic dialog for mask updates
            tb = std::make_shared<rceditor::SToolbarInfo>();
            tb->nToolbarID = (int)m_activeDynamicDialog->nIDD;
            tb->m_blIsInLevelManager = m_activeDynamicDialog->m_blIsInLevelManager;
            for (auto& c : m_activeDynamicDialog->vecCtrlInfo)
            {
                if (!c) continue;
                tb->vecItemIDs.push_back((uint16_t)c->nIDC);
                tb->vecItemText.push_back(c->csText);
                tb->vecLevelMask.push_back(c->uiLevelMask);
            }
            fromDynamic = true;
        }
        if (!tb)
            return;
        size_t idxItem = data->ctrlId;
        if (idxItem >= tb->vecItemIDs.size())
            return;
        // ensure vecLevelMask size
        if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
            tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0);
        // For checkbox toggle, when checked -> set current level bit; unchecked -> clear current level bit
        const int levelIdx = std::max(0, m_curLevel - 1);
        const UINT bit = (1u << levelIdx);
		if (newChecked)
			tb->vecLevelMask[idxItem] |= bit;
		else
			tb->vecLevelMask[idxItem] &= ~bit;
		m_tree.SetCheck(hItem, newChecked);

        // If toolbar not already participating, ensure its flag is set when any item has mask
        if (!tb->m_blIsInLevelManager && tb->vecLevelMask[idxItem] != 0)
            tb->m_blIsInLevelManager = true;

        if (fromDynamic && m_activeDynamicDialog)
        {
            // propagate back to dynamic dialog controls
            if (idxItem < m_activeDynamicDialog->vecCtrlInfo.size() && m_activeDynamicDialog->vecCtrlInfo[idxItem])
            {
                m_activeDynamicDialog->vecCtrlInfo[idxItem]->uiLevelMask = tb->vecLevelMask[idxItem];
            }
        }
        else if (itTb != resPtr->mapLangAndToolbarInfo.end())
        {
            itTb->second->vecLevelMask = tb->vecLevelMask;
            itTb->second->m_blIsInLevelManager = tb->m_blIsInLevelManager;
        }

        // sync surface if showing this toolbar
        if (m_surface && m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == tb->nToolbarID)
        {
            m_surface->SetToolbarInfo(tb);
            m_surface->Invalidate(FALSE);
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        return;
    }
    else
    {
        // Menus: no checkbox semantics currently
        return;
    }

    // Refresh current view if the affected dialog is on surface.
    if (m_surface && m_surface->GetDialogInfo() && m_surface->GetDialogInfo()->nIDD == dlg->nIDD)
    {
        m_surface->SetDialogInfo(dlg);
        m_surface->SetCurrentLevel(m_curLevel);
        m_surface->Invalidate(FALSE);
    }

    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
}

bool CFormRcEditor::PickOpenFile(CString& outPath) const
{
	CFileDialog dlg(TRUE, _T("exe"), NULL,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_ENABLESIZING,
		_T("Executable (*.exe;*.dll)|*.exe;*.dll|All files (*.*)|*.*||"),
		(const_cast<CFormRcEditor*>(this)));
	if (dlg.DoModal() != IDOK)
		return false;
	outPath = dlg.GetPathName();
	return true;
}

bool CFormRcEditor::PickSaveFile(CString& outPath) const
{
	CFileDialog dlg(FALSE, _T("exe"), NULL,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
		_T("Executable (*.exe;*.dll)|*.exe;*.dll|All files (*.*)|*.*||"),
		(const_cast<CFormRcEditor*>(this)));
	if (dlg.DoModal() != IDOK)
		return false;
	outPath = dlg.GetPathName();
	return true;
}

bool CFormRcEditor::TryParseDword(const CString& s, DWORD& outValue)
{
	CString t = s;
	t.Trim();
	if (t.IsEmpty())
		return false;

	LPCTSTR start = t.GetString();
	TCHAR* endPtr = nullptr;
	unsigned long v = _tcstoul(start, &endPtr, 0); // base=0 支持 0x
	if (endPtr == start)
		return false;
	outValue = (DWORD)v;
	return true;
}

LRESULT CFormRcEditor::OnPropertyChanged(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	// 属性网格已移除，此消息处理保留为空
	// 属性变更通过 CPropertyListCtrl 的回调处理
	return 0;
}

void CFormRcEditor::OnFileOpen()
{
	CString path;
	if (!PickOpenFile(path))
		return;

	if (!OpenFile(path))
	{

	}



	return;
	//! 旧版本支持单个文件
	// Reuse this field as "current opened binary" path.
	m_currentIniPath = path;

	m_pe.reset(new rceditor::PEFileEditor(path));
	auto err = m_pe->ReadPEInfo();
	if (err != rceditor::PEFileEditor::ERR_SUCCESS)
	{
		m_pe.reset();
		m_pRes.reset();
		AfxMessageBox(ML(UI_MSG_OPEN_PARSE_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
		return;
	}

	// Build a UI-editable copy to avoid mutating the original PE resource until Save/SaveAs.
	m_pRes = std::make_shared<rceditor::SSingleResourceInfo>(m_pe->GetResourceInfo());

	// Default: switch designer surface to the first dialog.
	if (m_surface && m_pRes && !m_pRes->mapLangAndDialogInfo.empty())
	{
		m_surface->SetDialogInfo(m_pRes->mapLangAndDialogInfo.begin()->second);
		m_surface->SetCurrentLevel(m_curLevel);
	}

	RebuildTree();
	UpdatePropertyGrid();
	UpdateBadgeUiFromSelection();
}

void CFormRcEditor::OnFileSave()
{
	if (!m_pe)
	{
		AfxMessageBox(ML(UI_MSG_PLEASE_OPEN_EXECUTABLE(gbCurNeedShowLan)), MB_ICONWARNING);
		return;
	}
	if (!m_pRes)
	{
		AfxMessageBox(ML(UI_MSG_NO_RESOURCE_TO_SAVE(gbCurNeedShowLan)), MB_ICONWARNING);
		return;
	}
	if (m_currentIniPath.IsEmpty())
	{
		AfxMessageBox(ML(UI_MSG_INVALID_PATH_USE_SAVEAS(gbCurNeedShowLan)), MB_ICONWARNING);
		return;
	}

	// Commit UI-editable copy back to PE editor model, then write.
	m_pe->SetResourceInfo(*m_pRes);

	if (m_pe->UpDatePEInfo())
	{
		AfxMessageBox(ML(UI_MSG_SAVE_SUCCESS(gbCurNeedShowLan)), MB_ICONINFORMATION);
		return;
	}
	AfxMessageBox(ML(UI_MSG_SAVE_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
}

void CFormRcEditor::OnFileSaveAs()
{
	if (!m_pe)
	{
		AfxMessageBox(ML(UI_MSG_PLEASE_OPEN_EXECUTABLE(gbCurNeedShowLan)), MB_ICONWARNING);
		return;
	}
	if (!m_pRes)
	{
		AfxMessageBox(ML(UI_MSG_NO_RESOURCE_TO_SAVE(gbCurNeedShowLan)), MB_ICONWARNING);
		return;
	}
	CString path;
	if (!PickSaveFile(path))
		return;

	// Commit UI-editable copy back to PE editor model, then write.
	m_pe->SetResourceInfo(*m_pRes);

	if (m_pe->UpDatePEInfoAs(path))
	{
		m_currentIniPath = path;
		AfxMessageBox(ML(UI_MSG_SAVEAS_SUCCESS(gbCurNeedShowLan)), MB_ICONINFORMATION);
		RebuildTree();
		return;
	}
	AfxMessageBox(ML(UI_MSG_SAVEAS_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
}

void CFormRcEditor::OnFileExit()
{
	EndDialog(IDOK);
}


void CFormRcEditor::OnSearchChanged()
{
	if (!m_editSearch.GetSafeHwnd())
		return;
	m_editSearch.GetWindowText(m_searchText);
    
    // Update search button text: show "Clear" when search box has text, "Search" otherwise
    if (m_btnSearch.GetSafeHwnd())
    {
        CString trimmed = m_searchText;
        trimmed.Trim();
        m_btnSearch.icon = trimmed.IsEmpty() ? ParameterEditorStyle::CButton::Search : ParameterEditorStyle::CButton::Close;
        m_btnSearch.Invalidate(FALSE);
        if (trimmed.IsEmpty())
            m_btnSearch.SetWindowText(ML(UI_SEARCH(gbCurNeedShowLan)));
        else
            m_btnSearch.SetWindowText(ML(UI_CLEAR(gbCurNeedShowLan)));
    }
    
    // Preserve default behavior for simple rebuilds, but allow derived classes to override more
    // advanced search handling without changing base semantics.
    OnSearchTextChanged();
}

void CFormRcEditor::OnSearchTextChanged()
{
    // Default implementation: rebuild tree using existing filter text.
    // For complex search, rebuild a temporary tree showing only matches.
    // RebuildTree handles filter via m_searchText; just call it.
    
    // 设置搜索模式标志：有搜索文本时为搜索模式
    CString trimmed = m_searchText;
    trimmed.Trim();
    m_isSearchMode = !trimmed.IsEmpty();
    
    RebuildSearchTree();
}

// 默认的搜索树重建实现：调用原型树重建逻辑（使用 m_searchText 过滤）
// 派生类可覆盖此方法以实现自定义搜索逻辑
void CFormRcEditor::RebuildSearchTree()
{
    // 基类默认实现：使用已有的 RebuildTree 逻辑（内部已支持 m_searchText 过滤）
    RebuildTree();
}

// 根据当前模式刷新树：搜索模式刷新搜索树，否则刷新原型树
void CFormRcEditor::RefreshCurrentTree()
{
    if (m_isSearchMode)
        RebuildSearchTree();
    else
        RebuildTree();
}

// Append search matches for menus/toolbars when performing search-based tree
bool CFormRcEditor::AppendSearchMatchesForResource(const std::shared_ptr<rceditor::SSingleResourceInfo>& res,
    const CString& filePath, const CString& searchLower, HTREEITEM hRoot)
{
    if (!res)
        return false;

    bool any = false;
    // Search menus
    // 在等级管理器模式（m_showLevelUi == true）中不显示/搜索菜单信息，仅显示对话框和工具栏
    if (!m_showLevelUi)
    {
        for (const auto& kv : res->mapLangAndMenuInfo)
        {
            auto menu = kv.second;
            if (!menu) continue;
            CString txt = CString(menu->csText).MakeLower();
            CString idStr; idStr.Format(_T("%u"), menu->nID);
            if (txt.Find(searchLower) >= 0 || idStr.Find(searchLower) >= 0)
            {
                any = true;
                CString nodeText; nodeText.Format(_T("%s (%u)"), CString(menu->csText), menu->nID);
                auto nd = std::make_unique<TreeNodeData>();
                nd->kind = TreeNodeData::NK_Menu;
                nd->dlgId = menu->nID;
                nd->ctrlId = 0;
                nd->filePath = filePath;
                TreeNodeData* pNd = nd.get();
                m_treeNodeData.push_back(std::move(nd));

                HTREEITEM hMenu = m_tree.InsertItem(nodeText, hRoot, TVI_LAST);
                m_tree.SetItemData(hMenu, (DWORD_PTR)pNd);

                // add first-level submenu items (flat) to help locate matches
                for (const auto& sub : menu->vecSubMenu)
                {
                    if (!sub) continue;
                    CString sText = CString(sub->csText).MakeLower();
                    CString sId; sId.Format(_T("%u"), sub->nID);
                    if (sText.Find(searchLower) >= 0 || sId.Find(searchLower) >= 0)
                    {
                        auto sNd = std::make_unique<TreeNodeData>();
                        sNd->kind = TreeNodeData::NK_Menu;
                        sNd->dlgId = sub->nID;
                        sNd->ctrlId = 0;
                        sNd->filePath = filePath;
                        TreeNodeData* ps = sNd.get();
                        m_treeNodeData.push_back(std::move(sNd));

                        CString subNode; subNode.Format(_T("%s (%u)"), CString(sub->csText), sub->nID);
                        HTREEITEM hItem = m_tree.InsertItem(subNode, hMenu, TVI_LAST);
                        m_tree.SetItemData(hItem, (DWORD_PTR)ps);
                    }
                }
            }
        }
    }

    // Search toolbars
    for (const auto& kv : res->mapLangAndToolbarInfo)
    {
        auto tb = kv.second;
        if (!tb || (m_showLevelUi && ShouldIgnoreLevelAuditToolbar(filePath, tb->nToolbarID))) continue;
        CString idStr; idStr.Format(_T("%u"), tb->nToolbarID);
        CString nodeText;
        if (m_showLevelUi)
            nodeText = GetLevelAuditToolbarText(filePath, *tb, ML(UI_TOOLBARS(gbCurNeedShowLan)));
        else
            nodeText.Format(_T("Toolbar (%u)"), tb->nToolbarID);
        CString nameLower = nodeText;
        nameLower.MakeLower();
        if (idStr.Find(searchLower) >= 0 || (m_showLevelUi && nameLower.Find(searchLower) >= 0))
        {
            any = true;
            auto nd = std::make_unique<TreeNodeData>();
            nd->kind = TreeNodeData::NK_Toolbar;
            nd->dlgId = tb->nToolbarID;
            nd->ctrlId = 0;
            nd->filePath = filePath;
            TreeNodeData* pNd = nd.get();
            m_treeNodeData.push_back(std::move(nd));

            HTREEITEM hTb = m_tree.InsertItem(nodeText, hRoot, TVI_LAST);
            m_tree.SetItemData(hTb, (DWORD_PTR)pNd);
        }
    }

    // Search dynamic dialogs separately and place matches under a DynamicWnds category
    if (!res->mapDynamicDialogInfo.empty())
    {
        HTREEITEM hCatDynamics = nullptr;
        for (const auto& kv : res->mapDynamicDialogInfo)
        {
            auto dlg = kv.second;
            if (!dlg || (m_showLevelUi && ShouldIgnoreLevelAuditDialog(filePath, *dlg))) continue;
            CString txt = CString(dlg->csCaption).MakeLower();
            CString idStr; idStr.Format(_T("%u"), dlg->nIDD);
            if (txt.Find(searchLower) >= 0 || idStr.Find(searchLower) >= 0)
            {
                any = true;
                if (!hCatDynamics)
                {
                    auto catNd = std::make_unique<TreeNodeData>();
                    catNd->kind = TreeNodeData::NK_Category;
                    catNd->dlgId = 0;
                    catNd->ctrlId = (UINT)~0u;
                    catNd->filePath = filePath;
                    TreeNodeData* pCatNd = catNd.get();
                    m_treeNodeData.push_back(std::move(catNd));

                    hCatDynamics = m_tree.InsertItem(ML(u8"DynamicWnds"), hRoot, TVI_LAST);
                    m_tree.SetItemData(hCatDynamics, (DWORD_PTR)pCatNd);
                }

                CString nodeText;
                if (m_showLevelUi)
                    nodeText = CString(dlg->csCaption);
                else
                    nodeText.Format(_T("%s (%u)"), CString(dlg->csCaption), dlg->nIDD);
                auto nd = std::make_unique<TreeNodeData>();
                nd->kind = TreeNodeData::NK_DynamicWnd;
                nd->dlgId = dlg->nIDD;
                nd->ctrlId = 0;
                nd->filePath = filePath;
                TreeNodeData* pNd = nd.get();
                m_treeNodeData.push_back(std::move(nd));

                HTREEITEM hItem = m_tree.InsertItem(nodeText, GetLevelAuditParent(hRoot, hCatDynamics, filePath, *dlg), TVI_LAST);
                m_tree.SetItemData(hItem, (DWORD_PTR)pNd);
            }
        }
    }

    return any;
}

void CFormRcEditor::OnSearchClicked()
{
	CString s;
	m_editSearch.GetWindowText(s);
	s.Trim();
	
	// If search box has text, clicking the button clears it
	if (!s.IsEmpty())
	{
		m_editSearch.SetWindowText(_T(""));
		m_searchText.Empty();
		if (m_btnSearch.GetSafeHwnd())
			m_btnSearch.SetWindowText(ML(UI_SEARCH(gbCurNeedShowLan)));
		RebuildTree();
		return;
	}

	// If search box is empty, select first match in tree (legacy behavior)
	HTREEITEM root = m_tree.GetRootItem();
	if (!root)
		return;
	HTREEITEM child = m_tree.GetChildItem(root);
	if (child)
		m_tree.SelectItem(child);
}

void CFormRcEditor::OnCheckAllClicked()
{
	if (!m_surface)
		return;
	if (m_inBadgeUpdate)
		return;

    // Delegate to virtual hook so derived classes (LevelManager) can implement behavior.
    HandleCheckAllClicked();
}

void CFormRcEditor::OnBadgeSelChange()
{
    HandleBadgeSelChange();
}

LRESULT CFormRcEditor::OnDesignSurfaceNotify(
	WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (m_closing)
		return 0;

	// Keep surface bound to the canonical model instance stored in `m_pRes`.
	// This prevents UI/model drift (tree check <-> m_blIsInLevelManager).
	if (m_surface && m_pRes)
	{
		// If surface is showing a toolbar selection, do not attempt to rebind dialog
		// info from the surface back into the model. Rebinding here can override
		// an intentional toolbar preview selection and cause the tree to jump to
		// the dialog's first child.
		if (!m_surface->IsToolbarSelected())
		{
			auto cur = m_surface->GetDialogInfo();
			if (cur)
			{
				auto it = m_pRes->mapLangAndDialogInfo.find(cur->nIDD);
				if (it != m_pRes->mapLangAndDialogInfo.end() && it->second && it->second != cur)
				{
					m_surface->SetDialogInfo(it->second);
					m_surface->SetCurrentLevel(m_curLevel);
				}
			}
		}
	}
	SyncTreeSelectionFromSurface();
	UpdatePropertyGrid();
	UpdatePropertyList();  // 同步更新属性列表控件
	UpdateBadgeUiFromSelection();
	return 0;
}

// 将 UpdateBadgeUiFromSelection 放在文件末尾以便读取到所有成员定义。
// (Removed heavy save/restore implementations; tree recreation is simple and
// does not attempt to preserve expanded/selection state.)

// 属性列表专用的属性ID常量
namespace PropListPID
{
    const DWORD_PTR PID_DLG_CAPTION = 1001;
    const DWORD_PTR PID_DLG_INLEVEL = 1101;
    const DWORD_PTR PID_CTRL_TEXT = 2002;
    const DWORD_PTR PID_CTRL_MULTILINE = 2101;   // 多行显示属性
    const DWORD_PTR PID_CTRL_LEVEL_ENABLED = 2102;
    const DWORD_PTR PID_MULTI_LEVEL_BASE = 3100;  // 多选时每个等级的属性ID基址
    const DWORD_PTR PID_TB_INLEVEL = 9004;
    const DWORD_PTR PID_TB_ITEM_LEVEL_BASE = 9100;  // 工具栏项每个等级的属性ID基址
}

// 更新属性列表控件的显示内容
void CFormRcEditor::UpdatePropertyList()
{
    if (!m_propList.GetSafeHwnd())
        return;
    if (m_closing)
        return;

    m_propList.RemoveAllProperties();

    if (!m_surface)
        return;

    auto dlg = m_surface->GetDialogInfo();
    auto selCtrl = m_surface->GetSelectedCtrl();
    auto sel = m_surface->GetMultiSelectedIndices();
    const bool dialogSelected = m_surface->IsDialogSelected() || (!selCtrl && sel.empty() && dlg);

    // 检查是否正在显示工具栏
    auto activeTb = m_surface->GetActiveToolbar();
    auto tbSel = m_surface->GetSelectedToolbarIndices();

    // 工具栏模式
    if (activeTb && !tbSel.empty())
    {
        m_propList.AddCategory(ML(UI_PROPLIST_TOOLBAR(gbCurNeedShowLan)));
        
        CString tbIdStr;
        tbIdStr.Format(_T("%u"), activeTb->nToolbarID);
        m_propList.AddProperty(ML(UI_PROPLIST_TOOLBAR_ID(gbCurNeedShowLan)), tbIdStr, 0, true);
        m_propList.AddBoolProperty(ML(UI_PROPLIST_IN_LEVEL_MANAGER(gbCurNeedShowLan)), activeTb->m_blIsInLevelManager, PropListPID::PID_TB_INLEVEL);

        if (tbSel.size() == 1)
        {
            // 单选工具栏按钮
            int idx = tbSel.front();
            if (idx >= 0 && (size_t)idx < activeTb->vecItemIDs.size())
            {
                m_propList.AddCategory(ML(UI_PROPLIST_BUTTON_INFO(gbCurNeedShowLan)));
                
                CString itemIdStr;
                itemIdStr.Format(_T("%u"), activeTb->vecItemIDs[idx]);
                m_propList.AddProperty(ML(UI_PROPLIST_BUTTON_ID(gbCurNeedShowLan)), itemIdStr, 0, true);
                
                // 显示按钮文本（从 RT_STRING 提取，只读）
                CString itemText;
                if ((size_t)idx < activeTb->vecItemText.size())
                    itemText = CString(activeTb->vecItemText[idx]);
                m_propList.AddProperty(ML(UI_PROPLIST_BUTTON_TEXT(gbCurNeedShowLan)), itemText, 0, true);

                // 等级掩码
                m_propList.AddCategory(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)));
                UINT mask = 0;
                if ((size_t)idx < activeTb->vecLevelMask.size())
                    mask = activeTb->vecLevelMask[idx];
                
                for (int lvl = 0; lvl < 8; ++lvl)
                {
                    CString label;
                    label.Format(ML(UI_PROPLIST_LEVEL_FMT(gbCurNeedShowLan)), GetLevelDisplayNumber(lvl));
                    bool enabled = (mask & (1u << lvl)) != 0;
                    m_propList.AddBoolProperty(label, enabled, PropListPID::PID_TB_ITEM_LEVEL_BASE + lvl);
                }
            }
        }
        else
        {
            // 多选工具栏按钮
            m_propList.AddCategory(ML(UI_PROPLIST_MULTI_BUTTONS(gbCurNeedShowLan)));
            
            CString countStr;
            countStr.Format(_T("%d"), (int)tbSel.size());
            m_propList.AddProperty(ML(UI_PROPLIST_SELECT_COUNT(gbCurNeedShowLan)), countStr, 0, true);

            // 计算共享等级掩码
            unsigned int sharedAll = ~0u;
            unsigned int sharedAny = 0u;
            for (int idx : tbSel)
            {
                if (idx < 0 || (size_t)idx >= activeTb->vecItemIDs.size())
                    continue;
                UINT m = 0;
                if ((size_t)idx < activeTb->vecLevelMask.size())
                    m = activeTb->vecLevelMask[idx];
                sharedAll &= m;
                sharedAny |= m;
            }

            m_propList.AddCategory(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)));
            for (int lvl = 0; lvl < 8; ++lvl)
            {
                CString label;
                label.Format(ML(UI_PROPLIST_LEVEL_FMT(gbCurNeedShowLan)), GetLevelDisplayNumber(lvl));
                bool onAll = (sharedAll & (1u << lvl)) != 0;
                m_propList.AddBoolProperty(label, onAll, PropListPID::PID_TB_ITEM_LEVEL_BASE + lvl);
            }
        }
        return;
    }

    // 多选控件模式
    if (sel.size() > 1 && dlg)
    {
        m_propList.AddCategory(ML(UI_PROPLIST_MULTI_SELECT(gbCurNeedShowLan)));
        
        CString countStr;
        countStr.Format(_T("%d"), (int)sel.size());
        m_propList.AddProperty(ML(UI_PROPLIST_SELECT_COUNT(gbCurNeedShowLan)), countStr, 0, true);

        // 等级设置：仅在等级管理器界面时显示
        if (m_showLevelUi)
        {
            // 计算共享等级掩码
            unsigned int sharedAll = ~0u;
            unsigned int sharedAny = 0u;
            for (int i : sel)
            {
                if (i < 0 || i >= (int)dlg->vecCtrlInfo.size())
                    continue;
                auto c = dlg->vecCtrlInfo[i];
                if (!c)
                    continue;
                sharedAll &= c->uiLevelMask;
                sharedAny |= c->uiLevelMask;
            }

            m_propList.AddCategory(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)));
            for (int lvl = 0; lvl < 8; ++lvl)
            {
                CString label;
                label.Format(ML(UI_PROPLIST_LEVEL_FMT(gbCurNeedShowLan)), GetLevelDisplayNumber(lvl));
                bool onAll = (sharedAll & (1u << lvl)) != 0;
                m_propList.AddBoolProperty(label, onAll, PropListPID::PID_MULTI_LEVEL_BASE + lvl);
            }
        }
        return;
    }

    // 显示基本信息
    m_propList.AddCategory(ML(UI_PROPLIST_BASIC_INFO(gbCurNeedShowLan)));

    if (dlg && dialogSelected)
    {
        // 对话框属性
        m_propList.AddProperty(ML(UI_PROPLIST_TYPE(gbCurNeedShowLan)), ML(UI_DIALOG(gbCurNeedShowLan)), 0, true);
        // 标题属性：在等级管理器模式下为只读，否则可编辑
        m_propList.AddProperty(ML(UI_PROPLIST_TITLE(gbCurNeedShowLan)), CString(dlg->csCaption), PropListPID::PID_DLG_CAPTION, m_showLevelUi, !m_showLevelUi);
        
        CString idStr;
        idStr.Format(_T("%u"), dlg->nIDD);
        m_propList.AddProperty(ML(UI_ID(gbCurNeedShowLan)), idStr, 0, true);
        
        CString rc;
        rc.Format(_T("%d,%d,%d,%d"), dlg->rectDlg.left, dlg->rectDlg.top, 
            dlg->rectDlg.Width(), dlg->rectDlg.Height());
        m_propList.AddProperty(ML(UI_PROPLIST_POSITION(gbCurNeedShowLan)), rc, 0, true);
        
        // 非资源属性：仅在等级管理器界面时显示
        if (m_showLevelUi)
        {
            m_propList.AddCategory(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)));
            m_propList.AddBoolProperty(ML(UI_PROPLIST_IN_LEVEL_MANAGER(gbCurNeedShowLan)), dlg->m_blIsInLevelManager, PropListPID::PID_DLG_INLEVEL);
        }
    }
    else if (dlg && selCtrl)
    {
        // 控件属性
        m_propList.AddProperty(ML(UI_PROPLIST_TYPE(gbCurNeedShowLan)), ML(UI_CONTROL(gbCurNeedShowLan)), 0, true);
        m_propList.AddProperty(ML(UI_PROPLIST_CLASSNAME(gbCurNeedShowLan)), CString(selCtrl->csClassName), 0, true);
        // 文本属性：在等级管理器模式下为只读，否则可编辑
        m_propList.AddProperty(ML(UI_TEXT(gbCurNeedShowLan)), CString(selCtrl->csText), PropListPID::PID_CTRL_TEXT, m_showLevelUi, !m_showLevelUi);
        
        CString idStr;
        idStr.Format(_T("%u"), selCtrl->nIDC);
        m_propList.AddProperty(ML(UI_ID(gbCurNeedShowLan)), idStr, 0, true);
        
        CString rc;
        rc.Format(_T("%d,%d,%d,%d"), selCtrl->ctrlRect.left, selCtrl->ctrlRect.top,
            selCtrl->ctrlRect.Width(), selCtrl->ctrlRect.Height());
        m_propList.AddProperty(ML(UI_PROPLIST_POSITION(gbCurNeedShowLan)), rc, 0, true);
        
        // 多行显示属性（仅对有具体文本的控件如STATIC、BUTTON显示，EDIT不需要）
        // 在等级管理器模式下为只读
        CString className = CString(selCtrl->csClassName);
        className.MakeUpper();
        if (className == _T("STATIC") || className == _T("BUTTON"))
        {
            if (m_showLevelUi)
            {
                // 等级管理器模式：只读显示多行属性
                m_propList.AddProperty(ML(UI_PROPLIST_MULTILINE(gbCurNeedShowLan)), 
                    selCtrl->blMultiLineEditCtrl ? ML(UI_PROPLIST_YES(gbCurNeedShowLan)) : ML(UI_PROPLIST_NO(gbCurNeedShowLan)), 
                    0, true);
            }
            else
            {
                // 普通模式：可编辑多行属性
                m_propList.AddBoolProperty(ML(UI_PROPLIST_MULTILINE(gbCurNeedShowLan)), selCtrl->blMultiLineEditCtrl, PropListPID::PID_CTRL_MULTILINE);
            }
        }
        
        // 等级管理属性：仅在等级管理器界面时显示
        if (m_showLevelUi)
        {
            m_propList.AddCategory(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)));
            int levelIdx = std::max(0, m_curLevel - 1);
            bool enabled = selCtrl->GetLevel(levelIdx);
            m_propList.AddBoolProperty(ML(UI_PROPLIST_CURRENT_LEVEL_ENABLED(gbCurNeedShowLan)), enabled, PropListPID::PID_CTRL_LEVEL_ENABLED);
            
            // 显示所有等级
            for (int lvl = 0; lvl < 8; ++lvl)
            {
                CString label;
                label.Format(ML(UI_PROPLIST_LEVEL_FMT(gbCurNeedShowLan)), GetLevelDisplayNumber(lvl));
                bool lvlEnabled = selCtrl->GetLevel(lvl);
                m_propList.AddBoolProperty(label, lvlEnabled, PropListPID::PID_MULTI_LEVEL_BASE + lvl);
            }
        }
    }
    else
    {
        m_propList.AddProperty(ML(UI_SELECTION(gbCurNeedShowLan)), ML(UI_PROPLIST_NONE(gbCurNeedShowLan)), 0, true);
    }
}

// 属性列表变更回调处理
void CFormRcEditor::OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item)
{
    if (m_closing || m_inPropUpdate)
        return;

    if (!m_surface)
        return;

    auto dlg = m_surface->GetDialogInfo();
    auto selCtrl = m_surface->GetSelectedCtrl();
    auto sel = m_surface->GetMultiSelectedIndices();
    auto activeTb = m_surface->GetActiveToolbar();
    auto tbSel = m_surface->GetSelectedToolbarIndices();

    DWORD_PTR pid = item.dwData;

    // 工具栏相关属性（包含动态窗口的伪工具栏）
    if (pid == PropListPID::PID_TB_INLEVEL)
    {
        if (activeTb)
        {
            activeTb->m_blIsInLevelManager = item.bBoolValue;
            if (m_pRes)
            {
                auto itTb = m_pRes->mapLangAndToolbarInfo.find(activeTb->nToolbarID);
                if (itTb != m_pRes->mapLangAndToolbarInfo.end() && itTb->second)
                {
                    itTb->second->m_blIsInLevelManager = item.bBoolValue;
                }
                else if (m_activeDynamicDialog && (int)m_activeDynamicDialog->nIDD == activeTb->nToolbarID)
                {
                    m_activeDynamicDialog->m_blIsInLevelManager = item.bBoolValue;
                }
            }
            m_surface->SetToolbarInfo(activeTb);
            m_surface->Invalidate(FALSE);
            // Sync tree checkbox visuals without rebuilding the whole tree to preserve
            // selection/expanded state. Update property list only to reflect changes.
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            UpdatePropertyList();
        }
        return;
    }

    // 工具栏按钮等级设置
    if (pid >= PropListPID::PID_TB_ITEM_LEVEL_BASE && pid < PropListPID::PID_TB_ITEM_LEVEL_BASE + 32)
    {
        if (activeTb && !tbSel.empty())
        {
            int lvl = (int)(pid - PropListPID::PID_TB_ITEM_LEVEL_BASE);
            const UINT bit = (1u << lvl);
            
            for (int idx : tbSel)
            {
                if (idx < 0 || (size_t)idx >= activeTb->vecItemIDs.size())
                    continue;
                if (activeTb->vecLevelMask.size() <= (size_t)idx)
                    activeTb->vecLevelMask.resize(activeTb->vecItemIDs.size(), 0);
                
                if (item.bBoolValue)
                    activeTb->vecLevelMask[idx] |= bit;
                else
                    activeTb->vecLevelMask[idx] &= ~bit;
            }
            
            // 同步到模型或动态窗口
            if (m_pRes)
            {
                auto itTb = m_pRes->mapLangAndToolbarInfo.find(activeTb->nToolbarID);
                if (itTb != m_pRes->mapLangAndToolbarInfo.end() && itTb->second)
                    itTb->second->vecLevelMask = activeTb->vecLevelMask;
                else if (m_activeDynamicDialog && (int)m_activeDynamicDialog->nIDD == activeTb->nToolbarID)
                {
                    for (size_t i = 0; i < m_activeDynamicDialog->vecCtrlInfo.size() && i < activeTb->vecLevelMask.size(); ++i)
                    {
                        auto c = m_activeDynamicDialog->vecCtrlInfo[i];
                        if (c)
                            c->uiLevelMask = activeTb->vecLevelMask[i];
                    }
                }
            }
            
            m_surface->Invalidate(FALSE);
            // Avoid full tree rebuild to preserve user's multi-selection/expanded state.
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            UpdatePropertyList();
        }
        return;
    }

    // 多选控件等级设置
    if (pid >= PropListPID::PID_MULTI_LEVEL_BASE && pid < PropListPID::PID_MULTI_LEVEL_BASE + 32)
    {
        int lvl = (int)(pid - PropListPID::PID_MULTI_LEVEL_BASE);
        const UINT bit = (1u << lvl);
        
        // 如果是单选控件，处理单个控件
        if (sel.size() <= 1 && selCtrl)
        {
            if (item.bBoolValue)
                selCtrl->uiLevelMask |= bit;
            else
                selCtrl->uiLevelMask &= ~bit;
            
            m_surface->Invalidate(FALSE);
			if (GetSafeHwnd())
				PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
			UpdatePropertyList();
            return;
        }
        
        // 多选控件
        if (dlg && sel.size() > 1)
        {
            for (int i : sel)
            {
                if (i < 0 || i >= (int)dlg->vecCtrlInfo.size())
                    continue;
                auto c = dlg->vecCtrlInfo[i];
                if (!c)
                    continue;
                
                if (item.bBoolValue)
                    c->uiLevelMask |= bit;
                else
                    c->uiLevelMask &= ~bit;
            }
            // Do not rebuild the tree or alter selection/checkbox visuals.
            // Only refresh surface, property list and badge UI so multi-selection
            // editing remains intact and visible to the user.
            // Avoid rebinding the dialog to surface here because SetDialogInfo may
            // clear the surface multi-selection state. Only refresh level and redraw
            // so the visual state (including multi-selection) is preserved.
            m_surface->SetCurrentLevel(m_curLevel);
            m_surface->Invalidate(FALSE);
            UpdatePropertyList();
            UpdateBadgeUiFromSelection();
        }
        return;
    }

    switch (pid)
    {
    case PropListPID::PID_DLG_CAPTION:
        if (dlg)
        {
            m_surface->SetDialogCaption(CStringW(item.strValue));
        }
        break;

    case PropListPID::PID_DLG_INLEVEL:
        if (dlg)
        {
            dlg->m_blIsInLevelManager = item.bBoolValue;
            m_surface->SetDialogInfo(dlg);
            m_surface->SetCurrentLevel(m_curLevel);
            m_surface->Invalidate(FALSE);
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            UpdatePropertyList();
        }
        break;

    case PropListPID::PID_CTRL_TEXT:
        if (selCtrl)
        {
            m_surface->SetCtrlText(selCtrl->nIDC, CStringW(item.strValue));
        }
        break;

    case PropListPID::PID_CTRL_MULTILINE:
        if (selCtrl)
        {
            m_surface->SetCtrlMultiLine(selCtrl->nIDC, item.bBoolValue);
            m_surface->Invalidate(FALSE);
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            UpdatePropertyList();
        }
        break;

    case PropListPID::PID_CTRL_LEVEL_ENABLED:
        if (selCtrl && dlg)
        {
            int levelIdx = std::max(0, m_curLevel - 1);
            const UINT mask = (1u << levelIdx);
            if (item.bBoolValue)
                selCtrl->uiLevelMask |= mask;
            else
                selCtrl->uiLevelMask &= ~mask;
            m_surface->Invalidate(FALSE);
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            UpdatePropertyList();
        }
        break;

    default:
        break;
    }
}

void CFormRcEditor::UpdateBadgeUiFromSelection()
{
    // 防重入
    if (m_inBadgeUpdate)
        return;
    m_inBadgeUpdate = true;

    // 等级是整个窗口的预览条件，不依赖当前选中了文件、分组还是具体控件。
    if (m_comboBadge.GetSafeHwnd())
    {
        if (m_curLevel >= 1 && m_curLevel <= 8)
            m_comboBadge.SetCurSel(m_curLevel - 1);
        m_comboBadge.EnableWindow(TRUE);
    }

    if (!m_surface)
    {
        if (m_checkAll.GetSafeHwnd())
            m_checkAll.EnableWindow(FALSE);
        m_inBadgeUpdate = false;
        return;
    }

    if (!m_checkAll.GetSafeHwnd() || !m_comboBadge.GetSafeHwnd())
    {
        m_inBadgeUpdate = false;
        return;
    }

    auto dlg = m_surface->GetDialogInfo();
    auto activeTb = m_surface->GetActiveToolbar();
    auto sel = m_surface->GetMultiSelectedIndices();

    // If no dialog is bound but a toolbar preview is active, treat the toolbar as the
    // current selection context so badge UI remains usable when editing toolbars.
    if (!dlg && activeTb)
    {
        // Map toolbar selection to badge state. The checkbox represents whether
        // all toolbar items are enabled for the current level when all items
        // are selected. For partial selection (non-empty but not all items)
        // show the indeterminate state. Selecting the toolbar node itself
        // (no per-item selection) reflects the toolbar-level participation flag.
        auto tbSel = m_surface->GetSelectedToolbarIndices();
        const size_t totalItems = activeTb->vecItemIDs.size();
        const size_t selCount = tbSel.size();

        // If no per-item selection, show toolbar-level flag
        if (selCount == 0)
        {
            m_checkAll.EnableWindow(TRUE);
            m_checkAll.SetCheck(activeTb->m_blIsInLevelManager ? BST_CHECKED : BST_UNCHECKED);
            m_inBadgeUpdate = false;
            return;
        }

        int have = 0;
        for (int idx : tbSel)
        {
            if (idx < 0 || (size_t)idx >= activeTb->vecItemIDs.size())
                continue;
            if ((size_t)idx < activeTb->vecLevelMask.size() && activeTb->vecLevelMask[idx] != 0)
                ++have;
        }

        // If selection covers all toolbar items, evaluate fully; otherwise show indeterminate
        if (selCount == totalItems && totalItems > 0)
        {
            m_checkAll.EnableWindow(TRUE);
            if (have == 0)
                m_checkAll.SetCheck(BST_UNCHECKED);
            else if (have == (int)totalItems)
                m_checkAll.SetCheck(BST_CHECKED);
            else
                m_checkAll.SetCheck(BST_INDETERMINATE);
        }
        else
        {
            // partial selection -> indeterminate (unless nothing selected which was handled)
            m_checkAll.EnableWindow(TRUE);
            m_checkAll.SetCheck(BST_INDETERMINATE);
        }


        m_inBadgeUpdate = false;
        return;
    }

    if (!dlg)
    {
        // 没有预览对象时只禁用全选；仍可先选等级再勾选左侧分组。
        m_checkAll.EnableWindow(FALSE);
        m_checkAll.SetCheck(BST_UNCHECKED);
        m_inBadgeUpdate = false;
        return;
    }

    if (sel.empty())
    {
        // 有绑定对话框但无选区：允许用户使用全选或切换等级，因此保持控件可用
        m_checkAll.EnableWindow(TRUE);
        if (!m_preserveBadgeCheck)
            m_checkAll.SetCheck(BST_UNCHECKED);
        m_inBadgeUpdate = false;
        return;
    }

    const int levelIdx = std::max(0, std::min(7, m_curLevel - 1));
    const UINT bit = (1u << levelIdx);
    int selCount = 0;
    int have = 0;
    const int totalControls = (dlg ? (int)dlg->vecCtrlInfo.size() : 0);
    for (int i : sel)
    {
        if (i < 0 || i >= (int)dlg->vecCtrlInfo.size())
            continue;
        auto c = dlg->vecCtrlInfo[i];
        if (!c)
            continue;
        ++selCount;
        if ((c->uiLevelMask & bit) != 0)
            ++have;
    }

    if (selCount == 0)
    {
        m_checkAll.SetCheck(BST_UNCHECKED);
        m_checkAll.EnableWindow(FALSE);
    }
    else
    {
        // If selection covers all controls in the dialog, evaluate fully; otherwise show indeterminate
        if (selCount == totalControls && totalControls > 0)
        {
            m_checkAll.EnableWindow(TRUE);
            if (!m_preserveBadgeCheck)
            {
                if (have == 0)
                    m_checkAll.SetCheck(BST_UNCHECKED);
                else if (have == totalControls)
                    m_checkAll.SetCheck(BST_CHECKED);
                else
                    m_checkAll.SetCheck(BST_INDETERMINATE);
            }
        }
        else
        {
            // partial selection -> indeterminate
            m_checkAll.EnableWindow(TRUE);
            if (!m_preserveBadgeCheck)
                m_checkAll.SetCheck(BST_INDETERMINATE);
        }
    }
    m_inBadgeUpdate = false;
}


