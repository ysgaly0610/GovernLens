/*!
 * \file CFormRcBase.cpp
 * \brief CFormRcBase 的实现
 */
#include "stdafx.h"
#include "CFormRcBase.h"
#include <dwmapi.h>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

namespace ParamRange {

IMPLEMENT_DYNAMIC(CFormNoRcBase, CDialog)

CStringW CFormNoRcBase::NormalizeJsonDisplayPath(const CString& path)
{
    const CStringW original(path);
    if (original.IsEmpty()) return original;
    if (original.Left(4) == L"\\\\?\\" || original.Left(4) == L"\\\\.\\")
        return original;

    const DWORD capacity = ::GetFullPathNameW(original, 0, nullptr, nullptr);
    if (capacity == 0) return original;
    std::vector<wchar_t> buffer(capacity);
    const DWORD length = ::GetFullPathNameW(original, capacity, buffer.data(), nullptr);
    if (length == 0 || length >= capacity) return original;
    return CStringW(buffer.data());
}

/*! \brief 构造函数：分配对话模板缓冲区 */
CFormNoRcBase::CFormNoRcBase(CWnd* pParent)
    : CDialog()
{
    m_pWndBuf = new WORD[knMaxBufferSize];
}

/*! \brief 析构函数：释放对话模板缓冲区 */
CFormNoRcBase::~CFormNoRcBase()
{
    if (m_pWndBuf)
    {
        delete[] m_pWndBuf;
        m_pWndBuf = nullptr;
    }
}

/*! \brief 在内部缓冲区构造 DLGTEMPLATE 并返回指针 */
// 在内部缓冲区构造对话模板（DLGTEMPLATE）并返回指针。
// 注意：返回的指针指向内部缓冲区，调用者不得释放或在析构后继续使用。
DLGTEMPLATE* CFormNoRcBase::CreateTemplate(DWORD dwStyle, CRect& rect, CStringW caption, DWORD dwStyleEx)
{
    if (m_pWndBuf)
        ZeroMemory(m_pWndBuf, knMaxBufferSize * sizeof(WORD));

    DLGTEMPLATE* pTemplate = reinterpret_cast<DLGTEMPLATE*>(m_pWndBuf);
    pTemplate->style = dwStyle;
    pTemplate->dwExtendedStyle = dwStyleEx;
    pTemplate->cdit = 0;
    pTemplate->x = (short)rect.left;
    pTemplate->y = (short)rect.top;
    pTemplate->cx = (short)rect.Width();
    pTemplate->cy = (short)rect.Height();

    WORD* pWord = reinterpret_cast<WORD*>(pTemplate + 1);
    *pWord++ = 0; // menu
    *pWord++ = 0; // class

    int len = caption.GetLength();
    wcscpy_s(reinterpret_cast<wchar_t*>(pWord), len + 1, caption.GetString());
    pWord += len + 1;
    return pTemplate;
}

void CFormNoRcBase::EnableParameterEditorStyle()
{
    if (m_parameterStyle) return;
    m_parameterStyle = true;
    m_parameterBackground.CreateSolidBrush(ParameterEditorStyle::Background());
    LOGFONT font = {};
    if (GetFont()) GetFont()->GetLogFont(&font);
    else ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(font), &font);
    font.lfHeight = -StylePx(17);
    font.lfWeight = FW_SEMIBOLD;
    m_parameterCaptionFont.CreateFontIndirect(&font);
    const DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
    ::DwmSetWindowAttribute(m_hWnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    ModifyStyle(WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, WS_CLIPCHILDREN,
        SWP_FRAMECHANGED);
    m_parameterClose.primary = true;
    m_parameterClose.iconOnly = true;
    m_parameterClose.icon = ParameterEditorStyle::CButton::Close;
    CString closeLabel;
    CMenu* systemMenu = GetSystemMenu(FALSE);
    if (systemMenu) systemMenu->GetMenuString(SC_CLOSE, closeLabel, MF_BYCOMMAND);
    m_parameterClose.Create(closeLabel, WS_CHILD | BS_OWNERDRAW |
        (ShowParameterCloseButton() ? WS_VISIBLE : 0),
        CRect(0, 0, 1, 1), this, ID_PARAMETER_CLOSE);
    m_parameterPathTips.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    m_parameterPathTips.SetMaxTipWidth(StylePx(900));
    m_parameterPathTips.Activate(TRUE);
    LayoutParameterCaption();
}

void CFormNoRcBase::LayoutParameterCaption()
{
    if (!m_parameterClose.GetSafeHwnd()) return;
    CRect client; GetClientRect(client);
    const int width = StylePx(42);
    m_parameterClose.MoveWindow(client.right-width, 0, width, ParameterCaptionHeight());
    UpdateParameterCaptionTooltip();
}

void CFormNoRcBase::SetParameterPathTooltip(CWnd& control, const CString& text)
{
    if (!m_parameterPathTips.GetSafeHwnd() || !control.GetSafeHwnd()) return;
    m_parameterPathTips.DelTool(&control);
    TOOLINFO info = {};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = m_hWnd;
    info.uId = reinterpret_cast<UINT_PTR>(control.GetSafeHwnd());
    info.lpszText = const_cast<LPTSTR>(text.GetString());
    m_parameterPathTips.SendMessage(TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
}

void CFormNoRcBase::UpdateParameterCaptionTooltip()
{
    if (!m_parameterPathTips.GetSafeHwnd()) return;
    CString title; GetWindowText(title);
    CRect rect; GetClientRect(rect);
    rect.right -= StylePx(48);
    rect.bottom = ParameterCaptionHeight();
    m_parameterPathTips.DelTool(this, ID_PARAMETER_CLOSE);
    m_parameterPathTips.AddTool(this, title, rect, ID_PARAMETER_CLOSE);
}

void CFormNoRcBase::SetParameterToolbarRect(const CRect& rect)
{
    if (rect == m_parameterToolbar) return;
    const CRect old(m_parameterToolbar);
    if (!rect.IsRectEmpty() && (rect.Height() != old.Height() || !m_parameterBandBrush.GetSafeHandle()))
    {
        CClientDC screen(this);
        CDC memory; memory.CreateCompatibleDC(&screen);
        CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1, rect.Height());
        CBitmap* previous = memory.SelectObject(&bitmap);
        ParameterEditorStyle::Gradient(memory, CRect(0,0,1,rect.Height()), RGB(151,44,60), RGB(105,14,27));
        memory.SelectObject(previous);
        m_parameterBandBrush.DeleteObject();
        m_parameterBandBrush.CreatePatternBrush(&bitmap);
    }
    m_parameterToolbar = rect;
    if (!old.IsRectEmpty()) InvalidateRect(old, FALSE);
    if (!rect.IsRectEmpty()) InvalidateRect(rect, FALSE);
}

void CFormNoRcBase::PaintParameterBackground(CDC& dc)
{
    CRect client; GetClientRect(client);
    dc.FillSolidRect(client, ParameterEditorStyle::Background());
    CRect caption(0, 0, client.right, ParameterCaptionHeight());
    ParameterEditorStyle::WineBand(dc, caption);
    if (!m_parameterToolbar.IsRectEmpty())
        ParameterEditorStyle::WineBand(dc, m_parameterToolbar);
    const int saved = dc.SaveDC();
    dc.SelectObject(&m_parameterCaptionFont);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(255, 255, 255));
    caption.left += StylePx(14);
    caption.right -= StylePx(48);
    CString title; GetWindowText(title);
    dc.DrawText(title, caption, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
    dc.RestoreDC(saved);
}

LRESULT CFormNoRcBase::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
    if (!m_parameterStyle) return CDialog::WindowProc(message, wp, lp);
    switch (message)
    {
    case WM_NCMOUSEMOVE:
        if (m_parameterPathTips.GetSafeHwnd())
        {
            CPoint point(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
            ScreenToClient(&point);
            MSG mouse = {};
            mouse.hwnd = m_hWnd;
            mouse.message = WM_MOUSEMOVE;
            mouse.lParam = MAKELPARAM(point.x, point.y);
            m_parameterPathTips.RelayEvent(&mouse);
            TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE | TME_NONCLIENT, m_hWnd, 0 };
            ::TrackMouseEvent(&track);
        }
        break;
    case WM_NCMOUSELEAVE:
    case WM_NCLBUTTONDOWN:
        if (m_parameterPathTips.GetSafeHwnd()) m_parameterPathTips.Pop();
        break;
    case WM_NCCALCSIZE:
        {
            RECT* client = wp
                ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0]
                : reinterpret_cast<RECT*>(lp);
            const LONG windowTop = client->top;
            const LRESULT result = CDialog::WindowProc(message, wp, lp);
            if (!IsZoomed()) client->top = windowTop;
            return result;
        }
    case WM_DWMCOMPOSITIONCHANGED:
        {
            const DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
            ::DwmSetWindowAttribute(m_hWnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
            RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
            return 0;
        }
    case WM_NCACTIVATE:
        return DefWindowProc(message, wp, IsIconic() ? lp : -1);
    case WM_NCPAINT:
        {
            if (IsIconic()) break;
            CWindowDC dc(this);
            CRect window, client;
            GetWindowRect(&window);
            GetClientRect(&client);
            ClientToScreen(&client);
            client.OffsetRect(-window.left, -window.top);
            window.OffsetRect(-window.left, -window.top);
            dc.ExcludeClipRect(&client);
            dc.FillSolidRect(window, RGB(105, 14, 27));
            return 0;
        }
    case WM_PAINT:
        {
            CPaintDC dc(this);
            PaintParameterBackground(dc);
            return 0;
        }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        {
            CDC* dc = CDC::FromHandle(reinterpret_cast<HDC>(wp));
            CRect rect; ::GetWindowRect(reinterpret_cast<HWND>(lp), &rect);
            ScreenToClient(&rect);
            dc->SetBkMode(TRANSPARENT);
            const bool onBand = m_parameterToolbar.PtInRect(rect.CenterPoint()) != FALSE;
            dc->SetTextColor(onBand ? RGB(255,255,255) : ParameterEditorStyle::Text());
            if (onBand && m_parameterBandBrush.GetSafeHandle())
            {
                dc->SetBrushOrg(m_parameterToolbar.left-rect.left, m_parameterToolbar.top-rect.top);
                return reinterpret_cast<LRESULT>(m_parameterBandBrush.GetSafeHandle());
            }
            dc->SetBkColor(ParameterEditorStyle::Background());
            return reinterpret_cast<LRESULT>(m_parameterBackground.GetSafeHandle());
        }
    case WM_NCHITTEST:
        {
            const LRESULT hit = CDialog::WindowProc(message, wp, lp);
            if (hit != HTCLIENT) return hit;
            CPoint point(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
            ScreenToClient(&point);
            if (!IsZoomed() && point.y >= 0 && point.y < StylePx(5))
            {
                CRect client; GetClientRect(client);
                if (point.x < StylePx(10)) return HTTOPLEFT;
                if (point.x >= client.right-StylePx(10)) return HTTOPRIGHT;
                return HTTOP;
            }
            if (point.y >= 0 && point.y < ParameterCaptionHeight()) return HTCAPTION;
            return hit;
        }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_PARAMETER_CLOSE && HIWORD(wp) == BN_CLICKED)
        {
            SendMessage(WM_CLOSE);
            return 0;
        }
        break;
    case WM_SIZE:
        LayoutParameterCaption();
        Invalidate(FALSE);
        break;
    case WM_SETTEXT:
        {
            const LRESULT result = CDialog::WindowProc(message, wp, lp);
            CRect rect; GetClientRect(rect); rect.bottom = ParameterCaptionHeight();
            InvalidateRect(rect, FALSE);
            UpdateParameterCaptionTooltip();
            return result;
        }
    }
    return CDialog::WindowProc(message, wp, lp);
}

} // namespace ParamRange
