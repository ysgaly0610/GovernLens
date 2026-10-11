#pragma once

#include <afxwin.h>
#include <afxcmn.h>
#include <algorithm>

namespace ParameterEditorStyle
{
    enum FileText { NoLoadedFile, UnsavedChanges, SaveTo, SavedTo, NoSaveTarget };
    inline const wchar_t* FileStatusText(FileText text, int language)
    {
        static const wchar_t* const strings[][7] = {
            { L"\u672a\u52a0\u8f7d\u6587\u4ef6", L"No file loaded", L"\u30d5\u30a1\u30a4\u30eb\u672a\u8aad\u8fbc", L"\ub85c\ub4dc\ub41c \ud30c\uc77c \uc5c6\uc74c", L"\u0e22\u0e31\u0e07\u0e44\u0e21\u0e48\u0e44\u0e14\u0e49\u0e42\u0e2b\u0e25\u0e14\u0e44\u0e1f\u0e25\u0e4c", L"Ch\u01b0a t\u1ea3i t\u1ec7p", L"\u672a\u8f09\u5165\u6a94\u6848" },
            { L"\uff08\u672a\u4fdd\u5b58\u4fee\u6539\uff09", L" (Unsaved changes)", L"\uff08\u672a\u4fdd\u5b58\u306e\u5909\u66f4\uff09", L" (\uc800\uc7a5\ud558\uc9c0 \uc54a\uc740 \ubcc0\uacbd)", L" (\u0e01\u0e32\u0e23\u0e40\u0e1b\u0e25\u0e35\u0e48\u0e22\u0e19\u0e41\u0e1b\u0e25\u0e07\u0e17\u0e35\u0e48\u0e22\u0e31\u0e07\u0e44\u0e21\u0e48\u0e1a\u0e31\u0e19\u0e17\u0e36\u0e01)", L" (Thay \u0111\u1ed5i ch\u01b0a l\u01b0u)", L"\uff08\u672a\u5132\u5b58\u4fee\u6539\uff09" },
            { L"\u4fdd\u5b58\u5230\uff1a", L"Save to: ", L"\u4fdd\u5b58\u5148\uff1a", L"\uc800\uc7a5 \uc704\uce58: ", L"\u0e1a\u0e31\u0e19\u0e17\u0e36\u0e01\u0e44\u0e1b\u0e22\u0e31\u0e07: ", L"L\u01b0u v\u00e0o: ", L"\u5132\u5b58\u81f3\uff1a" },
            { L"\u5df2\u4fdd\u5b58\u81f3\uff1a", L"Saved to: ", L"\u4fdd\u5b58\u6e08\u307f\uff1a", L"\uc800\uc7a5\ub428: ", L"\u0e1a\u0e31\u0e19\u0e17\u0e36\u0e01\u0e41\u0e25\u0e49\u0e27\u0e17\u0e35\u0e48: ", L"\u0110\u00e3 l\u01b0u v\u00e0o: ", L"\u5df2\u5132\u5b58\u81f3\uff1a" },
            { L"\u672a\u6307\u5b9a", L"Not specified", L"\u672a\u6307\u5b9a", L"\uc9c0\uc815\ub418\uc9c0 \uc54a\uc74c", L"\u0e22\u0e31\u0e07\u0e44\u0e21\u0e48\u0e23\u0e30\u0e1a\u0e38", L"Ch\u01b0a ch\u1ec9 \u0111\u1ecbnh", L"\u672a\u6307\u5b9a" }
        };
        return strings[text][language >= 0 && language < 7 ? language : 1];
    }

    inline COLORREF Background() { return RGB(243, 243, 243); }
    inline COLORREF Text() { return RGB(35, 43, 55); }
    inline int Scale(CWnd* window, int value)
    {
        CClientDC dc(window);
        return MulDiv(value, dc.GetDeviceCaps(LOGPIXELSX), 96);
    }

    inline void Gradient(CDC& dc, const CRect& rect, COLORREF top, COLORREF bottom)
    {
        const int height = (std::max)(1, rect.Height());
        for (int y = 0; y < height; ++y)
        {
            const int denominator = (std::max)(1, height - 1);
            const COLORREF color = RGB(
                GetRValue(top) + (GetRValue(bottom) - GetRValue(top)) * y / denominator,
                GetGValue(top) + (GetGValue(bottom) - GetGValue(top)) * y / denominator,
                GetBValue(top) + (GetBValue(bottom) - GetBValue(top)) * y / denominator);
            dc.FillSolidRect(rect.left, rect.top + y, rect.Width(), 1, color);
        }
    }

    inline void WineBand(CDC& dc, const CRect& rect)
    {
        Gradient(dc, rect, RGB(151, 44, 60), RGB(105, 14, 27));
        dc.Draw3dRect(rect, RGB(196, 112, 125), RGB(91, 16, 28));
        CRect glow(rect.left + rect.Width() / 5, rect.top + 1,
            rect.right - rect.Width() / 7, rect.top + 3);
        Gradient(dc, glow, RGB(211, 93, 112), RGB(164, 53, 72));
    }

    class CButton : public ::CButton
    {
    public:
        enum Icon { NoIcon, Folder, Generate, Search, Close };
        bool primary = false;
        bool iconOnly = false;
        Icon icon = NoIcon;

    protected:
        bool m_hot = false;
        virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override
        {
            if (message == BM_SETSTYLE && (GetStyle() & BS_TYPEMASK) == BS_OWNERDRAW)
                wp = (wp & ~BS_TYPEMASK) | BS_OWNERDRAW;
            if (message == WM_MOUSEMOVE && !m_hot)
            {
                m_hot = true;
                TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, GetSafeHwnd(), 0 };
                ::TrackMouseEvent(&track);
                Invalidate(FALSE);
            }
            else if (message == WM_MOUSELEAVE)
            {
                m_hot = false;
                Invalidate(FALSE);
            }
            return ::CButton::WindowProc(message, wp, lp);
        }

        virtual void DrawItem(LPDRAWITEMSTRUCT item) override
        {
            CDC dc;
            dc.Attach(item->hDC);
            const int saved = dc.SaveDC();
            CRect rect(item->rcItem);
            const bool disabled = (item->itemState & ODS_DISABLED) != 0;
            const bool pressed = (item->itemState & ODS_SELECTED) != 0;
            const bool accent = primary && !disabled;
            COLORREF top = accent ? RGB(170, 47, 66) : RGB(255, 255, 255);
            COLORREF bottom = accent ? RGB(111, 13, 29) : RGB(232, 233, 235);
            if (m_hot && !disabled) top = accent ? RGB(196, 64, 85) : RGB(255, 245, 246);
            if (pressed) std::swap(top, bottom);
            Gradient(dc, rect, top, bottom);
            dc.Draw3dRect(rect, accent ? RGB(190, 108, 121) : RGB(185, 187, 191),
                accent ? RGB(82, 8, 22) : RGB(167, 170, 175));
            CRect inside(rect); inside.DeflateRect(1, 1);
            dc.Draw3dRect(inside, accent ? RGB(198, 102, 120) : RGB(255, 255, 255),
                accent ? RGB(131, 34, 49) : RGB(239, 239, 241));
            dc.SetBkMode(TRANSPARENT);
            const COLORREF foreground = disabled ? RGB(148, 149, 152) : accent ? RGB(255, 255, 255) : Text();
            dc.SetTextColor(foreground);
            if (GetFont()) dc.SelectObject(GetFont());
            CRect content(rect); content.DeflateRect(Scale(this, 10), 2);
            if (pressed) content.OffsetRect(1, 1);
            if (icon != NoIcon)
            {
                const int size = Scale(this, 16);
                const int x = iconOnly ? rect.CenterPoint().x - size / 2 : content.left;
                const int y = content.CenterPoint().y - size / 2;
                CPen pen(PS_SOLID, (std::max)(1, Scale(this, 2)), foreground);
                dc.SelectObject(&pen);
                dc.SelectStockObject(NULL_BRUSH);
                if (icon == Folder)
                {
                    POINT points[] = { {x,y+size/4}, {x+size/3,y+size/4}, {x+size/2,y+size/2},
                        {x+size,y+size/2}, {x+size*4/5,y+size}, {x,y+size}, {x,y+size/4} };
                    dc.Polyline(points, _countof(points));
                }
                else if (icon == Generate)
                {
                    dc.Ellipse(x+size/4, y+size/4, x+size*3/4, y+size*3/4);
                    for (int i = 0; i < 3; ++i)
                    {
                        int offset = size/4 + i*size/4;
                        dc.MoveTo(x+offset,y); dc.LineTo(x+offset,y+size/4);
                        dc.MoveTo(x+offset,y+size*3/4); dc.LineTo(x+offset,y+size);
                        dc.MoveTo(x,y+offset); dc.LineTo(x+size/4,y+offset);
                        dc.MoveTo(x+size*3/4,y+offset); dc.LineTo(x+size,y+offset);
                    }
                }
                else if (icon == Search)
                {
                    dc.Ellipse(x, y, x+size*3/4, y+size*3/4);
                    dc.MoveTo(x+size*2/3,y+size*2/3); dc.LineTo(x+size,y+size);
                }
                else if (icon == Close)
                {
                    dc.MoveTo(x+2,y+2); dc.LineTo(x+size-2,y+size-2);
                    dc.MoveTo(x+size-2,y+2); dc.LineTo(x+2,y+size-2);
                }
                dc.SelectStockObject(BLACK_PEN);
                content.left += size + Scale(this, 8);
            }
            if (!iconOnly)
            {
                CString text; GetWindowText(text);
                dc.DrawText(text, content, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT))
            {
                CRect focus(rect); focus.DeflateRect(4, 4); dc.DrawFocusRect(focus);
            }
            dc.RestoreDC(saved);
            dc.Detach();
        }
    };

    class CHeader : public CHeaderCtrl
    {
    protected:
        virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override
        {
            if (message != WM_PAINT) return CHeaderCtrl::WindowProc(message, wp, lp);
            CPaintDC dc(this);
            CRect client; GetClientRect(client);
            Gradient(dc, client, RGB(252, 252, 253), RGB(233, 234, 237));
            const int saved = dc.SaveDC();
            if (GetFont()) dc.SelectObject(GetFont());
            dc.SetTextColor(Text());
            dc.SetBkMode(TRANSPARENT);
            for (int i = 0; i < GetItemCount(); ++i)
            {
                CRect rect; GetItemRect(i, rect);
                if (rect.Width() <= 0) continue;
                TCHAR text[512] = {};
                HDITEM item = {}; item.mask = HDI_TEXT | HDI_FORMAT;
                item.pszText = text; item.cchTextMax = _countof(text);
                GetItem(i, &item);
                dc.FillSolidRect(rect.right-1, rect.top, 1, rect.Height(), RGB(218, 220, 224));
                rect.DeflateRect(Scale(this, 9), 0);
                UINT format = DT_LEFT;
                if (item.fmt & HDF_CENTER) format = DT_CENTER;
                else if (item.fmt & HDF_RIGHT) format = DT_RIGHT;
                dc.DrawText(text, rect, format | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            dc.FillSolidRect(0, client.bottom-1, client.Width(), 1, RGB(211, 213, 217));
            dc.RestoreDC(saved);
            return 0;
        }
    };
}
