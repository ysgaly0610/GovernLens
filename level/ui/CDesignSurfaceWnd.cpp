#include "stdafx.h"
#include "CDesignSurfaceWnd.h"
#include "LevelAuditIgnore.h"

#include <algorithm>
#include <cmath>

#include <commctrl.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma comment(lib, "comctl32.lib")
// AlphaBlend 用于在显示等级管理器掩码时绘制半透明覆盖层
#pragma comment(lib, "Msimg32.lib")

namespace rceditor
{
	static std::vector<int> MakeDistributedBounds(int totalW, int count)
	{
		std::vector<int> bounds;
		if (totalW <= 0 || count <= 0)
			return bounds;

		bounds.reserve((size_t)count + 1);
		bounds.push_back(0);

// 将余数像素分配到前面的 `rem` 个帧中。
// 例如：totalW=101, count=5 -> base=20, rem=1 => 宽度：21,20,20,20,20
		int base = totalW / count;
		int rem = totalW % count;
		int x = 0;
		for (int i = 0; i < count; ++i)
		{
			int w = base + (i < rem ? 1 : 0);
			x += w;
			bounds.push_back(std::min(totalW, x));
		}
		return bounds;
	}

// 当工具栏位图条的宽度小于 `templateW*count` 时，通常意味着帧之间的一些像素（间隙/分隔符）被移除或裁剪。
// 通过在 (count-1) 个间隙上分配缺失的像素，将基于 `templateW` 的逻辑帧边界映射到实际条带上。
	static std::vector<int> MakeTemplateAlignedBounds(int totalW, int count, int templateW)
	{
		std::vector<int> bounds;
		if (totalW <= 0 || count <= 0 || templateW <= 0)
			return bounds;
		if (count == 1)
		{
			bounds.push_back(0);
			bounds.push_back(totalW);
			return bounds;
		}

		const int64_t logical = (int64_t)templateW * (int64_t)count;
		if (logical <= totalW)
			return bounds;
		const int64_t missing = logical - totalW;
		const int gaps = count - 1;

		bounds.reserve((size_t)count + 1);
		for (int i = 0; i <= count; ++i)
		{
            // 计算到边界 i 为止已被移除/跳过的像素数
			int64_t skip = 0;
			if (i <= 0)
				skip = 0;
			else if (i >= count)
				skip = missing;
			else
				skip = (missing * i) / gaps;

			int64_t x = (int64_t)templateW * i - skip;
			if (x < 0) x = 0;
			if (x > totalW) x = totalW;
			bounds.push_back((int)x);
		}
		// Ensure monotonic non-decreasing bounds (guard against rounding glitches)
		for (size_t i = 1; i < bounds.size(); ++i)
		{
			if (bounds[i] < bounds[i - 1])
				bounds[i] = bounds[i - 1];
		}
		return bounds;
	}

// 使用整数映射计算每帧边界。
// 这保证所有帧宽度之和等于 totalW，并防止累积舍入误差导致的漂移。
	static std::vector<int> MakeMappedBounds(int totalW, int count)
	{
		std::vector<int> bounds;
		if (totalW <= 0 || count <= 0)
			return bounds;
		bounds.reserve((size_t)count + 1);
		for (int i = 0; i <= count; ++i)
		{
			int x = (int)((int64_t)totalW * i / count);
			bounds.push_back(x);
		}
		return bounds;
	}

// 初始化公共控件类（仅一次）
static void InitCommonControlsOnce()
	{
		static bool inited = false;
		if (inited)
			return;
		INITCOMMONCONTROLSEX icex{};
		icex.dwSize = sizeof(icex);
		icex.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_DATE_CLASSES | ICC_PROGRESS_CLASS;
		::InitCommonControlsEx(&icex);
		inited = true;
	}

// 清除并释放工具栏渲染缓存
void CRcDesignSurfaceWnd::ClearToolbarRenders()
{
    for (auto& kv : m_toolbarRenders)
    {
        auto& tr = kv.second;
        // 先销毁任何创建的按钮 HWND（它们可能引用位图）
        for (auto hw : tr.btnHwnds)
        {
            if (hw && ::IsWindow(hw))
                ::DestroyWindow(hw);
        }
        tr.btnHwnds.clear();
        for (auto hb : tr.btnBmps)
        {
            if (hb)
                ::DeleteObject(hb);
        }
        if (tr.maskBmp)
        {
            ::DeleteObject(tr.maskBmp);
            tr.maskBmp = nullptr;
        }
        tr.btnBmps.clear();
    }
    m_toolbarRenders.clear();
}

void CRcDesignSurfaceWnd::SetToolbarInfo(std::shared_ptr<SToolbarInfo> tb)
{
    // 切换到工具栏预览模式时，清除对话框数据以确保两种视图互斥。
    // 销毁所有预览项（它们可能引用旧的对话框字体 / HWND），并清空对话框模型。
    for (auto& kv : m_preview)
        DestroyPreviewItem(kv.second);
    m_preview.clear();

    m_dialog.reset();

    m_activeToolbar = tb;
    m_showingToolbar = (tb != nullptr);
    m_selectedToolbarIndex = -1;
    m_toolbarPressedIndex = -1;
    // 标记工具栏为有活动选择，以避免宿主将 surface 的选择同步回 tree
    m_selectedToolbarIndices.clear();
    if (m_showingToolbar && tb && !tb->vecItemIDs.empty())
    {
        // 默认选中第一个可用按钮以表示工具栏有选择
        m_selectedToolbarIndex = 0;
        m_selectedToolbarIndices.push_back(0);
    }
    // 清理之前的渲染缓存，避免混合多套工具栏位图缓存
    ClearToolbarRenders();
    // 显示工具栏预览时，隐藏任何真实的预览 HWND，避免它们接收输入或干扰命中测试。
    // 隐藏而不是销毁可避免昂贵的重新创建操作。
    if (m_showingToolbar && m_previewHostHwnd && ::IsWindow(m_previewHostHwnd))
    {
        ::ShowWindow(m_previewHostHwnd, SW_HIDE);
        // hide any child preview windows as well
        for (auto& kv : m_preview)
        {
            auto& it = kv.second;
            if (it.hwnd && ::IsWindow(it.hwnd))
                ::ShowWindow(it.hwnd, SW_HIDE);
        }
    }
    if (m_showingToolbar && tb)
    {
        EnsureToolbarRender(tb->nToolbarID, *tb);
        // 为每个按钮创建 HWND，以便在可能时使用 WM_PRINT 渲染，提升外观与对齐精度
        CreateToolbarButtonHwnds(tb->nToolbarID);
        // 确保布局/滚动度量反映新创建的工具栏范围
        UpdateDialogRect();
        UpdateScrollbars();
    }
    // 切换到工具栏视图时，仍显示客户区，但在其上方绘制工具栏
    Invalidate(FALSE);
    // 通知宿主：工具栏视图已激活（宿主可据此显示工具栏属性）
    if (m_showingToolbar)
        NotifySelectionChanged(ESurfaceNotify::ToolbarSelectionChanged);
}

void CRcDesignSurfaceWnd::ClearToolbarInfo()
{
    // 清除工具栏资源并恢复对话框视图：清空活动工具栏并重置工具栏选择
    // 先释放工具栏渲染缓存
    ClearToolbarRenders();
    m_activeToolbar.reset();
    m_showingToolbar = false;
    m_selectedToolbarIndex = -1;
    m_selectedToolbarIndices.clear();
    m_toolbarPressedIndex = -1;
    // 恢复预览承载窗口可见性，以便返回对话框视图时能重用已创建的控件。
    // 如果之前隐藏了承载窗口，则再次显示；否则 EnsurePreviewHwnd 会按需创建。
    if (m_previewHostHwnd && ::IsWindow(m_previewHostHwnd))
    {
        ::ShowWindow(m_previewHostHwnd, SW_SHOWNA);
        for (auto& kv : m_preview)
        {
            auto& it = kv.second;
            if (it.hwnd && ::IsWindow(it.hwnd))
                ::ShowWindow(it.hwnd, SW_SHOWNA);
        }
    }
    // 注意：此处不要恢复 m_dialog；调用方应在合适时机通过 SetDialogInfo 设置对话框数据。
    Invalidate(FALSE);
    // toolbar hidden -> update scrollbars since content may shrink
    UpdateDialogRect();
    UpdateScrollbars();
}

// 为工具栏资源 ID 确保渲染缓存（如未存在则创建）
bool CRcDesignSurfaceWnd::EnsureToolbarRender(UINT toolbarId, const SToolbarInfo& tb)
{
    // 如果已缓存则直接返回（假定缓存可用）
    auto it = m_toolbarRenders.find(toolbarId);
    if (it != m_toolbarRenders.end())
        return true;

    ToolbarRender tr;
    tr.id = toolbarId;
    tr.btnW = tb.wWidth > 0 ? tb.wWidth : 16;
    tr.btnH = tb.wHeight > 0 ? tb.wHeight : 16;

    // 优先使用 SToolbarInfo 中提供的位图（来自 PE 解析）
    HBITMAP hBmp = nullptr;
    bool srcOwned = false; // 我们是否拥有 hBmp（若为 true 则需在结束时删除）
    if (tb.hBitmap)
    {
        hBmp = tb.hBitmap; // use bitmap loaded by PEFileEditor
        srcOwned = false; // ownership remains with PEFileEditor
    }
    else
    {
    // 尝试从当前模块加载位图资源
        HMODULE hMod = ::GetModuleHandleW(nullptr);
        if (hMod)
        {
            // 先尝试以 CreateDIBSection 方式加载（更易处理像素）
            hBmp = (HBITMAP)::LoadImageW(hMod, MAKEINTRESOURCEW(toolbarId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            if (!hBmp)
                hBmp = (HBITMAP)::LoadBitmap(hMod, MAKEINTRESOURCE(toolbarId));
        }
        if (!hBmp)
        {
            // 未找到位图资源 —— 创建空槽（占位）
            tr.btnBmps.resize(tb.vecItemIDs.size(), nullptr);
            m_toolbarRenders[toolbarId] = std::move(tr);
            return true;
        }
        srcOwned = true;
    }

    // 将图像条水平切分为单个按钮帧
    BITMAP bm{};
    ::GetObject(hBmp, sizeof(bm), &bm);
    int totalW = bm.bmWidth;
    int totalH = bm.bmHeight;
    int origCount = (int)tb.vecItemIDs.size();
    // 构建原始索引到活动切片索引的映射（排除 ID==0 占位项）
    std::vector<int> origToActive(origCount, -1);
    std::vector<int> activeToOrig;
    activeToOrig.reserve(origCount);
    for (int i = 0; i < origCount; ++i)
    {
        if (tb.vecItemIDs[i] != 0)
        {
            origToActive[i] = (int)activeToOrig.size();
            activeToOrig.push_back(i);
        }
    }
    int activeCount = (int)activeToOrig.size();

    // 当位图宽度不能被活动按钮数整除时，使用固定宽度会产生累积误差（向右侧累积）。
    // 此情况下通过分配余数像素来计算每个按钮的源边界，避免漂移。
    std::vector<int> detectedBounds;
    const bool divisible = (totalW > 0 && activeCount > 0 && (totalW % activeCount) == 0);
    const int templateW = (tb.wWidth > 0 ? (int)tb.wWidth : 0);
    const int templateH = (tb.wHeight > 0 ? (int)tb.wHeight : 0);

    // 如果位图条不可整除，避免简单使用 `totalW/count` 作为槽宽，这会导致误差累积。
    // 常见处理方法：
    // 1) 均匀打包的帧：使用余数分配。
    // 2) 固定宽度帧且存在间隙：totalW < templateW*count，使用模板对齐。
    if (!divisible && totalW > 0 && activeCount > 0)
    {
        // Prefer template-aligned bounds for strips narrower than templateW*activeCount.
        if (templateW > 0 && (int64_t)templateW * activeCount > totalW)
            detectedBounds = MakeTemplateAlignedBounds(totalW, activeCount, templateW);

        // 回退策略：比例映射（同样可防止舍入误差累积）
        if (detectedBounds.empty())
            detectedBounds = MakeMappedBounds(totalW, activeCount);
    }
    // 为切片与目标位图选择统一的帧宽。
    // 一些工具栏位图包含额外边距或模板宽度与实际帧宽不同，需要权衡选择。
    int frameW = (tb.wWidth > 0 ? (int)tb.wWidth : (tr.btnW > 0 ? tr.btnW : 16));
    if (frameW <= 0)
        frameW = 16;
    if (totalW > 0 && activeCount > 0)
    {
        // If the strip is exactly divisible by count, trust that as the true frame width.
        if (totalW % activeCount == 0)
        {
            int equal = totalW / activeCount;
            if (equal > 0)
                frameW = equal;
        }
        else if (tb.wWidth > 0)
        {
            // If template width * count fits, prefer it (typical RT_TOOLBAR strips).
            if ((int64_t)tb.wWidth * activeCount <= totalW)
                frameW = (int)tb.wWidth;
            else
            {
                // Otherwise fall back to nearest integer partition.
                int equal = totalW / activeCount;
                if (equal > 0)
                    frameW = equal;
            }
        }
        else
        {
            int equal = totalW / activeCount;
            if (equal > 0)
                frameW = equal;
        }
    }

    // 目标按钮尺寸：
    // - 优先采用模板尺寸（与真实工具栏布局匹配）。
    // - 这样可以避免当 totalW/count < templateW 时帧被压缩并发生漂移。
    int dstW = (templateW > 0 ? templateW : std::max(1, frameW));
    int dstH = (templateH > 0 ? templateH : std::max(1, (totalH > 0) ? totalH : (tr.btnH > 0 ? tr.btnH : 16)));
    tr.btnW = std::max(1, dstW);
    tr.btnH = std::max(1, dstH);
    // 为每个按钮创建位图缓存（DIB 或兼容位图）
    tr.btnWidths.clear();
    tr.btnWidths.reserve(origCount);
    // Partition the strip into `count` slots.
    // 将条带分成若干槽：
    // 优先使用 detectedBounds（若已检测到），否则对可整除的条带使用固定宽并居中留白
    int slotW = tr.btnW;
    int slotBaseOffset = 0;
    if (detectedBounds.empty())
    {
        if (totalW > 0 && activeCount > 0 && slotW > 0)
        {
            int used = slotW * activeCount;
            if (used <= totalW)
            {
                int extra = totalW - used;
                if (extra > 0)
                    slotBaseOffset = extra / 2;
            }
            else
            {
                // 若计算得到的宽度超出可用宽度，则退回为等分且不留边距
                slotW = std::max(1, totalW / activeCount);
                slotBaseOffset = 0;
            }
        }
    }

    for (int i = 0; i < origCount; ++i)
    {
        int sx = 0;
        int sx2 = 0;
        int aidx = (i >= 0 && i < origCount) ? origToActive[i] : -1;
        if (!detectedBounds.empty())
        {
            // detectedBounds corresponds to active items (size activeCount+1)
            if (aidx >= 0)
            {
                sx = detectedBounds[(size_t)aidx];
                sx2 = detectedBounds[(size_t)aidx + 1];
            }
            else
            {
                // placeholder: no source slice
                sx = 0;
                sx2 = 0;
            }
        }
        else if (totalW > 0 && activeCount > 0)
        {
            if (slotW > 0)
            {
                // slotW partition applies across activeCount; map using aidx
                if (aidx >= 0)
                {
                    sx = slotBaseOffset + aidx * slotW;
                    sx2 = std::min(totalW, sx + slotW);
                }
                else
                {
                    sx = 0; sx2 = 0;
                }
            }
            else
            {
                if (aidx >= 0)
                {
                    sx = (int)((int64_t)totalW * aidx / activeCount);
                    sx2 = (int)((int64_t)totalW * (aidx + 1) / activeCount);
                }
                else
                {
                    sx = 0; sx2 = 0;
                }
            }
        }
        int sw = std::max(0, sx2 - sx);
        tr.btnWidths.push_back(sw);

        // 若源宽度为 0，则视为占位符：不创建位图
        if (sw <= 0)
        {
            tr.btnBmps.push_back(nullptr);
            tr.btnHwnds.push_back(nullptr);
            continue;
        }

        // 创建兼容位图并从源位图拷贝对应部分
        HDC dc = ::GetDC(nullptr);
        HDC mem = ::CreateCompatibleDC(dc);
        HBITMAP hb = ::CreateCompatibleBitmap(dc, tr.btnW, tr.btnH);
        HBITMAP old = (HBITMAP)SelectObject(mem, hb);
        HDC src = ::CreateCompatibleDC(dc);
        HBITMAP oldSrc = (HBITMAP)SelectObject(src, hBmp);
        if (sw == tr.btnW)
        {
            ::BitBlt(mem, 0, 0, tr.btnW, tr.btnH, src, sx, 0, SRCCOPY);
        }
        else
        {
            // 避免拉伸：将源像素居中裁剪后拷贝到目标位图
            int copyW = std::min(tr.btnW, sw);
            int srcX = sx + std::max(0, (sw - copyW) / 2);
            int dstX = std::max(0, (tr.btnW - copyW) / 2);
            int srcH = totalH;
            int copyH = std::min(tr.btnH, srcH);
            int srcY = std::max(0, (srcH - copyH) / 2);
            int dstY = std::max(0, (tr.btnH - copyH) / 2);
            ::BitBlt(mem, dstX, dstY, copyW, copyH, src, srcX, srcY, SRCCOPY);
        }
        SelectObject(src, oldSrc);
        ::DeleteDC(src);
        SelectObject(mem, old);
        ::DeleteDC(mem);
        ::ReleaseDC(nullptr, dc);
        tr.btnBmps.push_back(hb);
        tr.btnHwnds.push_back(nullptr);
    }

    // 仅在我们负责创建源位图时才销毁它（避免删除外部所有权的位图）
    if (srcOwned && hBmp)
        ::DeleteObject(hBmp);
    m_toolbarRenders[toolbarId] = std::move(tr);
    return true;
}

// 为工具栏按钮创建（隐藏）HWND，以便在可能时使用 WM_PRINT 进行渲染。
// 这有助于减少与位图渲染之间的对齐/视觉差异。
void CRcDesignSurfaceWnd::CreateToolbarButtonHwnds(UINT toolbarId)
{
    auto it = m_toolbarRenders.find(toolbarId);
    if (it == m_toolbarRenders.end())
        return;
    ToolbarRender& tr = it->second;
    // ensure preview host exists
    EnsurePreviewHost();
    if (!m_previewHostHwnd || !::IsWindow(m_previewHostHwnd))
        return;
    for (size_t i = 0; i < tr.btnBmps.size(); ++i)
    {
        // 如果是占位符（没有源位图），不要为其创建 HWND
        HBITMAP hb = (i < tr.btnBmps.size()) ? tr.btnBmps[i] : nullptr;
        if (!hb)
        {
            if (i < tr.btnHwnds.size())
                tr.btnHwnds[i] = nullptr;
            else
                tr.btnHwnds.push_back(nullptr);
            continue;
        }
        if (i < tr.btnHwnds.size() && tr.btnHwnds[i] && ::IsWindow(tr.btnHwnds[i]))
            continue;
        // 创建一个按钮子窗口作为 WM_PRINT 的占位（隐藏），用于原生渲染预览
        HWND h = ::CreateWindowExW(0, L"BUTTON", L"",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, tr.btnW, tr.btnH,
            m_previewHostHwnd, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (h && ::IsWindow(h))
            ::ShowWindow(h, SW_HIDE);
        if (i < tr.btnHwnds.size())
            tr.btnHwnds[i] = h;
        else
            tr.btnHwnds.push_back(h);
    }
}

// 在指定画布矩形内绘制工具栏资源（按钮）
void CRcDesignSurfaceWnd::DrawToolbarCanvas(CDC& dc, const CRect& canvasRect, const SToolbarInfo& tb) const
{
    auto it = m_toolbarRenders.find(tb.nToolbarID);
    if (it == m_toolbarRenders.end())
        return;
    const ToolbarRender& tr = it->second;
    int x = canvasRect.left;
    for (size_t i = 0; i < tr.btnBmps.size(); ++i)
    {
        HBITMAP hb = tr.btnBmps[i];
        if (!hb)
        {
            x += tr.btnW + 4;
            continue;
        }
        // 如果存在实际 HWND，优先使用 WM_PRINT 以保留系统绘制的细节；否则使用缓存位图直接 blit
        HWND btnHwnd = (i < tr.btnHwnds.size()) ? tr.btnHwnds[i] : nullptr;
        if (btnHwnd && ::IsWindow(btnHwnd))
        {
            PrintPreviewToDc(dc, btnHwnd, CRect(x, canvasRect.top, x + tr.btnW, canvasRect.top + tr.btnH));
        }
        else
        {
            CDC mem;
            mem.CreateCompatibleDC(&dc);
            CBitmap bmp;
            bmp.Attach(hb);
            CBitmap* old = mem.SelectObject(&bmp);
            dc.BitBlt(x, canvasRect.top, tr.btnW, tr.btnH, &mem, 0, 0, SRCCOPY);
            mem.SelectObject(old);
            bmp.Detach();
        }
        x += tr.btnW + 4;
    }
}

// 获取指定 DPI 下的缓存对话框字体（若不存在则创建并缓存）
HFONT CRcDesignSurfaceWnd::GetCachedDialogFontForDPI(int logpixy)
{
    if (logpixy <= 0)
        return nullptr;
    auto it = m_hDialogFontsPerDpi.find(logpixy);
    if (it != m_hDialogFontsPerDpi.end())
        return it->second;
    if (!m_dialog || !m_dialog->pFontInfo)
        return nullptr;
    HFONT hf = CreateHfontFromSFontInfoWithDPI(m_dialog->pFontInfo.get(), logpixy);
    if (hf)
        m_hDialogFontsPerDpi[logpixy] = hf;
    return hf;
}

// 使用指定 LOGPIXELSY（DPI）从 SFontInfo 创建 HFONT
HFONT CRcDesignSurfaceWnd::CreateHfontFromSFontInfoWithDPI(const SFontInfo* pfi, int logpixy)
{
    if (!pfi)
        return nullptr;
    int pixelHeight = -MulDiv(static_cast<int>(pfi->wPointSize), logpixy, 72);
    LOGFONTW lf{};
    lf.lfHeight = pixelHeight;
    lf.lfWeight = static_cast<LONG>(pfi->wWeight);
    lf.lfItalic = pfi->bItalic ? TRUE : FALSE;
    lf.lfCharSet = pfi->bCharSet;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy_s(lf.lfFaceName, pfi->csFaceName.GetString(), _TRUNCATE);
    return ::CreateFontIndirectW(&lf);
}

/*!
 * \brief 绘制回退控件外观（当不能使用 WM_PRINT 预览时）
 * \param dc 目标设备上下文
 * \param canvas 控件在画布上的矩形（像素）
 * \param cls 控件类名
 * \param text 控件文本
 */
/*
 * \brief 绘制回退控件外观（当不能使用 WM_PRINT 预览时）
 */
void CRcDesignSurfaceWnd::DrawFallbackControl(CDC& dc, const CRect& canvas, const CStringW& cls, const CStringW& text, DWORD style, DWORD exStyle) const
{
    CStringW up = cls;
    up.MakeUpper();

    // 默认文本绘制参数
    int padding = 4;
    CRect r = canvas;

    // Prepare font: create a DC-specific HFONT from dialog font info when possible
    HFONT tempHf = nullptr;
    HFONT hfUse = nullptr;
    if (m_dialog && m_dialog->pFontInfo)
    {
        int logpixy = ::GetDeviceCaps(dc.GetSafeHdc(), LOGPIXELSY);
        HFONT cached = const_cast<CRcDesignSurfaceWnd*>(this)->GetCachedDialogFontForDPI(logpixy);
        hfUse = cached ? cached : (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    }
    else
    {
        hfUse = m_hDialogFont ? m_hDialogFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    }

    if (!hfUse)
        hfUse = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);

    CFont* selOld = nullptr;
    // We'll select hfUse manually in each branch below when drawing text

    if (up == L"EDIT")
    {
        // 白色背景并带下陷边框
        dc.FillSolidRect(&r, RGB(255, 255, 255));
        dc.DrawEdge(&r, EDGE_SUNKEN, BF_RECT);
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
        CFont* font = CFont::FromHandle(hfUse);
        CFont* old = dc.SelectObject(font);
        CRect tr = r;
        tr.DeflateRect(padding, 2, padding, 2);
        ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        dc.SelectObject(old);
        selOld = old;
        return;
    }
    if (up == L"BUTTON" || up == L"BUTTON32")
    {
        // Distinguish groupbox / checkbox which are also BUTTON class
        bool isGroup = ((style & BS_GROUPBOX) != 0) || up == L"GROUPBOX";
        bool isCheck = ((style & BS_CHECKBOX) != 0) || ((style & BS_AUTOCHECKBOX) != 0) || ((style & BS_AUTORADIOBUTTON) != 0) || up == L"CHECKBOX";
        if (isGroup)
        {
            // Group box: draw frame and label at top-left
            dc.FillSolidRect(&r, RGB(250,250,250));
            CPen p(PS_SOLID, 1, RGB(160,160,160));
            CPen* oldp = dc.SelectObject(&p);
            // Draw frame but leave space for label
            CRect fr = r;
            int labW = 0;
            CFont* font = CFont::FromHandle(m_hDialogFont ? m_hDialogFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
            CFont* oldf = dc.SelectObject(font);
            SIZE sz{};
            ::GetTextExtentPoint32W(dc.GetSafeHdc(), text.GetString(), (int)wcslen(text.GetString()), &sz);
            labW = sz.cx + 8;
            dc.SelectObject(oldf);
            // top border left segment
            dc.MoveTo(fr.left + 8 + labW, fr.top);
            dc.LineTo(fr.right - 1, fr.top);
            // remaining borders
            dc.MoveTo(fr.left, fr.top + 1);
            dc.LineTo(fr.left, fr.bottom - 1);
            dc.LineTo(fr.right - 1, fr.bottom - 1);
            dc.LineTo(fr.right - 1, fr.top + 1);
            // draw label background to match control background
            CRect lblRect(fr.left + 8, fr.top - 1, fr.left + 8 + labW, fr.top + 1 + sz.cy);
            dc.FillSolidRect(&lblRect, RGB(250,250,250));
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
            dc.SelectObject(font);
            CRect trlbl = lblRect;
            trlbl.DeflateRect(2, 0, 2, 0);
            ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &trlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            dc.SelectObject(oldf);
            dc.SelectObject(oldp);
            return;
        }
        if (isCheck)
        {
            // Checkbox/radio: draw box + label
            // draw background as transparent
            dc.SetBkMode(TRANSPARENT);
            // box rect
            int boxSize = std::min(16, r.Height() - 4);
            CRect box(r.left + 2, r.top + (r.Height() - boxSize) / 2, r.left + 2 + boxSize, r.top + (r.Height() - boxSize) / 2 + boxSize);
            dc.FillSolidRect(&box, ::GetSysColor(COLOR_WINDOW));
            dc.Draw3dRect(&box, RGB(120,120,120), RGB(200,200,200));
            // checkmark if checked flag present
            bool checked = (style & BST_CHECKED) != 0; // may be 0; best-effort
            if (checked)
            {
                CPen p2(PS_SOLID, 2, RGB(0,0,0));
                CPen* oldp2 = dc.SelectObject(&p2);
                dc.MoveTo(box.left + 3, box.top + box.Height() / 2);
                dc.LineTo(box.left + box.Width() / 3, box.bottom - 4);
                dc.LineTo(box.right - 3, box.top + 3);
                dc.SelectObject(oldp2);
            }
            // label
            dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
            CFont* font = CFont::FromHandle(m_hDialogFont ? m_hDialogFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
            CFont* old = dc.SelectObject(font);
            CRect tr = r;
            tr.left = box.right + 6;
            tr.DeflateRect(2, 2, 2, 2);
            ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            dc.SelectObject(old);
            return;
        }
        // 凸起的按钮外观
        dc.FillSolidRect(&r, ::GetSysColor(COLOR_BTNFACE));
        dc.DrawEdge(&r, EDGE_RAISED, BF_RECT);
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
        CFont* font = CFont::FromHandle(hfUse);
        CFont* old = dc.SelectObject(font);
        ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        dc.SelectObject(old);
        selOld = old;
        return;
    }

    if (up == L"COMBOBOX")
    {
        // 浅色背景并在右侧绘制下拉按钮
        dc.FillSolidRect(&r, RGB(255, 255, 255));
        // 绘制边框
        dc.Draw3dRect(&r, RGB(180,180,180), RGB(180,180,180));
        // 下拉按钮区域
        int btnW = std::max(16, ::GetSystemMetrics(SM_CXVSCROLL));
        CRect btn(r.right - btnW, r.top, r.right, r.bottom);
        dc.FillSolidRect(&btn, ::GetSysColor(COLOR_BTNFACE));
        dc.Draw3dRect(&btn, RGB(140,140,140), RGB(140,140,140));
        // 绘制小箭头
        POINT pts[3];
        int cx = (btn.left + btn.right) / 2;
        int cy = (btn.top + btn.bottom) / 2;
        pts[0] = { cx - 4, cy - 1 };
        pts[1] = { cx + 4, cy - 1 };
        pts[2] = { cx, cy + 3 };
        CPen p(PS_SOLID, 1, RGB(80,80,80));
        CPen* oldp = dc.SelectObject(&p);
        CBrush* oldb = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
        dc.Polygon(pts, 3);
        dc.SelectObject(oldb);
        dc.SelectObject(oldp);

        // 文本绘制
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
        CFont* font = CFont::FromHandle(hfUse);
        CFont* old = dc.SelectObject(font);
        CRect tr = r;
        tr.right = btn.left - 2;
        tr.DeflateRect(padding, 2, 2, 2);
        ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        dc.SelectObject(old);
        selOld = old;
        return;
    }

    // 通用控件回退绘制：浅浮雕矩形并绘制文本
    // 对于 List / Tree / ListView 等控件，绘制明显的 1px 边框以增强边界可见性（解决 6.3）。
    dc.FillSolidRect(&r, RGB(250, 250, 250));
    // 使用包含匹配而非精确比较，以兼容各种 class 名称（例如 SysListView32, LISTBOX 等）
    bool isListOrTree = (up.Find(L"LIST") >= 0) || (up.Find(L"LISTVIEW") >= 0) || (up.Find(L"TREE") >= 0) || (up.Find(L"CListCtrl") >= 0);
    if (isListOrTree)
    {
        // 为列表/树控件绘制清晰的 3d 边框，便于区分边界
        dc.Draw3dRect(&r, RGB(180,180,180), RGB(180,180,180));
        // 在内部绘制一点浅色填充，模拟经典控件边缘
        CRect inner = r;
        inner.DeflateRect(1,1,1,1);
        dc.Draw3dRect(&inner, RGB(230,230,230), RGB(230,230,230));
        // 进一步区分 List 与 Tree 的视觉：
        // - List: 绘制若干水平分隔线以模拟行边界
        // - Tree: 绘制缩进与折叠图标样式的占位符，帮助识别为树控件
        bool isTree = (up.Find(L"TREE") >= 0) || (up.Find(L"TREEVIEW") >= 0);
        bool isList = !isTree;
        if (isList)
        {
            // 绘制示例行（不会影响实际数据），使用淡灰线做分隔
            int rowH = std::max(12, inner.Height() / 6);
            int y = inner.top + rowH;
            CPen penRow(PS_SOLID, 1, RGB(220,220,220));
            CPen* oldPen = dc.SelectObject(&penRow);
            for (int i = 0; i < 5 && y < inner.bottom; ++i, y += rowH)
            {
                dc.MoveTo(inner.left + 2, y);
                dc.LineTo(inner.right - 2, y);
            }
            dc.SelectObject(oldPen);
            // 绘制首行高亮提示（示意选中行）
            CRect sel(inner.left + 2, inner.top + 2, inner.right - 2, inner.top + rowH - 2);
            CBrush bSel(RGB(235, 245, 255));
            CBrush* oldB = dc.SelectObject(&bSel);
            dc.FillRect(&sel, &bSel);
            dc.SelectObject(oldB);
        }
        else
        {
            // Tree: 绘制缩进竖线与占位折叠三角/加号
            CPen penLine(PS_SOLID, 1, RGB(200,200,200));
            CPen* oldPen = dc.SelectObject(&penLine);
            int indentX = inner.left + 12;
            // 画三条竖线表示多级缩进
            int y0 = inner.top + 8;
            for (int row = 0; row < 4; ++row)
            {
                int yy = y0 + row * 16;
                // 竖线
                dc.MoveTo(indentX, yy - 8);
                dc.LineTo(indentX, yy + 8);
                // 折叠图标（简化为方块+线）
                CRect glyph(indentX - 8, yy - 6, indentX - 2, yy + 6);
                dc.Draw3dRect(&glyph, RGB(160,160,160), RGB(200,200,200));
                // 横线代表节点文本
                dc.MoveTo(indentX + 6, yy - 2);
                dc.LineTo(inner.right - 6, yy - 2);
            }
            dc.SelectObject(oldPen);
        }
    }
    else
    {
        // 其他通用控件使用蚀刻边框作为回退样式
        dc.DrawEdge(&r, EDGE_ETCHED, BF_RECT);
    }
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
    CFont* font = CFont::FromHandle(hfUse);
    CFont* old = dc.SelectObject(font);
    CRect tr = r;
    tr.DeflateRect(padding, 2, padding, 2);
    // ensure text does not overflow control bounds: singleline with end ellipsis
    ::DrawTextW(dc.GetSafeHdc(), text.GetString(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    dc.SelectObject(old);

    // no cleanup here; cached HFONT lifecycle managed by UpdateDialogFont/OnDestroy
}

void CRcDesignSurfaceWnd::SelectControlByIndex(int idx, bool ctrlDown, bool shiftDown)
{
    if (!m_dialog)
        return;

    if (idx < 0 || idx >= (int)m_dialog->vecCtrlInfo.size())
        return;

    // 隐藏的禁用控件不能通过搜索或键盘索引重新选中。
    auto ctrl = m_dialog->vecCtrlInfo[idx];
    if (!ctrl || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*ctrl)))
        return;

    m_activeComboId = 0;
    m_activeComboDrawRectCanvas = CRect(0,0,0,0);
    m_guides = Guides{};

    m_dialogSelected = false;
    if (shiftDown)
    {
        auto it = std::find(m_multiSelected.begin(), m_multiSelected.end(), idx);
        if (it == m_multiSelected.end())
        {
            m_multiSelected.push_back(idx);
            m_selectedIndex = idx;
        }
        else
        {
            m_multiSelected.erase(it);
            if (m_multiSelected.empty()) m_selectedIndex = -1; else m_selectedIndex = m_multiSelected.front();
        }
    }
    else if (ctrlDown)
    {
        auto it = std::find(m_multiSelected.begin(), m_multiSelected.end(), idx);
        if (it == m_multiSelected.end())
        {
            m_multiSelected.push_back(idx);
            m_selectedIndex = idx;
        }
        else
        {
            m_multiSelected.erase(it);
            if (m_multiSelected.empty()) m_selectedIndex = -1; else m_selectedIndex = m_multiSelected.front();
        }
    }
    else
    {
        m_multiSelected.clear();
        m_selectedIndex = idx;
        m_multiSelected.push_back(idx);
    }
    NotifySelectionChanged(0);
    Invalidate(FALSE);
}

// Select a toolbar item by index, supporting Ctrl toggle and Shift additive similar to controls.
void CRcDesignSurfaceWnd::SelectToolbarByIndex(int idx, bool ctrlDown, bool shiftDown)
{
    if (!m_activeToolbar)
        return;

    // reset transient state similar to control selection
    m_activeComboId = 0;
    m_activeComboDrawRectCanvas = CRect(0,0,0,0);
    m_guides = Guides{};

    if (shiftDown)
    {
        auto it = std::find(m_selectedToolbarIndices.begin(), m_selectedToolbarIndices.end(), idx);
        if (it == m_selectedToolbarIndices.end())
        {
            m_selectedToolbarIndices.push_back(idx);
            m_selectedToolbarIndex = idx;
        }
        else
        {
            m_selectedToolbarIndices.erase(it);
            if (m_selectedToolbarIndices.empty()) m_selectedToolbarIndex = -1; else m_selectedToolbarIndex = m_selectedToolbarIndices.front();
        }
    }
    else if (ctrlDown)
    {
        auto it = std::find(m_selectedToolbarIndices.begin(), m_selectedToolbarIndices.end(), idx);
        if (it == m_selectedToolbarIndices.end())
        {
            m_selectedToolbarIndices.push_back(idx);
            m_selectedToolbarIndex = idx;
        }
        else
        {
            m_selectedToolbarIndices.erase(it);
            if (m_selectedToolbarIndices.empty()) m_selectedToolbarIndex = -1; else m_selectedToolbarIndex = m_selectedToolbarIndices.front();
        }
    }
    else
    {
        m_selectedToolbarIndices.clear();
        m_selectedToolbarIndices.push_back(idx);
        m_selectedToolbarIndex = idx;
    }

    // notify host/property grid
    NotifySelectionChanged(ESurfaceNotify::ToolbarSelectionChanged);
    Invalidate(FALSE);
}

void CRcDesignSurfaceWnd::SelectAllControls()
{
    if (!m_dialog)
        return;
    m_multiSelected.clear();
    for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
    {
        if (m_dialog->vecCtrlInfo[i] &&
            (!m_levelManagerConfigMode || !IsLevelAuditControlHidden(*m_dialog->vecCtrlInfo[i])))
            m_multiSelected.push_back(i);
    }
    m_selectedIndex = (m_multiSelected.empty() ? -1 : m_multiSelected.front());
    m_dialogSelected = false;
    NotifySelectionChanged(0);
    Invalidate(FALSE);
}

std::vector<int> CRcDesignSurfaceWnd::GetMultiSelectedIndices() const
{
    return m_multiSelected;
}

void CRcDesignSurfaceWnd::SetSelectedToolbarIndices(const std::vector<int>& indices)
{
    m_selectedToolbarIndices = indices;
    m_selectedToolbarIndex = m_selectedToolbarIndices.empty() ? -1 : m_selectedToolbarIndices.front();
    // Notify host/property grid of selection change
    NotifySelectionChanged(ESurfaceNotify::ToolbarSelectionChanged);
    Invalidate(FALSE);
}

	BEGIN_MESSAGE_MAP(CRcDesignSurfaceWnd, CWnd)
		ON_WM_CREATE()
		ON_WM_PAINT()
		ON_WM_ERASEBKGND()
		ON_WM_DESTROY()
		ON_WM_SIZE()
		ON_WM_HSCROLL()
		ON_WM_VSCROLL()
		ON_WM_LBUTTONDOWN()
		ON_WM_LBUTTONUP()
		ON_WM_MOUSEMOVE()
		ON_WM_SETCURSOR()
	END_MESSAGE_MAP()

/*!
 * \brief 析构函数（注意：不要在析构中手动 DestroyWindow）
 *
 * 父窗口在销毁时会自动销毁子窗口，避免在析构中重复销毁。
 */
CRcDesignSurfaceWnd::~CRcDesignSurfaceWnd()
{
    // During parent teardown, MFC destroys child HWNDs.
    // Avoid calling DestroyWindow from the destructor to prevent double-destroy.
    // Free any toolbar render bitmaps
    ClearToolbarRenders();
}

/*!
 * \brief 确保存在一个隐藏的承载窗口用于创建控件预览
 *
 * 该承载窗口位于远离可见区域的位置，用于创建原生子控件并通过 WM_PRINT 绘制到画布。
 */
void CRcDesignSurfaceWnd::EnsurePreviewHost()
{
		if (m_previewHostHwnd && ::IsWindow(m_previewHostHwnd))
			return;
		InitCommonControlsOnce();
		m_previewHostHwnd = ::CreateWindowExW(
			0,
			L"STATIC",
			L"",
			WS_CHILD | WS_VISIBLE,
			-20000, -20000, 10, 10,
			GetSafeHwnd(),
			nullptr,
			::GetModuleHandleW(nullptr),
			nullptr);
	}

/*!
 * \brief 销毁单个预览项并重置其状态
 * \param it 需销毁的 PreviewItem 引用
 */
void CRcDesignSurfaceWnd::DestroyPreviewItem(PreviewItem& it)
{
		if (it.hwnd && ::IsWindow(it.hwnd))
			::DestroyWindow(it.hwnd);
		if (it.hFont)
		{
			::DeleteObject(it.hFont);
			it.hFont = nullptr;
		}
		it = PreviewItem{};
	}

/*!
 * \brief 销毁预览承载窗口及其所有子预览控件
 */
void CRcDesignSurfaceWnd::DestroyPreviewHost()
{
        for (auto& kv : m_preview)
            DestroyPreviewItem(kv.second);
		m_preview.clear();
        // destroy any toolbar button HWNDs cached in renders
        for (auto& kv : m_toolbarRenders)
        {
            auto& tr = kv.second;
            for (auto hw : tr.btnHwnds)
            {
                if (hw && ::IsWindow(hw))
                    ::DestroyWindow(hw);
            }
            tr.btnHwnds.clear();
        }
		if (m_previewHostHwnd && ::IsWindow(m_previewHostHwnd))
		{
			::DestroyWindow(m_previewHostHwnd);
			m_previewHostHwnd = nullptr;
		}
	}

/*!
 * \brief 创建或更新预览控件的 HWND
 * \param it 预览项引用（可能已有 hwnd）
 * \param exStyle 扩展样式
 * \param cls 控件类名
 * \param text 控件文本
 * \param style 常规样式
 * \param size 预览所需的大小（像素）
 * \return 若确保成功返回 true，否则返回 false
 */
bool CRcDesignSurfaceWnd::EnsurePreviewHwnd(PreviewItem& it, DWORD exStyle, const CStringW& cls, const CStringW& text, DWORD style, const CSize& size)
{
		EnsurePreviewHost();
		if (!m_previewHostHwnd || !::IsWindow(m_previewHostHwnd))
			return false;

		DWORD wantStyle = style;
		wantStyle &= ~(WS_POPUP | WS_CAPTION);
		wantStyle |= WS_CHILD | WS_VISIBLE;
		CStringW wantCls = cls.IsEmpty() ? L"STATIC" : cls;

        // 某些资源可能带有 WS_VSCROLL/WS_HSCROLL，这会使预览看起来像带滚动的控件。
        // 资源编辑器通常希望下拉框只显示一个下拉箭头的外观。
		if (_wcsicmp(wantCls.GetString(), L"COMBOBOX") == 0)
		{
			wantStyle &= ~(WS_VSCROLL | WS_HSCROLL);
		}
		else if (_wcsicmp(wantCls.GetString(), L"EDIT") == 0)
		{
			// Ensure edit is visible against dialog background.
			wantStyle |= WS_BORDER;
			if ((wantStyle & (ES_AUTOHSCROLL | ES_MULTILINE | ES_AUTOVSCROLL)) == 0)
				wantStyle |= ES_AUTOHSCROLL;
			exStyle &= ~WS_EX_TRANSPARENT;
		}

		if (it.hwnd && ::IsWindow(it.hwnd))
		{
			if (_wcsicmp(it.cls.GetString(), wantCls.GetString()) != 0)
				DestroyPreviewItem(it);
		}

		if (!it.hwnd)
		{
			it.hwnd = ::CreateWindowExW(
				exStyle,
				wantCls.GetString(),
				text.GetString(),
				wantStyle,
				0, 0, std::max<int>(1, (int)size.cx), std::max<int>(1, (int)size.cy),
				m_previewHostHwnd,
				(HMENU)nullptr,
				::GetModuleHandleW(nullptr),
				nullptr);
			if (!it.hwnd || !::IsWindow(it.hwnd))
			{
				it.hwnd = nullptr;
				return false;
			}
            // Create per-preview HFONT matching dialog font but using preview HWND DPI
            HFONT useHf = nullptr;
            if (m_dialog && m_dialog->pFontInfo)
            {
#ifdef __cpp_if_constexpr
                // nothing
#endif
                // try GetDpiForWindow (Windows 10+). If not available, fall back to screen DPI.
                int logpixy = 0;
                HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
                typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
                GetDpiForWindow_t pGetDpiForWindow = nullptr;
                if (user32)
                    pGetDpiForWindow = (GetDpiForWindow_t)::GetProcAddress(user32, "GetDpiForWindow");
                if (pGetDpiForWindow)
                {
                    UINT dpi = pGetDpiForWindow(it.hwnd);
                    logpixy = static_cast<int>(dpi);
                }
                else
                {
                    HDC hdc = ::GetDC(it.hwnd);
                    logpixy = ::GetDeviceCaps(hdc, LOGPIXELSY);
                    ::ReleaseDC(it.hwnd, hdc);
                }
                it.hFont = CreateHfontFromSFontInfoWithDPI(m_dialog->pFontInfo.get(), logpixy);
                useHf = it.hFont ? it.hFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
            }
            else
            {
                useHf = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
            }
            ::SendMessageW(it.hwnd, WM_SETFONT, (WPARAM)useHf, FALSE);
			it.inited = false;
		}

		it.cls = wantCls;
		it.text = text;
		it.style = wantStyle;
		it.exStyle = exStyle;
		it.size = size;

		::SetWindowTextW(it.hwnd, text.GetString());
		::SetWindowLongPtrW(it.hwnd, GWL_STYLE, (LONG_PTR)wantStyle);
		::SetWindowLongPtrW(it.hwnd, GWL_EXSTYLE, (LONG_PTR)exStyle);
		::SetWindowPos(it.hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		::MoveWindow(it.hwnd, 0, 0, std::max<int>(1, (int)size.cx), std::max<int>(1, (int)size.cy), TRUE);

		InitPreviewContentIfNeeded(it, wantCls);
		return true;
	}

/*!
 * \brief 如果需要，初始化预览控件的内部内容（例如 Tab/Tree 的示例项）
 * \param it 预览项引用
 * \param cls 控件类名
 */
void CRcDesignSurfaceWnd::InitPreviewContentIfNeeded(PreviewItem& it, const CStringW& cls)
{
		if (it.inited || !it.hwnd || !::IsWindow(it.hwnd))
			return;

		// Minimal init for special controls (mirrors existing RcDesigner preview behavior)
		if (_wcsicmp(cls.GetString(), L"SysTabControl32") == 0)
		{
			TCITEMW tci{};
			tci.mask = TCIF_TEXT;
			wchar_t t1[] = L"Tab1";
			tci.pszText = t1;
			TabCtrl_InsertItem(it.hwnd, 0, &tci);
			wchar_t t2[] = L"Tab2";
			tci.pszText = t2;
			TabCtrl_InsertItem(it.hwnd, 1, &tci);
			TabCtrl_SetCurSel(it.hwnd, 0);
		}
		else if (_wcsicmp(cls.GetString(), L"SysTreeView32") == 0)
		{
			TVINSERTSTRUCTW ins{};
			ins.hParent = TVI_ROOT;
			ins.hInsertAfter = TVI_LAST;
			ins.item.mask = TVIF_TEXT;
			wchar_t root[] = L"Root";
			ins.item.pszText = root;
			HTREEITEM hRoot = TreeView_InsertItem(it.hwnd, &ins);
			if (hRoot)
			{
				ins.hParent = hRoot;
				wchar_t child[] = L"Child";
				ins.item.pszText = child;
				TreeView_InsertItem(it.hwnd, &ins);
				TreeView_Expand(it.hwnd, hRoot, TVE_EXPAND);
			}
		}

		it.inited = true;
	}

/*!
 * \brief 使用控件的 WM_PRINT 将其绘制到指定 DC 的给定矩形区域
 * \param dc 目标设备上下文
 * \param hwnd 要绘制的窗口句柄
 * \param targetCanvasRect 在画布坐标下的目标矩形
 * \return 绘制成功返回 true，否则返回 false
 */
bool CRcDesignSurfaceWnd::PrintPreviewToDc(CDC& dc, HWND hwnd, const CRect& targetCanvasRect) const
{
		if (!hwnd || !::IsWindow(hwnd))
			return false;

    // 传入的 targetCanvasRect 为世界/画布坐标。调用方应设置 DC 的视口
    // 以使世界坐标映射到设备坐标（参见 OnPaint）。这里我们根据目标矩形的
    // 左上角偏移计算一个局部视口，相对于当前 DC 原点进行定位，保证 WM_PRINT
    // 在正确的位置绘制。
		int saved = dc.SaveDC();
        // 将 WM_PRINT 的绘制目标定位到相对于当前相机（m_scroll）的期望世界位置。
        // 通过减去相机偏移将世界坐标转换为设备坐标。
        // 注意：此方法为 const，m_scroll 不在此处修改，仅读取以计算偏移。
        CPoint vp(-m_scroll.x + targetCanvasRect.left, -m_scroll.y + targetCanvasRect.top);
        ::SetViewportOrgEx(dc.GetSafeHdc(), vp.x, vp.y, nullptr);
		::IntersectClipRect(dc.GetSafeHdc(), 0, 0,
			std::max(1, targetCanvasRect.Width()), std::max(1, targetCanvasRect.Height()));
		::SendMessageW(hwnd, WM_PRINT, (WPARAM)dc.GetSafeHdc(), PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
		dc.RestoreDC(saved);
		return true;
	}

/*!
 * \brief 创建设计画布窗口实例
 * \param parent 父窗口
 * \param id 窗口 ID
 * \param rect 初始矩形
 * \return 创建成功返回 TRUE
 */
BOOL CRcDesignSurfaceWnd::Create(CWnd* parent, UINT id, const RECT& rect)
{
		CString clsName = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), nullptr);
		return CWnd::CreateEx(0, clsName, _T("DesignSurfaceWnd2"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, parent, id);
	}

/*!
 * \brief WM_CREATE 消息处理，初始化基础度量并更新滚动条
 */
int CRcDesignSurfaceWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
		if (CWnd::OnCreate(lpCreateStruct) == -1)
			return -1;
		m_baseUnits = GetSystemDialogBaseUnits();
		// Ensure font cache empty
		m_hDialogFont = nullptr;
		UpdateDialogRect();
		UpdateScrollbars();
		return 0;
	}

// Create HFONT from SFontInfo
HFONT CRcDesignSurfaceWnd::CreateHfontFromSFontInfo(const SFontInfo* pfi)
{
    if (!pfi)
        return nullptr;
    // Convert point size to pixel height using screen DC
    HDC hdc = ::GetDC(nullptr);
    int logpixy = GetDeviceCaps(hdc, LOGPIXELSY);
    ::ReleaseDC(nullptr, hdc);
    int pixelHeight = -MulDiv(static_cast<int>(pfi->wPointSize), logpixy, 72);

    LOGFONTW lf{};
    lf.lfHeight = pixelHeight;
    lf.lfWeight = static_cast<LONG>(pfi->wWeight);
    lf.lfItalic = pfi->bItalic ? TRUE : FALSE;
    lf.lfCharSet = pfi->bCharSet;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    // copy face name
    wcsncpy_s(lf.lfFaceName, pfi->csFaceName.GetString(), _TRUNCATE);

    return ::CreateFontIndirectW(&lf);
}

// Compute base units based on HFONT metrics
CSize CRcDesignSurfaceWnd::ComputeBaseUnitsFromFont(HFONT hf)
{
    if (!hf)
        return GetSystemDialogBaseUnits();
    HDC hdc = ::GetDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hf);
    TEXTMETRICW tm{};
    ::GetTextMetricsW(hdc, &tm);
    // Prefer measuring average character width from a representative sample
    // which often better matches the dialog base unit used by resource tools.
    const wchar_t* sample = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    SIZE sz = {};
    if (!::GetTextExtentPoint32W(hdc, sample, static_cast<int>(wcslen(sample)), &sz))
    {
        // fallback to tm metrics
        ::SelectObject(hdc, old);
        ::ReleaseDC(nullptr, hdc);
        int baseX = (static_cast<int>(tm.tmAveCharWidth) + static_cast<int>(tm.tmMaxCharWidth)) / 2;
        int baseY = static_cast<int>(tm.tmHeight) + static_cast<int>(tm.tmExternalLeading);
        if (baseX <= 0) baseX = 1;
        if (baseY <= 0) baseY = 1;
        return CSize(baseX, baseY);
    }
    ::SelectObject(hdc, old);
    ::ReleaseDC(nullptr, hdc);

    int avgChar = sz.cx / static_cast<int>(wcslen(sample));
    int baseX = std::max(1, avgChar);
    int baseY = static_cast<int>(tm.tmHeight) + static_cast<int>(tm.tmExternalLeading);
    if (baseY <= 0) baseY = 1;
    return CSize(baseX, baseY);
}

void CRcDesignSurfaceWnd::UpdateDialogFont()
{
    // destroy existing
    if (m_hDialogFont)
    {
        ::DeleteObject(m_hDialogFont);
        m_hDialogFont = nullptr;
    }
    // clear per-dpi cache
    for (auto& kv : m_hDialogFontsPerDpi)
    {
        if (kv.second)
            ::DeleteObject(kv.second);
    }
    m_hDialogFontsPerDpi.clear();
    if (m_dialog && m_dialog->pFontInfo)
    {
        m_hDialogFont = CreateHfontFromSFontInfo(m_dialog->pFontInfo.get());
        if (m_hDialogFont)
        {
            m_baseUnits = ComputeBaseUnitsFromFont(m_hDialogFont);
            return;
        }
    }
    // fallback to system base units
    m_baseUnits = GetSystemDialogBaseUnits();
}

/*!
 * \brief WM_DESTROY 消息处理，销毁预览主机并转发默认处理
 */
void CRcDesignSurfaceWnd::OnDestroy()
{
		DestroyPreviewHost();
		if (m_hDialogFont)
		{
			::DeleteObject(m_hDialogFont);
			m_hDialogFont = nullptr;
		}
		for (auto& kv : m_hDialogFontsPerDpi)
		{
			if (kv.second)
				::DeleteObject(kv.second);
		}
		m_hDialogFontsPerDpi.clear();
		CWnd::OnDestroy();
	}

/*!
 * \brief WM_ERASEBKGND 消息处理（我们使用双缓冲绘制，因此在此处返回 TRUE 以跳过默认擦除）
 */
BOOL CRcDesignSurfaceWnd::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

/*!
 * \brief 确保指定索引的控件在视口内可见（必要时滚动）
 * \param idx 控件在 vecCtrlInfo 中的索引
 */
void CRcDesignSurfaceWnd::EnsureControlVisibleByIndex(int idx)
{
    if (!m_dialog)
        return;
    if (idx < 0 || idx >= (int)m_dialog->vecCtrlInfo.size())
        return;
    auto c = m_dialog->vecCtrlInfo[idx];
    if (!c || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*c))) return;
    // Ensure dialog layout/scroll metrics are up-to-date before computing target
    UpdateDialogRect();
    UpdateScrollbars();

    CRect dlgPx = DluToPxRect(c->ctrlRect, m_baseUnits);
    CRect canvas = DialogRectToCanvas(dlgPx);
    CRect client;
    GetClientRect(&client);

    // Compute minimal scroll change so the control is fully visible.
    // Prefer to move as little as possible instead of centering which can
    // sometimes make scrollbars appear/disappear due to transient metrics.
    int newX = m_scroll.x;
    int newY = m_scroll.y;
    const int margin = 16; // leave some space around control

    // Horizontal: ensure canvas.left..canvas.right inside [m_scroll.x, m_scroll.x + client.Width()]
    if (canvas.left < m_scroll.x + margin)
        newX = std::max<int>(0, canvas.left - margin);
    else if (canvas.right > m_scroll.x + client.Width() - margin)
        newX = canvas.right - client.Width() + margin;

    // Vertical
    if (canvas.top < m_scroll.y + margin)
        newY = std::max<int>(0, canvas.top - margin);
    else if (canvas.bottom > m_scroll.y + client.Height() - margin)
        newY = canvas.bottom - client.Height() + margin;

    // Clamp to content bounds
    int maxX = m_contentSize.cx - client.Width();
    if (maxX < 0) maxX = 0;
    int maxY = m_contentSize.cy - client.Height();
    if (maxY < 0) maxY = 0;
    newX = std::max(0, std::min(maxX, newX));
    newY = std::max(0, std::min(maxY, newY));

    if (newX != m_scroll.x || newY != m_scroll.y)
    {
        m_scroll.x = newX;
        m_scroll.y = newY;
        // Ensure scrollbar controls reflect the new content/position
        UpdateScrollbars();
        Invalidate(FALSE);
    }
}

/*!
 * \brief WM_SIZE 消息处理，更新滚动条和重绘
 */
void CRcDesignSurfaceWnd::OnSize(UINT nType, int cx, int cy)
{
		CWnd::OnSize(nType, cx, cy);
		UpdateScrollbars();
		Invalidate(FALSE);
	}

/*!
 * \brief 设置当前等级（1-8），用于绘制等级徽章
 * \param level 要设置的等级
 */
void CRcDesignSurfaceWnd::SetCurrentLevel(int level)
{
		m_currentLevel = std::max(1, std::min(8, level));
		Invalidate(FALSE);
	}

/*!
 * \brief 启用或禁用等级管理配置模式。在该模式下，禁止移动/调整对话框或控件大小等交互。
 * \param enable 是否启用配置模式
 */
void CRcDesignSurfaceWnd::SetLevelManagerConfigMode(bool enable)
{
		if (m_levelManagerConfigMode == enable)
			return;
		m_levelManagerConfigMode = enable;
		// If marquee selection was active, commit selection
		if (m_marqueeSelecting)
		{
			m_marqueeSelecting = false;
			// compute final selection from m_previewItemRect
			CRect selRectDlg = NormalizeRect(m_previewItemRect);
			// if user just clicked without dragging, restore previous selection
			if (selRectDlg.Width() < 4 && selRectDlg.Height() < 4)
			{
				m_selectedIndex = m_restoreSelectedIndex;
				m_multiSelected = m_restoreMultiSelected;
			}
			else
			{
				// select all controls whose pixel rect intersects selRectDlg
				m_multiSelected.clear();
				if (m_dialog)
				{
					for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
					{
						auto c = m_dialog->vecCtrlInfo[i];
						if (!c) continue;
						CRect rpx = DluToPxRect(c->ctrlRect, m_baseUnits);
						// intersection test (rects in same coordinate space: dialog-client pixels)
						if (!(selRectDlg.right <= rpx.left || selRectDlg.left >= rpx.right || selRectDlg.bottom <= rpx.top || selRectDlg.top >= rpx.bottom))
						{
							m_multiSelected.push_back(i);
						}
					}
					if (!m_multiSelected.empty())
						m_selectedIndex = m_multiSelected.front();
				}
			}
			NotifySelectionChanged(0);
			Invalidate(FALSE);
		}
		m_dragging = false;
		m_previewActive = false;
		m_dragOnDialog = false;
		m_dragOnComboDrop = false;
		m_dragMode = DragMode::None;
		m_activeComboId = 0;
		m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
		m_guides = Guides{};
		Invalidate(FALSE);
	}

/*!
 * \brief 设置对话框标题并触发重绘与通知
 * \param caption 新的标题文本
 */
void CRcDesignSurfaceWnd::SetDialogCaption(const CStringW& caption)
{
		if (!m_dialog)
			return;
		m_dialog->csCaption = caption;
		Invalidate(FALSE);
		NotifySelectionChanged(1);
	}

/*!
 * \brief 在指定对话框中根据控件 ID 查找对应的 SCtrlInfo
 * \param dlg 对话框信息指针
 * \param ctrlId 控件 ID
 * \return 找到返回对应的 shared_ptr，否则返回 nullptr
 */
static std::shared_ptr<rceditor::SCtrlInfo> FindCtrlById(const std::shared_ptr<rceditor::SDialogInfo>& dlg, UINT ctrlId)
{
		if (!dlg)
			return nullptr;
		for (auto& c : dlg->vecCtrlInfo)
		{
			if (c && c->nIDC == ctrlId)
				return c;
		}
		return nullptr;
	}

/*!
 * \brief 设置指定控件的文本并触发重绘与通知
 * \param ctrlId 控件 ID
 * \param text 新的文本
 */
void CRcDesignSurfaceWnd::SetCtrlText(UINT ctrlId, const CStringW& text)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->csText = text;
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 设置指定控件的类名（Class）并触发重绘
 * \param ctrlId 控件 ID
 * \param cls 新的类名
 */
void CRcDesignSurfaceWnd::SetCtrlClass(UINT ctrlId, const CStringW& cls)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->csClassName = cls;
		// preview cache may require different class window next time
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 以 DLU 单位设置控件矩形并更新布局/滚动
 * \param ctrlId 控件 ID
 * \param dluRect 控件矩形（DLU 单位）
 */
void CRcDesignSurfaceWnd::SetCtrlRectDlu(UINT ctrlId, const CRect& dluRect)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->ctrlRect = dluRect;
		UpdateDialogRect();
		UpdateScrollbars();
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 设置指定控件的样式（style）
 * \param ctrlId 控件 ID
 * \param style 新样式
 */
void CRcDesignSurfaceWnd::SetCtrlStyle(UINT ctrlId, DWORD style)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->dwStyle = style;
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 设置指定控件的扩展样式（exStyle）
 * \param ctrlId 控件 ID
 * \param exStyle 新的扩展样式
 */
void CRcDesignSurfaceWnd::SetCtrlExStyle(UINT ctrlId, DWORD exStyle)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->dwExStyle = exStyle;
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 为指定控件切换多行显示模式
 * \param ctrlId 控件 ID
 * \param enable 是否启用多行
 * \note 仅修改 blMultiLineEditCtrl 属性，不修改控件的 dwStyle
 *       因为 ES_MULTILINE (0x0004) 和 BS_MULTILINE (0x2000) 是不同的值，
 *       直接修改样式可能会破坏其他样式位（如按钮类型）
 */
void CRcDesignSurfaceWnd::SetCtrlMultiLine(UINT ctrlId, bool enable)
{
		auto c = FindCtrlById(m_dialog, ctrlId);
		if (!c)
			return;
		c->blMultiLineEditCtrl = enable;
		// 仅设置非资源属性，不修改 dwStyle 以避免破坏其他样式位
		Invalidate(FALSE);
		NotifySelectionChanged(2);
	}

/*!
 * \brief 判断是否应绘制等级徽章（取决于对话框是否在等级管理器中）
 * \return 若应绘制返回 true
 */
bool CRcDesignSurfaceWnd::ShouldDrawLevelBadges() const
{
    return m_dialog && m_dialog->m_blIsInLevelManager;
}

// 为动态窗口提供默认控件大小（DLU），并标记是否为“高个”控件。
// 取 VS RC 编辑器常见的默认值，适当放大便于在设计面板中查看。
CSize CRcDesignSurfaceWnd::GetDefaultDynamicCtrlSizeDlu(const CStringW& cls, bool& outTall) const
{
    outTall = false;
    CStringW up = cls;
    up.MakeUpper();

    if (up.Find(L"LISTVIEW") >= 0 || up.Find(L"TREE") >= 0 || up.Find(L"LIST") >= 0)
    {
        outTall = true;
        return CSize(90, 60);
    }
    if (up.Find(L"GROUPBOX") >= 0)
    {
        outTall = true;
        return CSize(100, 50);
    }
    if (up.Find(L"COMBOBOX") >= 0)
    {
        // 下拉高度稍大，便于看见
        return CSize(70, 70);
    }
    if (up.Find(L"TAB") >= 0)
    {
        outTall = true;
        return CSize(100, 30);
    }
    if (up.Find(L"PROGRESS") >= 0)
        return CSize(100, 12);
    if (up.Find(L"TRACKBAR") >= 0 || up.Find(L"SLIDER") >= 0)
        return CSize(90, 16);
    if (up.Find(L"SCROLLBAR") >= 0)
    {
        outTall = true;
        return CSize(14, 70);
    }
    if (up.Find(L"EDIT") >= 0)
        return CSize(80, 14);
    if (up.Find(L"STATIC") >= 0)
        return CSize(60, 10);
    if (up.Find(L"CHECK") >= 0 || up.Find(L"RADIO") >= 0)
        return CSize(70, 12);
    if (up.Find(L"BUTTON") >= 0)
        return CSize(50, 14);

    // 默认控件大小
    return CSize(60, 14);
}

// 针对 mapDynamicDialogInfo 中缺少位置信息的动态窗口，生成默认控件布局。
// 条件：
// 1) m_blIsDynamic == true，或
// 2) 所有控件矩形宽/高无效（<=0）——防止只有声明未绘制。
// 静态窗口保持原有布局，不受影响。
void CRcDesignSurfaceWnd::PrepareDynamicDialogLayout()
{

    
    if (!m_dialog)
        return;
    if (!m_dialog->m_blIsDynamic)
        return;
    if (m_dialog->vecCtrlInfo.empty())
        return;

    bool hasValidRect = false;
    for (auto& c : m_dialog->vecCtrlInfo)
    {
        if (!c)
            continue;
        if (c->ctrlRect.Width() > 0 && c->ctrlRect.Height() > 0)
        {
            hasValidRect = true;
            break;
        }
    }
    // 若不是动态窗口且已有有效矩形，则不干预（保留静态窗口风格）
    if (!m_dialog->m_blIsDynamic && hasValidRect)
        return;

    const int maxPerRow = 10;
    const int startXDlu = 8;
    const int startYDlu = 8;
    const int gapXDlu = 6;
    const int gapYDlu = 8;

    std::vector<std::shared_ptr<SCtrlInfo>> normal;
    std::vector<std::shared_ptr<SCtrlInfo>> tall;
    normal.reserve(m_dialog->vecCtrlInfo.size());
    tall.reserve(m_dialog->vecCtrlInfo.size());

    for (auto& c : m_dialog->vecCtrlInfo)
    {
        if (!c)
            continue;
        bool isTall = false;
        CSize sz = GetDefaultDynamicCtrlSizeDlu(c->csClassName, isTall);
        if (sz.cx <= 0) sz.cx = 60;
        if (sz.cy <= 0) sz.cy = 14;
        // 先写入尺寸，稍后排布位置
        c->ctrlRect = CRect(0, 0, sz.cx, sz.cy);
        (isTall ? tall : normal).push_back(c);
    }

    int curX = startXDlu;
    int curY = startYDlu;
    int rowH = 0;
    int countInRow = 0;
    int maxRight = startXDlu;

    auto placeList = [&](const std::vector<std::shared_ptr<SCtrlInfo>>& list)
    {
        for (auto& c : list)
        {
            if (!c)
                continue;
            int w = std::max(1, c->ctrlRect.Width());
            int h = std::max(1, c->ctrlRect.Height());

            if (countInRow >= maxPerRow)
            {
                curX = startXDlu;
                curY += rowH + gapYDlu;
                rowH = 0;
                countInRow = 0;
            }

            c->ctrlRect = CRect(curX, curY, curX + w, curY + h);
            maxRight = std::max(maxRight, curX + w);
            curX += w + gapXDlu;
            rowH = std::max(rowH, h);
            ++countInRow;
        }
    };

    placeList(normal);

    // 高个控件起新的一行，避免压缩前排。
    if (!tall.empty())
    {
        if (countInRow > 0)
        {
            curX = startXDlu;
            curY += rowH + gapYDlu;
            rowH = 0;
            countInRow = 0;
        }
        placeList(tall);
    }

    // 根据排布更新窗口大小，确保能容纳所有控件
    int totalWidthDlu = std::max(maxRight + startXDlu, 60);
    int totalHeightDlu = std::max(curY + rowH + startYDlu, 40);

    int baseLeft = m_dialog->rectDlg.left;
    int baseTop = m_dialog->rectDlg.top;
    m_dialog->rectDlg.left = baseLeft;
    m_dialog->rectDlg.top = baseTop;
    m_dialog->rectDlg.right = baseLeft + totalWidthDlu;
    m_dialog->rectDlg.bottom = baseTop + totalHeightDlu;
}

/*!
 * \brief 在指定控件区域绘制等级徽章覆盖（绿色勾或红叉）
 * \param dc 目标设备上下文
 * \param targetCanvasRect 目标区域（画布坐标）
 * \param level 等级（目前未使用，仅保留接口一致性）
 * \param enabled 徽章是否为启用状态（决定勾或叉）
 */
void CRcDesignSurfaceWnd::DrawLevelBadgeOverlay(CDC& dc, const CRect& targetCanvasRect, int level, bool enabled) const
{
		UNREFERENCED_PARAMETER(level);
		// Legacy draws badge at top-right. Allow drawing for small controls
		// (eg. checkbox) by adapting badge size to available control area.
		CRect r = targetCanvasRect;
		if (r.Width() <= 0 || r.Height() <= 0)
			return;
		// Preferred badge size; will be reduced for small controls but not below min
		const int prefSize = 12;
		const int minSize = 6;
		int avail = std::min(r.Width(), r.Height()) - 4; // leave margin inside control
		int size = prefSize;
		if (avail < prefSize)
		{
			if (avail >= minSize)
				size = avail;
			else
			{
				// too small to draw inside control; skip drawing
				return;
			}
		}

		// draw badge at top-right inside control with small margin
		CRect b = r;
		b.left = b.right - (size + 4);
		b.top = b.top + 2;
		b.right = b.left + size;
		b.bottom = b.top + size;

		const COLORREF bg = ::GetSysColor(COLOR_3DFACE);
		const COLORREF border = RGB(120, 120, 120);
		const COLORREF fg = enabled ? RGB(0, 150, 0) : RGB(200, 0, 0);

		CPen penBorder(PS_SOLID, 1, border);
		CPen penFg(PS_SOLID, 2, fg);
		CBrush br(bg);
		CPen* op = dc.SelectObject(&penBorder);
		CBrush* ob = dc.SelectObject(&br);
		dc.Ellipse(&b);
		dc.SelectObject(ob);

		CPen* of = dc.SelectObject(&penFg);
		if (enabled)
		{
			CPoint p1(b.left + 3, b.top + 7);
			CPoint p2(b.left + 5, b.top + 9);
			CPoint p3(b.left + 9, b.top + 3);
			dc.MoveTo(p1);
			dc.LineTo(p2);
			dc.LineTo(p3);
		}
		else
		{
			dc.MoveTo(b.left + 3, b.top + 3);
			dc.LineTo(b.right - 3, b.bottom - 3);
			dc.MoveTo(b.right - 3, b.top + 3);
			dc.LineTo(b.left + 3, b.bottom - 3);
		}
		dc.SelectObject(of);
		dc.SelectObject(op);
	}

/*!
 * \brief 设置当前编辑的对话框信息并重置相关缓存（例如预览）
 * \param dlg 对话框信息指针
 */
void CRcDesignSurfaceWnd::SetDialogInfo(std::shared_ptr<SDialogInfo> dlg)
{
        // reset preview cache
        for (auto& kv : m_preview)
            DestroyPreviewItem(kv.second);
        m_preview.clear();

        // When switching to dialog view, clear any active toolbar to keep views mutually exclusive.
        ClearToolbarRenders();
        m_activeToolbar.reset();
        m_showingToolbar = false;
        m_selectedToolbarIndex = -1;
        m_selectedToolbarIndices.clear();
        m_toolbarPressedIndex = -1;

        m_dialog = dlg;
		// update cached dialog font and derived base units
		UpdateDialogFont();
		// 动态窗口缺少布局信息时，生成默认布局以便在设计面板中可视化和交互。
		PrepareDynamicDialogLayout();
		m_selectedIndex = -1;
		m_multiSelected.clear();
		m_dialogSelected = false;
		m_dragging = false;
		m_previewActive = false;
		m_dragMode = DragMode::None;
		m_activeComboId = 0;
		m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
		m_guides = Guides{};
		UpdateDialogRect();
		UpdateScrollbars();
    Invalidate(FALSE);
    // 同步通知宿主：保持与原有行为一致，确保宿主可立即响应对话框信息变更。
    // 已提供 NotifySelectionChangedDeferred 作为可选的异步通知接口（在头文件中声明），
    // 以便在未来需要避免重入时选用。但默认不使用以免改变现有调用语义。
    NotifySelectionChanged(ESurfaceNotify::SelectionChanged);
	}

/*!
 * \brief 判断给定类名是否为 ComboBox
 */
bool CRcDesignSurfaceWnd::IsComboClass(const CStringW& cls) const
{
    CStringW up = cls;
    up.MakeUpper();
    return up == L"COMBOBOX";
}

/*!
 * \brief 获取 ComboBox 折叠（未展开）时在像素坐标下的高度
 * \param droppedRect 折叠/展开时的区域（像素）
 */
int CRcDesignSurfaceWnd::GetComboCollapsedHeightPx(const CRect& droppedRect) const
{
		// Collapsed height should look like a normal 1-line combobox.
		// Use menu metrics as a reasonable approximation, and clamp to dropped height.
		int h = std::max(18, ::GetSystemMetrics(SM_CYMENUSIZE));
		h = std::min(h, std::max(1, droppedRect.Height()));
		return h;
	}

/*!
 * \brief 获取 ComboBox 展开时的画布矩形（资源中高度视为 dropped 高度）
 */
CRect CRcDesignSurfaceWnd::GetComboDroppedRectCanvas(const CRect& ctrlCanvasRect) const
{
		// RC rect height is treated as dropped height.
		return ctrlCanvasRect;
	}

/*!
 * \brief 获取 ComboBox 折叠状态下在画布上的矩形
 */
CRect CRcDesignSurfaceWnd::GetComboCollapsedRectCanvas(const CRect& droppedCanvasRect) const
{
		CRect r = droppedCanvasRect;
		r.bottom = r.top + GetComboCollapsedHeightPx(droppedCanvasRect);
		return r;
	}

/*!
 * \brief 近似计算折叠状态下下拉按钮在画布坐标中的矩形
 */
CRect CRcDesignSurfaceWnd::GetComboDropButtonRectCanvas(const CRect& collapsedCanvasRect) const
{
		// Approximate the drop-down button area on the right.
		const int btnW = std::max(16, ::GetSystemMetrics(SM_CXVSCROLL));
		CRect r = collapsedCanvasRect;
		r.left = std::max(r.left, r.right - btnW);
		return r;
	}

/*!
 * \brief 根据控件 ID 选择控件，支持 Ctrl 多选切换
 * \param nCtrlId 控件 ID
 * \param ctrlDown 是否按下 Ctrl
 */
void CRcDesignSurfaceWnd::SelectControl(UINT nCtrlId, bool ctrlDown)
{
		if (!m_dialog)
			return;

		m_activeComboId = 0;
		m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
		m_guides = Guides{};

		int found = -1;
		for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
		{
			auto c = m_dialog->vecCtrlInfo[i];
			if (c && c->nIDC == nCtrlId &&
                (!m_levelManagerConfigMode || !IsLevelAuditControlHidden(*c)))
			{
				found = i;
				break;
			}
		}
		if (found < 0)
			return;

    m_dialogSelected = false;
    if (ctrlDown)
    {
        // toggle selection of the found index
        auto it = std::find(m_multiSelected.begin(), m_multiSelected.end(), found);
        if (it == m_multiSelected.end())
        {
            m_multiSelected.push_back(found);
            m_selectedIndex = found;
        }
        else
        {
            m_multiSelected.erase(it);
            if (m_multiSelected.empty())
                m_selectedIndex = -1;
            else
                m_selectedIndex = m_multiSelected.front();
        }
    }
    else
    {
        m_multiSelected.clear();
        m_selectedIndex = found;
        m_multiSelected.push_back(found);
    }

    NotifySelectionChanged(0);
    Invalidate(FALSE);
	}

/*!
 * \brief 清除当前所有选择状态
 */
void CRcDesignSurfaceWnd::ClearSelection()
{
		m_activeComboId = 0;
		m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
		m_guides = Guides{};
		m_selectedIndex = -1;
		m_multiSelected.clear();
		m_dialogSelected = false;
		NotifySelectionChanged(0);
		Invalidate(FALSE);
	}

/*!
 * \brief 通知宿主窗口选择已改变（同步发送消息）
 * \param code 自定义代码，宿主可据此区分来源
 */
void CRcDesignSurfaceWnd::NotifySelectionChanged(ESurfaceNotify code, LPARAM data) const
{
    HWND h = ::GetParent(GetSafeHwnd());
    if (!h || !::IsWindow(h))
        return;
    // 使用同步 SendMessage 通知宿主。wParam 使用枚举值（已按类型转换为 UINT），
    // lParam 可携带额外数据（例如工具栏按钮的 itemId）。
    ::SendMessageW(h, WM_APP_DESIGNSURFACE, (WPARAM)static_cast<UINT>(code), data);
}

void CRcDesignSurfaceWnd::NotifySelectionChangedDeferred(ESurfaceNotify code, LPARAM data) const
{
    HWND h = ::GetParent(GetSafeHwnd());
    if (!h || !::IsWindow(h))
        return;
    // 异步通知：使用 PostMessage 将通知排入宿主窗口消息队列，避免在当前调用栈中触发宿主的
    // 同步处理器（可能会重建树或修改设计面板），从而导致重入或意外选择跳转。
    // wParam 使用枚举值（转换为 UINT），lParam 携带额外数据。
    ::PostMessageW(h, WM_APP_DESIGNSURFACE, (WPARAM)static_cast<UINT>(code), data);
}

/*!
 * \brief 获取系统对话框基准 DLU 单位
 * \return 返回 CSize(基准X, 基准Y)
 */
CSize CRcDesignSurfaceWnd::GetSystemDialogBaseUnits()
{
		DWORD dwUnits = ::GetDialogBaseUnits();
		return CSize(LOWORD(dwUnits), HIWORD(dwUnits));
	}

	int CRcDesignSurfaceWnd::DluToPxX(int dluX, CSize baseUnits)
	{
		return ::MulDiv(dluX, std::max(1L, baseUnits.cx), 4);
	}

	int CRcDesignSurfaceWnd::DluToPxY(int dluY, CSize baseUnits)
	{
		return ::MulDiv(dluY, std::max(1L, baseUnits.cy), 8);
	}

	CRect CRcDesignSurfaceWnd::DluToPxRect(const CRect& dluRect, CSize baseUnits)
	{
		int x = DluToPxX(dluRect.left, baseUnits);
		int y = DluToPxY(dluRect.top, baseUnits);
		int w = DluToPxX(dluRect.Width(), baseUnits);
		int h = DluToPxY(dluRect.Height(), baseUnits);
		return CRect(x, y, x + w, y + h);
	}

	int CRcDesignSurfaceWnd::GetBorderX() const
	{
		DWORD style = m_dialog ? m_dialog->dwStyle : (WS_CAPTION | WS_POPUP);
		bool thick = (style & WS_THICKFRAME) != 0;
		int bx = ::GetSystemMetrics(thick ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME);
		if ((style & DS_MODALFRAME) != 0)
			bx = std::max(bx, ::GetSystemMetrics(SM_CXDLGFRAME));
		return std::max(0, bx);
	}

	int CRcDesignSurfaceWnd::GetBorderY() const
	{
		DWORD style = m_dialog ? m_dialog->dwStyle : (WS_CAPTION | WS_POPUP);
		bool thick = (style & WS_THICKFRAME) != 0;
		int by = ::GetSystemMetrics(thick ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME);
		if ((style & DS_MODALFRAME) != 0)
			by = std::max(by, ::GetSystemMetrics(SM_CYDLGFRAME));
		return std::max(0, by);
	}

	int CRcDesignSurfaceWnd::GetCaptionHeight() const
	{
		DWORD style = m_dialog ? m_dialog->dwStyle : (WS_CAPTION | WS_POPUP);
		return ((style & WS_CAPTION) != 0) ? ::GetSystemMetrics(SM_CYCAPTION) : 0;
	}

	void CRcDesignSurfaceWnd::UpdateDialogRect()
	{
		// Mirror RC editor convention: `rectDlg` stores dialog client size in DLU.
		// Convert to client px, then expand to window px by adding non-client metrics.
		CSize clientPx(420, 260);
		CString title = _T("Dialog");
		if (m_dialog)
		{
			clientPx = CSize(
				DluToPxX(std::max(1L, (LONG)m_dialog->rectDlg.Width()), m_baseUnits),
				DluToPxY(std::max(1L, (LONG)m_dialog->rectDlg.Height()), m_baseUnits));
			title = CString(m_dialog->csCaption);
			UNREFERENCED_PARAMETER(title);
		}

		const int bx = GetBorderX();
		const int by = GetBorderY();
		const int cap = GetCaptionHeight();

		const int winW = std::max<int>(1, clientPx.cx + bx * 2);
		const int winH = std::max<int>(1, clientPx.cy + by * 2 + cap);
		CRect win(0, 0, winW, winH);
        m_dialogRectCanvas = win;

        // Keep dialog in positive canvas space by anchoring with a small margin like VS
        // 然后根据当前滚动偏移（m_scroll）进行一次性纠偏。注意：之前代码对 m_dialogRectCanvas
        // 执行了两次 OffsetRect(-m_scroll)，导致在滚动后坐标出现双倍偏移，从而引发命中测试/选择
        // 偏移错误。这里确保只对画布做一次反向平移修正。
        const int margin = 15;
        // 对话框在画布上的位置应当是固定的（以 margin 为偏移），
        // 不应在此处再根据 m_scroll 做反向平移。
        // 鼠标坐标由 ClientToCanvas() 使用 m_scroll 进行转换为画布坐标，
        // 因此在画布空间中对话框保持恒定位置更易保持坐标系统一致性。
        m_dialogRectCanvas.OffsetRect(margin, margin);

		// If the dialog rect is too small to hold non-client, still keep sane
		if (m_dialogRectCanvas.Width() < bx * 2 + 20)
			m_dialogRectCanvas.right = m_dialogRectCanvas.left + (bx * 2 + 20);
		if (m_dialogRectCanvas.Height() < by * 2 + cap + 20)
			m_dialogRectCanvas.bottom = m_dialogRectCanvas.top + (by * 2 + cap + 20);
	}

	CRect CRcDesignSurfaceWnd::GetDialogRectCanvas() const
	{
		return m_dialogRectCanvas;
	}

	CRect CRcDesignSurfaceWnd::GetDialogClientRectCanvas() const
	{
		CRect r = m_dialogRectCanvas;
		r.DeflateRect(GetBorderX(), GetBorderY());
		r.top += GetCaptionHeight();
		return r;
	}

	CPoint CRcDesignSurfaceWnd::GetDialogClientOriginCanvas() const
	{
		CRect rc = GetDialogClientRectCanvas();
		return CPoint(rc.left, rc.top);
	}

	CPoint CRcDesignSurfaceWnd::ClientToCanvas(const CPoint& clientPt) const
	{
		return CPoint(clientPt.x + m_scroll.x, clientPt.y + m_scroll.y);
	}

	bool CRcDesignSurfaceWnd::CanvasPointToDialogClient(const CPoint& canvasPt, CPoint& outDlgPt) const
	{
		CRect cli = GetDialogClientRectCanvas();
		if (!cli.PtInRect(canvasPt))
			return false;
		outDlgPt = CPoint(canvasPt.x - cli.left, canvasPt.y - cli.top);
		return true;
	}

	CRect CRcDesignSurfaceWnd::DialogRectToCanvas(const CRect& dlgRectPx) const
	{
		CPoint org = GetDialogClientOriginCanvas();
		CRect out = dlgRectPx;
		out.OffsetRect(org);
		return out;
	}

	void CRcDesignSurfaceWnd::DrawDialogFrame(CDC& dc) const
	{
		CRect win = GetDialogRectCanvas();
		if (win.IsRectEmpty())
			return;

    // 使用系统颜色以获得更原生/类 Visual Studio 的外观。
    const COLORREF frameBorderDark = ::GetSysColor(COLOR_3DSHADOW);
    const COLORREF frameBorderLight = ::GetSysColor(COLOR_3DHILIGHT);
    const COLORREF captionFill = ::GetSysColor(COLOR_3DFACE);
    const COLORREF captionText = ::GetSysColor(COLOR_WINDOWTEXT);
    const COLORREF clientFill = ::GetSysColor(COLOR_WINDOW);
    const COLORREF clientBorder = ::GetSysColor(COLOR_3DSHADOW);

    // 窗口表面（背景 + 外部边框）
    CRect rc = win;
    dc.FillSolidRect(rc, captionFill);
    // 绘制略厚的类 VS 风格浅蓝边框，仅用于视觉效果，不改变布局度量。
    // 通过绘制若干嵌套的 3D 矩形来实现。
    const int visualBorderW = 3; // visual-only border thickness
    const COLORREF borderInner = RGB(140, 170, 210);
    const COLORREF borderOuter = RGB(200, 220, 245);
    for (int i = 0; i < visualBorderW; ++i)
    {
        CRect r2 = rc;
        r2.DeflateRect(i, i, i, i);
        // draw a two-tone border for slight 3D impression
        dc.Draw3dRect(r2, borderOuter, borderInner);
    }

        // 标题栏绘制
		int capH = (m_dialog && ((m_dialog->dwStyle & WS_CAPTION) != 0)) ? GetCaptionHeight() : 0;
		if (capH > 0)
		{
			CRect cap(win.left + 1, win.top + 1, win.right - 1, std::min(win.bottom - 1, win.top + 1 + capH));
            // 绘制简单的垂直渐变以近似 VS 的标题栏
			COLORREF titleStart = RGB(60, 120, 200); // darker
			COLORREF titleEnd = RGB(120, 170, 230); // lighter
			int h = cap.Height();
			if (h <= 0) h = 1;
            // Use GetRValue/GetGValue/GetBValue to extract color channels correctly
            BYTE sR = GetRValue(titleStart);
            BYTE sG = GetGValue(titleStart);
            BYTE sB = GetBValue(titleStart);
            BYTE eR = GetRValue(titleEnd);
            BYTE eG = GetGValue(titleEnd);
            BYTE eB = GetBValue(titleEnd);
            for (int i = 0; i < h; ++i)
            {
                float t = (h > 1) ? (float)i / (float)(h - 1) : 0.0f;
                int r = (int)((1 - t) * sR + t * eR);
                int g = (int)((1 - t) * sG + t * eG);
                int b = (int)((1 - t) * sB + t * eB);
                dc.FillSolidRect(cap.left, cap.top + i, cap.Width(), 1, RGB(r, g, b));
            }
            // 下边线
			dc.FillSolidRect(cap.left, cap.bottom - 1, cap.Width(), 1, clientBorder);

			CString title = m_dialog ? CString(m_dialog->csCaption) : _T("");
			dc.SetBkMode(TRANSPARENT);
			dc.SetTextColor(RGB(255, 255, 255));
            // 尝试绘制加粗标题文本（保留默认字体实现）
			LOGFONT lf{};
			CFont* sfont = CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT));
			if (sfont)
			{
				CFont* oldf = dc.SelectObject(sfont);
				int n = ::GetObjectType((HGDIOBJ)sfont->GetSafeHandle());
				dc.SelectObject(oldf);
			}
            // 带左边距绘制标题
			CRect tr = cap;
			tr.left += 8;
			tr.right -= 8;
			dc.DrawText(title, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
		}

        // 客户区（白色）
		CRect cli = GetDialogClientRectCanvas();
		if (!cli.IsRectEmpty())
		{
			dc.FillSolidRect(cli, clientFill);
			dc.Draw3dRect(cli, clientBorder, clientBorder);
		}
	}

	void CRcDesignSurfaceWnd::DrawGrid(CDC& dc, const CRect& dialogClientCanvas) const
	{
		if (!m_showGrid)
			return;
		if (dialogClientCanvas.IsRectEmpty())
			return;
		const int step = std::max(2, m_gridStep);
		const COLORREF dot = RGB(205, 205, 205);

		// ResourceHacker-like dotted grid: draw pixels at step intersections.
		for (int y = dialogClientCanvas.top + 1; y < dialogClientCanvas.bottom - 1; y += step)
		{
			for (int x = dialogClientCanvas.left + 1; x < dialogClientCanvas.right - 1; x += step)
				dc.SetPixelV(x, y, dot);
		}
	}

	void CRcDesignSurfaceWnd::DrawGuides(CDC& dc, const CRect& dialogClientCanvas) const
	{
		if (!m_showGuides)
			return;
		if (dialogClientCanvas.IsRectEmpty())
			return;
		if (!m_guides.hasH && !m_guides.hasV)
			return;

		CPen pen(PS_SOLID, 1, RGB(255, 0, 128));
		CPen* old = dc.SelectObject(&pen);
		if (m_guides.hasV)
		{
			dc.MoveTo(m_guides.x, dialogClientCanvas.top);
			dc.LineTo(m_guides.x, dialogClientCanvas.bottom);
		}
		if (m_guides.hasH)
		{
			dc.MoveTo(dialogClientCanvas.left, m_guides.y);
			dc.LineTo(dialogClientCanvas.right, m_guides.y);
		}
		dc.SelectObject(old);
	}

	void CRcDesignSurfaceWnd::UpdateGuidesForMove(const CRect& movingDlgPx)
	{
		m_guides = Guides{};
    if (!m_dialog || m_selectedIndex < 0)
        return;

		// Candidates: dialog edges + centers of other controls.
		const int snapPx = 4;
		int bestDx = snapPx + 1;
		int bestDy = snapPx + 1;
		int bestX = 0;
		int bestY = 0;

		const int ml = movingDlgPx.left;
		const int mr = movingDlgPx.right;
		const int mt = movingDlgPx.top;
		const int mb = movingDlgPx.bottom;
		const int mcx = (ml + mr) / 2;
		const int mcy = (mt + mb) / 2;

		std::vector<int> candidatesX;
		std::vector<int> candidatesY;
		candidatesX.reserve((size_t)m_dialog->vecCtrlInfo.size() * 3 + 2);
		candidatesY.reserve((size_t)m_dialog->vecCtrlInfo.size() * 3 + 2);

		// Dialog client edges
		CRect cliCanvas = GetDialogClientRectCanvas();
		CRect cliDlg(0, 0, cliCanvas.Width(), cliCanvas.Height());
		candidatesX.push_back(cliDlg.left);
		candidatesX.push_back(cliDlg.right);
		candidatesY.push_back(cliDlg.top);
		candidatesY.push_back(cliDlg.bottom);

		for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
		{
			if (i == m_selectedIndex)
				continue;
			auto c = m_dialog->vecCtrlInfo[i];
			if (!c)
				continue;
			CRect r = DluToPxRect(c->ctrlRect, m_baseUnits);
			candidatesX.push_back(r.left);
			candidatesX.push_back(r.right);
			candidatesX.push_back((r.left + r.right) / 2);
			candidatesY.push_back(r.top);
			candidatesY.push_back(r.bottom);
			candidatesY.push_back((r.top + r.bottom) / 2);
		}

		const int anchorsX[] = { ml, mr, mcx };
		const int anchorsY[] = { mt, mb, mcy };

		for (int cx : candidatesX)
		{
			for (int ax : anchorsX)
			{
				int d = std::abs(cx - ax);
				if (d <= snapPx && d < bestDx)
				{
					bestDx = d;
					bestX = cx;
				}
			}
		}
		for (int cy : candidatesY)
		{
			for (int ay : anchorsY)
			{
				int d = std::abs(cy - ay);
				if (d <= snapPx && d < bestDy)
				{
					bestDy = d;
					bestY = cy;
				}
			}
		}

		if (bestDx <= snapPx)
		{
			m_guides.hasV = true;
			m_guides.x = GetDialogClientOriginCanvas().x + bestX;
		}
		if (bestDy <= snapPx)
		{
			m_guides.hasH = true;
			m_guides.y = GetDialogClientOriginCanvas().y + bestY;
		}
	}

	int CRcDesignSurfaceWnd::HitTestItem(const CPoint& canvasPt) const
	{
		if (!m_dialog)
			return -1;
		CPoint dlgPt;
		if (!CanvasPointToDialogClient(canvasPt, dlgPt))
			return -1;

		// Collect all hits and choose the best one.
		// GroupBox often overlaps its children; prefer child controls.
		auto IsGroupBox = [](const std::shared_ptr<SCtrlInfo>& c) -> bool
		{
			if (!c)
				return false;
			CStringW cls = c->csClassName;
			cls.MakeUpper();
			// Win32 group box is BUTTON with BS_GROUPBOX; some sources store it as "GROUPBOX".
			if (cls == L"GROUPBOX")
				return true;
			if (cls == L"BUTTON" && ((c->dwStyle & BS_GROUPBOX) != 0))
				return true;
			return false;
		};

		int bestIdx = -1;
		bool bestIsGroup = true;
		int bestArea = INT_MAX;
		for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
		{
			auto c = m_dialog->vecCtrlInfo[i];
			if (!c || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*c)))
				continue;
			CRect r = DluToPxRect(c->ctrlRect, m_baseUnits);
			// COMBOBOX: hit-test should use collapsed rect unless it's currently expanded.
			if (IsComboClass(c->csClassName) && !IsActiveCombo(c->nIDC))
				r.bottom = r.top + GetComboCollapsedHeightPx(r);
			if (!r.PtInRect(dlgPt))
				continue;

			const bool isGroup = IsGroupBox(c);
			const int area = std::max(0, r.Width()) * std::max(0, r.Height());
			if (bestIdx < 0)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
				continue;
			}

			// Prefer non-group over group
			if (bestIsGroup && !isGroup)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
				continue;
			}
			if (!bestIsGroup && isGroup)
				continue;

			// Then prefer smaller area (more specific control).
			if (area < bestArea)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
			}
		}

		return bestIdx;
	}

	// Hit test using dialog-client coordinates (dlgPt in dialog-client pixel space)
	int CRcDesignSurfaceWnd::HitTestItemDlgClient(const CPoint& dlgPt) const
	{
		if (!m_dialog)
			return -1;

		// Collect all hits and choose the best one.
		auto IsGroupBox = [](const std::shared_ptr<SCtrlInfo>& c) -> bool
		{
			if (!c)
				return false;
			CStringW cls = c->csClassName;
			cls.MakeUpper();
			if (cls == L"GROUPBOX")
				return true;
			if (cls == L"BUTTON" && ((c->dwStyle & BS_GROUPBOX) != 0))
				return true;
			return false;
		};

		int bestIdx = -1;
		bool bestIsGroup = true;
		int bestArea = INT_MAX;
		for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
		{
			auto c = m_dialog->vecCtrlInfo[i];
			if (!c || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*c)))
				continue;
			CRect r = DluToPxRect(c->ctrlRect, m_baseUnits);
			if (IsComboClass(c->csClassName) && !IsActiveCombo(c->nIDC))
				r.bottom = r.top + GetComboCollapsedHeightPx(r);
			if (!r.PtInRect(dlgPt))
				continue;

			const bool isGroup = IsGroupBox(c);
			const int area = std::max(0, r.Width()) * std::max(0, r.Height());
			if (bestIdx < 0)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
				continue;
			}

			if (bestIsGroup && !isGroup)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
				continue;
			}
			if (!bestIsGroup && isGroup)
				continue;
			if (area < bestArea)
			{
				bestIdx = i;
				bestIsGroup = isGroup;
				bestArea = area;
			}
		}

		return bestIdx;
	}

	CRcDesignSurfaceWnd::DragMode CRcDesignSurfaceWnd::HitTestHandleOnly(const CPoint& canvasPt, const CRect& canvasRect) const
	{
		static const DragMode modes[] = { DragMode::SizeNW, DragMode::SizeN, DragMode::SizeNE, DragMode::SizeE, DragMode::SizeSE, DragMode::SizeS, DragMode::SizeSW, DragMode::SizeW };
		for (auto m : modes)
		{
			CRect hr = HandleRectCanvas(m, canvasRect);
			if (hr.PtInRect(canvasPt))
				return m;
		}
		return DragMode::None;
	}

	CRect CRcDesignSurfaceWnd::HandleRectCanvas(DragMode mode, const CRect& canvasRect) const
	{
		const int half = 6 / 2;
		CRect out(0, 0, 0, 0);
		switch (mode)
		{
		case DragMode::SizeNW:
			out = CRect(canvasRect.left - half, canvasRect.top - half, canvasRect.left + half, canvasRect.top + half);
			break;
		case DragMode::SizeN:
			out = CRect(canvasRect.CenterPoint().x - half, canvasRect.top - half, canvasRect.CenterPoint().x + half, canvasRect.top + half);
			break;
		case DragMode::SizeNE:
			out = CRect(canvasRect.right - half, canvasRect.top - half, canvasRect.right + half, canvasRect.top + half);
			break;
		case DragMode::SizeE:
			out = CRect(canvasRect.right - half, canvasRect.CenterPoint().y - half, canvasRect.right + half, canvasRect.CenterPoint().y + half);
			break;
		case DragMode::SizeSE:
			out = CRect(canvasRect.right - half, canvasRect.bottom - half, canvasRect.right + half, canvasRect.bottom + half);
			break;
		case DragMode::SizeS:
			out = CRect(canvasRect.CenterPoint().x - half, canvasRect.bottom - half, canvasRect.CenterPoint().x + half, canvasRect.bottom + half);
			break;
		case DragMode::SizeSW:
			out = CRect(canvasRect.left - half, canvasRect.bottom - half, canvasRect.left + half, canvasRect.bottom + half);
			break;
		case DragMode::SizeW:
			out = CRect(canvasRect.left - half, canvasRect.CenterPoint().y - half, canvasRect.left + half, canvasRect.CenterPoint().y + half);
			break;
		default:
			break;
		}
		return out;
	}

	CRcDesignSurfaceWnd::DragMode CRcDesignSurfaceWnd::HitTestDialogBorderCanvas(const CPoint& canvasPt) const
	{
		CRect r = GetDialogRectCanvas();
		if (r.IsRectEmpty())
			return DragMode::None;
		const int hit = 5;
		CRect hitRc = r;
		hitRc.InflateRect(hit, hit);
		if (!hitRc.PtInRect(canvasPt))
			return DragMode::None;

		bool left = std::abs(canvasPt.x - r.left) <= hit;
		bool right = std::abs(canvasPt.x - r.right) <= hit;
		bool top = std::abs(canvasPt.y - r.top) <= hit;
		bool bottom = std::abs(canvasPt.y - r.bottom) <= hit;
		if (left && top) return DragMode::SizeNW;
		if (right && top) return DragMode::SizeNE;
		if (right && bottom) return DragMode::SizeSE;
		if (left && bottom) return DragMode::SizeSW;
		if (top) return DragMode::SizeN;
		if (right) return DragMode::SizeE;
		if (bottom) return DragMode::SizeS;
		if (left) return DragMode::SizeW;
		return DragMode::None;
	}

	HCURSOR CRcDesignSurfaceWnd::CursorForDragMode(DragMode mode) const
	{
		switch (mode)
		{
		case DragMode::SizeNW:
		case DragMode::SizeSE:
			return ::LoadCursor(nullptr, IDC_SIZENWSE);
		case DragMode::SizeNE:
		case DragMode::SizeSW:
			return ::LoadCursor(nullptr, IDC_SIZENESW);
		case DragMode::SizeN:
		case DragMode::SizeS:
			return ::LoadCursor(nullptr, IDC_SIZENS);
		case DragMode::SizeE:
		case DragMode::SizeW:
			return ::LoadCursor(nullptr, IDC_SIZEWE);
		case DragMode::Move:
			return ::LoadCursor(nullptr, IDC_SIZEALL);
		default:
			return ::LoadCursor(nullptr, IDC_ARROW);
		}
	}

	CRect CRcDesignSurfaceWnd::NormalizeRect(const CRect& r)
	{
		CRect out = r;
		if (out.left > out.right) std::swap(out.left, out.right);
		if (out.top > out.bottom) std::swap(out.top, out.bottom);
		return out;
	}

	CRect CRcDesignSurfaceWnd::NormalizeAndClampItemRect(const CRect& dlgRectPx) const
	{
		CRect out = NormalizeRect(dlgRectPx);
		CRect cliCanvas = GetDialogClientRectCanvas();
		CRect cli(0, 0, cliCanvas.Width(), cliCanvas.Height());
		int minW = 16;
		int minH = 14;
		if (out.Width() < minW) out.right = out.left + minW;
		if (out.Height() < minH) out.bottom = out.top + minH;

		int w = out.Width();
		int h = out.Height();
		out.left = std::max(cli.left, std::min(out.left, cli.right - w));
		out.top = std::max(cli.top, std::min(out.top, cli.bottom - h));
		out.right = out.left + w;
		out.bottom = out.top + h;
		return out;
	}

	CRect CRcDesignSurfaceWnd::ApplyDragItem(const CRect& start, const CPoint& delta, DragMode mode) const
	{
		CRect out = start;
		switch (mode)
		{
		case DragMode::Move: out.OffsetRect(delta); break;
		case DragMode::SizeNW: out.left += delta.x; out.top += delta.y; break;
		case DragMode::SizeN: out.top += delta.y; break;
		case DragMode::SizeNE: out.right += delta.x; out.top += delta.y; break;
		case DragMode::SizeE: out.right += delta.x; break;
		case DragMode::SizeSE: out.right += delta.x; out.bottom += delta.y; break;
		case DragMode::SizeS: out.bottom += delta.y; break;
		case DragMode::SizeSW: out.left += delta.x; out.bottom += delta.y; break;
		case DragMode::SizeW: out.left += delta.x; break;
		default: break;
		}
		// Dropped-height resize for COMBOBOX should not be clamped to dialog client.
		if (m_dragOnComboDrop)
			return NormalizeRect(out);
		return NormalizeAndClampItemRect(out);
	}

	CRect CRcDesignSurfaceWnd::ApplyDragDialogCanvas(const CRect& start, const CPoint& delta, DragMode mode) const
	{
		CRect out = start;
		switch (mode)
		{
		case DragMode::Move: out.OffsetRect(delta); break;
		case DragMode::SizeNW: out.left += delta.x; out.top += delta.y; break;
		case DragMode::SizeN: out.top += delta.y; break;
		case DragMode::SizeNE: out.right += delta.x; out.top += delta.y; break;
		case DragMode::SizeE: out.right += delta.x; break;
		case DragMode::SizeSE: out.right += delta.x; out.bottom += delta.y; break;
		case DragMode::SizeS: out.bottom += delta.y; break;
		case DragMode::SizeSW: out.left += delta.x; out.bottom += delta.y; break;
		case DragMode::SizeW: out.left += delta.x; break;
		default: break;
		}
		CRect norm = NormalizeRect(out);
		return norm;
	}

	CPoint CRcDesignSurfaceWnd::MaybeSnap(const CPoint& v) const
	{
		// same as CDesignSurfaceWnd: snap to 1px grid
		auto snap = [](int x) { return x; };
		return CPoint(snap(v.x), snap(v.y));
	}

	void CRcDesignSurfaceWnd::DrawSelectionCanvas(CDC& dc, const CRect& canvasRect) const
	{
		CPen pen(PS_SOLID, 1, RGB(0, 120, 215));
		CPen* old = dc.SelectObject(&pen);
		CBrush* oldBr = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
		dc.Rectangle(canvasRect);
		dc.SelectObject(oldBr);
		dc.SelectObject(old);

		CBrush br;
		br.CreateStockObject(WHITE_BRUSH);
		CPen pen2(PS_SOLID, 1, RGB(0, 120, 215));
		CPen* op2 = dc.SelectObject(&pen2);
		CBrush* ob2 = dc.SelectObject(&br);
		static const DragMode modes[] = { DragMode::SizeNW, DragMode::SizeN, DragMode::SizeNE, DragMode::SizeE, DragMode::SizeSE, DragMode::SizeS, DragMode::SizeSW, DragMode::SizeW };
		for (auto m : modes)
			dc.Rectangle(HandleRectCanvas(m, canvasRect));
		dc.SelectObject(ob2);
		dc.SelectObject(op2);
	}

	void CRcDesignSurfaceWnd::DrawPreviewRectCanvas(CDC& dc, const CRect& canvasRect) const
	{
		CPen pen(PS_DOT, 1, RGB(0, 0, 0));
		CPen* old = dc.SelectObject(&pen);
		CBrush* oldBr = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
		dc.Rectangle(canvasRect);
		dc.SelectObject(oldBr);
		dc.SelectObject(old);
	}

	void CRcDesignSurfaceWnd::OnPaint()
	{
		CPaintDC dcPaint(this);
		CRect client;
		GetClientRect(&client);
		if (client.IsRectEmpty())
			return;

        // 使用双缓冲绘制以避免闪烁
		CDC memDC;
		memDC.CreateCompatibleDC(&dcPaint);
		CBitmap bmp;
		bmp.CreateCompatibleBitmap(&dcPaint, client.Width(), client.Height());
		CBitmap* oldBmp = memDC.SelectObject(&bmp);
		memDC.SetBkMode(TRANSPARENT);

        // 工作区背景（类似 Visual Studio），填充整个后备缓冲，与相机/滚动无关
        memDC.FillSolidRect(&client, ::GetSysColor(COLOR_APPWORKSPACE));

        // 根据当前滚动设置视口变换，使世界/画布坐标映射到设备坐标（将滚动视为相机偏移）。
        // 在绘制背景后设置视口，以保证背景覆盖整个后备缓冲而不受相机原点影响。
		int savedDC = memDC.SaveDC();
		::SetViewportOrgEx(memDC.GetSafeHdc(), -m_scroll.x, -m_scroll.y, nullptr);

        // 决定是否应绘制工具栏预览。
        // 若设置了对话框并且该对话框标记为动态窗口，则优先绘制对话框而非将其当作工具栏预览，
        // 即便 m_showingToolbar 为 true 也以对话框为准。
        bool effectiveShowToolbar = m_showingToolbar && (!m_dialog || !m_dialog->m_blIsDynamic);

        // 如果不应显示工具栏预览，则在下面绘制对话框内容。
        if (!effectiveShowToolbar)
        {
            UpdateDialogRect();
            DrawDialogFrame(memDC);

            // 在对话框客户区内绘制网格
            DrawGrid(memDC, GetDialogClientRectCanvas());
        }
        else
        {
            // still need dialog metrics for placing toolbar, but skip drawing controls
            UpdateDialogRect();
        }

		const bool drawBadges = ShouldDrawLevelBadges();
        if (!effectiveShowToolbar && m_dialog)
        {
            // 绘制每个控件：尽可能通过 WM_PRINT 使用真实控件进行预览渲染
            for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
            {
                auto c = m_dialog->vecCtrlInfo[i];
                // 等级管理中不绘制资源原本禁用的控件及其等级标记。
                if (!c || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*c)))
                    continue;
                CRect dlgPx = DluToPxRect(c->ctrlRect, m_baseUnits);
                CRect canvas = DialogRectToCanvas(dlgPx);

                const bool isCombo = IsComboClass(c->csClassName);
                CRect canvasDropped = canvas;
                CRect canvasCollapsed = canvas;
                CRect canvasDraw = canvas;
                CSize previewSize(canvas.Width(), canvas.Height());
                if (isCombo)
                {
                    canvasDropped = GetComboDroppedRectCanvas(canvas);
                    canvasCollapsed = GetComboCollapsedRectCanvas(canvasDropped);
                    canvasDraw = IsActiveCombo(c->nIDC) ? canvasDropped : canvasCollapsed;
                    previewSize = CSize(canvasDropped.Width(), canvasDropped.Height());
                    if (IsActiveCombo(c->nIDC))
                        m_activeComboDrawRectCanvas = canvasDraw;
                }

                bool drawn = false;
                PreviewItem& pit = m_preview[c->nIDC];
                CStringW cls = c->csClassName;
                CStringW text = c->csText;
                if (EnsurePreviewHwnd(pit, c->dwExStyle, cls, text, c->dwStyle, previewSize))
                    drawn = PrintPreviewToDc(memDC, pit.hwnd, canvasDraw);
                if (drawn)
                {
                    // 如果使用真实控件 WM_PRINT 成功绘制，对于 List/Tree 也额外绘制边框
                    // 以保证预览控件与回退绘制在边界上的一致性（解决 6.3）。
                    CStringW upCls = cls;
                    upCls.MakeUpper();
                    bool isListOrTreePreview = (upCls.Find(L"LIST") >= 0) || (upCls.Find(L"LISTVIEW") >= 0) || (upCls.Find(L"TREE") >= 0) || (upCls.Find(L"SYSLISTVIEW32") >= 0) || (upCls.Find(L"SYSTREEVIEW32") >= 0);
                    if (isListOrTreePreview)
                    {
                        // 画一个 1px 的 3D 边框，增强边界可见性
                        memDC.Draw3dRect(canvasDraw, RGB(180,180,180), RGB(180,180,180));
                        CRect inner = canvasDraw;
                        inner.DeflateRect(1,1,1,1);
                        memDC.Draw3dRect(inner, RGB(230,230,230), RGB(230,230,230));
                    }
                }
                else
                {
                    DrawFallbackControl(memDC, canvasDraw, cls, text, c->dwStyle, c->dwExStyle);
                }

                if (isCombo && IsActiveCombo(c->nIDC))
                {
                    const COLORREF shadowCol = RGB(180, 180, 180);
                    memDC.FillSolidRect(canvasDraw.right + 1, canvasDraw.top + 2, 2, canvasDraw.Height() - 1, shadowCol);
                    memDC.FillSolidRect(canvasDraw.left + 2, canvasDraw.bottom + 1, canvasDraw.Width() - 1, 2, shadowCol);
                    memDC.Draw3dRect(canvasDraw, RGB(140, 140, 140), RGB(140, 140, 140));
                }

                if (drawBadges)
                {
                    // 跳过不支持交互的控件（例如 STATIC 标签或 GroupBox）以减少视觉干扰
                    CStringW clsUp = cls;
                    clsUp.MakeUpper();
                    bool isNonInteractive = false;
                    if ((clsUp == L"STATIC" && !(c->dwStyle & SS_NOTIFY)) || clsUp == L"GROUPBOX")
                        isNonInteractive = true;
                    if (!isNonInteractive && clsUp == L"BUTTON")
                    {
                        const DWORD interactiveBits = BS_CHECKBOX | BS_AUTOCHECKBOX | BS_RADIOBUTTON | BS_AUTORADIOBUTTON;
                        if ((c->dwStyle & BS_GROUPBOX) != 0 && (c->dwStyle & interactiveBits) == 0)
                            isNonInteractive = true;
                    }

                    if (!isNonInteractive)
                    {
                        bool enabled = c->GetLevel(std::max(0, m_currentLevel - 1));
                        DrawLevelBadgeOverlay(memDC, canvasDraw, m_currentLevel, enabled);
                    }
                }

                bool selected = std::find(m_multiSelected.begin(), m_multiSelected.end(), i) != m_multiSelected.end();
                if (selected)
                    DrawSelectionCanvas(memDC, canvasDraw);
            }
        }

        // 拖拽时在控件之上绘制参考线
        DrawGuides(memDC, GetDialogClientRectCanvas());

        // 如果当前选择的是工具栏节点，在对话框客户区顶部绘制工具栏预览。
        if (effectiveShowToolbar && m_activeToolbar)
        {
            CRect cli = GetDialogClientRectCanvas();
            // 一致的布局常量
            const int toolbar_margin = 8; // distance from client left/top
            const int button_padding = 4; // inner padding around buttons inside tbRect
            const int button_gap = 4; // gap between buttons

            // 从缓存渲染中获取按钮尺寸（若可用）
            int btnW = 16;
            int btnH = 16;
            auto trIt = m_toolbarRenders.find(m_activeToolbar->nToolbarID);
            const ToolbarRender* trPtr = (trIt != m_toolbarRenders.end()) ? &trIt->second : nullptr;
            if (trPtr)
            {
                btnW = trPtr->btnW;
                btnH = trPtr->btnH;
            }
            else
            {
                btnW = m_activeToolbar->wWidth > 0 ? m_activeToolbar->wWidth : 16;
                btnH = m_activeToolbar->wHeight > 0 ? m_activeToolbar->wHeight : 16;
            }

            size_t count = m_activeToolbar->vecItemIDs.size();
            int totalButtonsWidth = (int)count * btnW + (int)std::max<size_t>(0, count > 0 ? count - 1 : 0) * button_gap;
            int tbLeft = cli.left + toolbar_margin;
            int tbTop = cli.top + toolbar_margin;
            int tbRight = tbLeft + totalButtonsWidth + button_padding * 2;
            int tbBottom = tbTop + btnH + button_padding * 2;
            CRect tbRect(tbLeft, tbTop, tbRight, tbBottom);

            // 绘制工具栏背景和边框
            memDC.FillSolidRect(tbRect, ::GetSysColor(COLOR_3DFACE));
            memDC.Draw3dRect(tbRect, RGB(160,160,160), RGB(200,200,200));

            // 确保渲染缓存已准备
            EnsureToolbarRender(m_activeToolbar->nToolbarID, *m_activeToolbar);
            auto it2 = m_toolbarRenders.find(m_activeToolbar->nToolbarID);
            trPtr = (it2 != m_toolbarRenders.end()) ? &it2->second : trPtr;

            int x = tbRect.left + button_padding;
            int y = tbRect.top + button_padding;
            for (size_t i = 0; i < count; ++i)
            {
                HBITMAP hb = nullptr;
                bool isPlaceholder = false;
                if (trPtr && i < trPtr->btnBmps.size())
                    hb = trPtr->btnBmps[i];
                // 如果源切片宽度为零，则视为占位符并绘制分隔符样式
                if (!hb)
                    isPlaceholder = true;
                HWND btnHwnd = nullptr;
                if (trPtr && i < trPtr->btnHwnds.size())
                    btnHwnd = trPtr->btnHwnds[i];
                CRect btnRect(x, y, x + btnW, y + btnH);
                if (!isPlaceholder && hb)
                {
                    CDC mem;
                    mem.CreateCompatibleDC(&memDC);
                    CBitmap bmp;
                    bmp.Attach(hb);
                    CBitmap* old = mem.SelectObject(&bmp);
                    memDC.BitBlt(x, y, btnW, btnH, &mem, 0, 0, SRCCOPY);
                    mem.SelectObject(old);
                    bmp.Detach();
                }
                else
                {
                    // 占位符风格：浅灰背景 + 居中 1px 深色分隔线，两侧留白，避免重边框
                    memDC.FillSolidRect(x, y, btnW, btnH, RGB(245,245,245));
                    int sepW = 1;
                    int sepX = x + btnW / 2 - sepW / 2;
                    // 分隔线垂直留 2px 内边距
                    memDC.FillSolidRect(sepX, y + 2, sepW, btnH - 4, RGB(160,160,160));
                    // 在分隔线两侧画更浅的辅助线，制造细微高光/阴影感
                    memDC.FillSolidRect(sepX - 1, y + 2, 1, btnH - 4, RGB(220,220,220));
                    memDC.FillSolidRect(sepX + 1, y + 2, 1, btnH - 4, RGB(200,200,200));
                }

                // 按下时的视觉反馈
                if (m_toolbarPressedIndex == (int)i)
                {
                    memDC.Draw3dRect(x - 1, y - 1, btnW + 2, btnH + 2, RGB(100,100,100), RGB(150,150,150));
                }
                // draw multi-selection overlay for toolbar buttons
                if (std::find(m_selectedToolbarIndices.begin(), m_selectedToolbarIndices.end(), (int)i) != m_selectedToolbarIndices.end())
                {
                    DrawSelectionCanvas(memDC, btnRect);
                }

                // 与运行时一致：高等级继承低等级权限，缺失掩码默认允许。
                // 复用普通控件的红叉/绿勾，不改动工具栏的权限数据。
                if (m_levelManagerConfigMode && m_activeToolbar->m_blIsInLevelManager &&
                    m_activeToolbar->vecItemIDs[i] != 0)
                {
                    const int lvl = std::max(1, std::min(8, m_currentLevel));
                    const UINT inheritedBits = (1u << lvl) - 1u;
                    bool enabled = true;
                    if (i < m_activeToolbar->vecLevelMask.size())
                        enabled = (m_activeToolbar->vecLevelMask[i] & inheritedBits) != 0;
                    DrawLevelBadgeOverlay(memDC, btnRect, lvl, enabled);
                }
                x += btnW + button_gap;
            }
        }


		if (m_dialogSelected)
			DrawSelectionCanvas(memDC, GetDialogRectCanvas());

		if (m_previewActive)
		{
			if (m_dragOnDialog)
				DrawPreviewRectCanvas(memDC, m_previewDialogRectCanvas);
			else
				DrawPreviewRectCanvas(memDC, DialogRectToCanvas(m_previewItemRect));
		}

        // 在位图拷贝到屏幕之前恢复 DC 状态（包括视口）
        memDC.RestoreDC(savedDC);

        // 此处不绘制全局工具栏渲染：活动工具栏在上方已根据 m_showingToolbar 绘制。
		dcPaint.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
		memDC.SelectObject(oldBmp);
	}

	void CRcDesignSurfaceWnd::UpdateScrollbars()
	{
		if (m_inUpdateScrollbars)
			return;
		m_inUpdateScrollbars = true;

		CRect client;
		GetClientRect(&client);

		CRect dlg = GetDialogRectCanvas();
    int contentW = std::max<int>((int)client.Width(), (int)dlg.right + 60);
    int contentH = std::max<int>((int)client.Height(), (int)dlg.bottom + 60);

    // If a toolbar is showing, ensure content size accounts for toolbar extent
    if (m_showingToolbar && m_activeToolbar)
    {
        // toolbar is drawn inside dialog client at offset (left + 8, top + 8)
        CRect cli = GetDialogClientRectCanvas();
        int gap = 6;
        int btnW = 16, btnH = 16;
        auto trIt = m_toolbarRenders.find(m_activeToolbar->nToolbarID);
        if (trIt != m_toolbarRenders.end())
        {
            btnW = trIt->second.btnW;
            btnH = trIt->second.btnH;
        }
        else
        {
            btnW = m_activeToolbar->wWidth > 0 ? m_activeToolbar->wWidth : btnW;
            btnH = m_activeToolbar->wHeight > 0 ? m_activeToolbar->wHeight : btnH;
        }
        int tbWidth = (int)m_activeToolbar->vecItemIDs.size() * (btnW + gap) + 16; // include margins
        int tbRight = cli.left + 8 + tbWidth;
        int tbBottom = cli.top + 8 + btnH + 8;
        contentW = std::max(contentW, tbRight + 60);
        contentH = std::max(contentH, tbBottom + 60);
    }
		m_contentSize = CSize(contentW, contentH);

		int maxX = std::max<int>(0, (int)m_contentSize.cx - (int)client.Width());
		int maxY = std::max<int>(0, (int)m_contentSize.cy - (int)client.Height());
		m_scroll.x = std::max(0L, std::min((LONG)maxX, m_scroll.x));
		m_scroll.y = std::max(0L, std::min((LONG)maxY, m_scroll.y));

		UpdateHScroll();
		UpdateVScroll();

		m_inUpdateScrollbars = false;
	}

	void CRcDesignSurfaceWnd::UpdateHScroll()
	{
		CRect client;
		GetClientRect(&client);
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nPage = (UINT)std::max<int>(1, (int)client.Width());
		si.nMax = std::max<int>((int)si.nPage - 1, (int)m_contentSize.cx - 1);
		si.nPos = (int)m_scroll.x;
		SetScrollInfo(SB_HORZ, &si, TRUE);
	}

	void CRcDesignSurfaceWnd::UpdateVScroll()
	{
		CRect client;
		GetClientRect(&client);
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nPage = (UINT)std::max<int>(1, (int)client.Height());
		si.nMax = std::max<int>((int)si.nPage - 1, (int)m_contentSize.cy - 1);
		si.nPos = (int)m_scroll.y;
		SetScrollInfo(SB_VERT, &si, TRUE);
	}

	void CRcDesignSurfaceWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
	{
		UNREFERENCED_PARAMETER(nPos);
		UNREFERENCED_PARAMETER(pScrollBar);
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		if (!GetScrollInfo(SB_HORZ, &si))
			return;

		int x = (int)m_scroll.x;
		switch (nSBCode)
		{
		case SB_LINELEFT: x -= 20; break;
		case SB_LINERIGHT: x += 20; break;
		case SB_PAGELEFT: x -= (int)si.nPage; break;
		case SB_PAGERIGHT: x += (int)si.nPage; break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK: x = (int)si.nTrackPos; break;
		default: break;
		}
		int maxX = std::max(0, (int)si.nMax - (int)si.nPage + 1);
		x = std::max(0, std::min(maxX, x));
		if (x != m_scroll.x)
		{
			m_scroll.x = x;
			UpdateHScroll();
			Invalidate(FALSE);
		}
		CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
	}

	void CRcDesignSurfaceWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
	{
		UNREFERENCED_PARAMETER(nPos);
		UNREFERENCED_PARAMETER(pScrollBar);
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		if (!GetScrollInfo(SB_VERT, &si))
			return;

		int y = (int)m_scroll.y;
		switch (nSBCode)
		{
		case SB_LINEUP: y -= 20; break;
		case SB_LINEDOWN: y += 20; break;
		case SB_PAGEUP: y -= (int)si.nPage; break;
		case SB_PAGEDOWN: y += (int)si.nPage; break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK: y = (int)si.nTrackPos; break;
		default: break;
		}
		int maxY = std::max(0, (int)si.nMax - (int)si.nPage + 1);
		y = std::max(0, std::min(maxY, y));
		if (y != m_scroll.y)
		{
			m_scroll.y = y;
			UpdateVScroll();
			Invalidate(FALSE);
		}
		CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
	}

	void CRcDesignSurfaceWnd::OnLButtonDown(UINT nFlags, CPoint point)
	{
		SetFocus();
		SetCapture();
        // Ensure dialog/layout metrics and scrollbars are up-to-date before hit-testing.
        UpdateDialogRect();
        UpdateScrollbars();

        CPoint canvasPt = ClientToCanvas(point);
        bool ctrlDown = (nFlags & MK_CONTROL) != 0;
        bool shiftDown = (nFlags & MK_SHIFT) != 0;

        // If toolbar preview is showing, prefer handling toolbar hit first and
        // do not allow clicks to fall through to dialog controls.
        if (m_showingToolbar && m_activeToolbar)
        {
            CRect cli = GetDialogClientRectCanvas();
            const int toolbar_margin = 8;
            const int button_padding = 4;
            const int button_gap = 4;
            int btnW = 16, btnH = 16;
            auto trItTop = m_toolbarRenders.find(m_activeToolbar->nToolbarID);
            if (trItTop != m_toolbarRenders.end())
            {
                btnW = trItTop->second.btnW;
                btnH = trItTop->second.btnH;
            }
            else
            {
                btnW = m_activeToolbar->wWidth > 0 ? m_activeToolbar->wWidth : btnW;
                btnH = m_activeToolbar->wHeight > 0 ? m_activeToolbar->wHeight : btnH;
            }
            size_t count = m_activeToolbar->vecItemIDs.size();
            int totalButtonsWidth = (int)count * btnW + (int)std::max<size_t>(0, count > 0 ? count - 1 : 0) * button_gap;
            CRect tbRect(cli.left + toolbar_margin, cli.top + toolbar_margin, cli.left + toolbar_margin + totalButtonsWidth + button_padding * 2, cli.top + toolbar_margin + btnH + button_padding * 2);
            if (tbRect.PtInRect(canvasPt))
            {
                int x = canvasPt.x - (tbRect.left + button_padding);
                int idx = x / (btnW + button_gap);
                if (idx >= 0 && idx < (int)count)
                {
                    // If this toolbar item is a placeholder (ID==0), ignore clicks
                    UINT itemId = m_activeToolbar->vecItemIDs.size() > (size_t)idx ? m_activeToolbar->vecItemIDs[idx] : 0;
                    if (itemId != 0)
                    {
                        m_toolbarPressedIndex = idx;
                        // Support multi-selection with Ctrl (toggle) and Shift (additive).
                        if (ctrlDown)
                        {
                            auto it = std::find(m_selectedToolbarIndices.begin(), m_selectedToolbarIndices.end(), idx);
                            if (it == m_selectedToolbarIndices.end())
                                m_selectedToolbarIndices.push_back(idx);
                            else
                                m_selectedToolbarIndices.erase(it);
                        }
                        else if (shiftDown)
                        {
                            // additive: append if not present
                            if (std::find(m_selectedToolbarIndices.begin(), m_selectedToolbarIndices.end(), idx) == m_selectedToolbarIndices.end())
                                m_selectedToolbarIndices.push_back(idx);
                        }
                        else
                        {
                            // replace selection
                            m_selectedToolbarIndices.clear();
                            m_selectedToolbarIndices.push_back(idx);
                        }
                        m_selectedToolbarIndex = m_selectedToolbarIndices.empty() ? -1 : m_selectedToolbarIndices.front();
                        // Notify host so property grid can show toolbar button properties
                        NotifySelectionChanged(ESurfaceNotify::ToolbarSelectionChanged);
                        Invalidate(FALSE);
                    }
                    return; // consume click and do not hit-test dialog
                }
            }
            else
            {
                // Click outside toolbar area: clear toolbar selection so host/property grid
                // no longer shows toolbar properties.
                m_selectedToolbarIndices.clear();
                m_selectedToolbarIndex = -1;
                m_toolbarPressedIndex = -1;
                Invalidate(FALSE);
                NotifySelectionChanged(ESurfaceNotify::ToolbarSelectionChanged);
                // If there's no dialog currently set, stop here to avoid further dialog hit-testing
                // (we made toolbar and dialog views mutually exclusive; clicking background only clears toolbar).
                if (!m_dialog)
                    return;
                // otherwise fall through to allow interacting with the dialog
            }
        }

		// If a combo dropdown is open:
		// - Click outside dismisses
		// - Dragging the bottom edge adjusts dropped height (and persists to RC)
		if (m_activeComboId != 0)
		{
			if (m_activeComboDrawRectCanvas.IsRectEmpty())
			{
				m_activeComboId = 0;
				ReleaseCapture();
				return;
			}

			// Outside click closes.
			if (!m_activeComboDrawRectCanvas.PtInRect(canvasPt))
			{
				m_activeComboId = 0;
				m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
				Invalidate(FALSE);
				ReleaseCapture();
				return;
			}

			// Bottom edge drag => resize dropped height.
			const int edge = 4;
			if (std::abs(canvasPt.y - m_activeComboDrawRectCanvas.bottom) <= edge)
			{
				if (m_levelManagerConfigMode)
				{
					// In config mode, we allow closing but disallow resizing/dragging.
					ReleaseCapture();
					return;
				}
				// ensure selected index matches active combo
				int found = -1;
				for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
				{
					auto c = m_dialog->vecCtrlInfo[i];
					if (c && c->nIDC == m_activeComboId)
					{
						found = i;
						break;
					}
				}
				if (found >= 0)
				{
					m_selectedIndex = found;
					m_multiSelected.clear();
					m_multiSelected.push_back(found);
					m_dialogSelected = false;

					CPoint dlgPt;
					if (CanvasPointToDialogClient(canvasPt, dlgPt))
					{
						m_dragging = true;
						m_dragOnDialog = false;
						m_dragOnComboDrop = true;
						m_previewActive = true;
						m_dragMode = DragMode::SizeS;
						m_dragStartPt = dlgPt;
						auto c = m_dialog->vecCtrlInfo[m_selectedIndex];
						m_dragStartRect = DluToPxRect(c->ctrlRect, m_baseUnits);
						m_previewItemRect = m_dragStartRect;
						Invalidate(FALSE);
						return;
					}
				}
			}

			// Otherwise keep dropdown open and do not begin other drags.
			ReleaseCapture();
			return;
		}

		// dialog resize handles
		DragMode dmDlg = HitTestDialogBorderCanvas(canvasPt);
		if (dmDlg != DragMode::None)
		{
			if (m_levelManagerConfigMode)
			{
				// In config mode, disallow moving/resizing dialog.
				ReleaseCapture();
				return;
			}
			m_activeComboId = 0;
			m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
			m_dialogSelected = true;
			m_selectedIndex = -1;
			m_multiSelected.clear();
			m_dragging = true;
			m_dragOnDialog = true;
			m_previewActive = true;
			m_dragMode = dmDlg;
			m_dragStartPtCanvas = canvasPt;
			m_dragStartDialogRectCanvas = GetDialogRectCanvas();
			m_previewDialogRectCanvas = m_dragStartDialogRectCanvas;
			Invalidate(FALSE);
			return;
		}

        // item selection
        // Convert canvas point to dialog-client coordinates and perform hit test there.
        CPoint dlgPt;
        bool inDlg = CanvasPointToDialogClient(canvasPt, dlgPt);
        int idx = -1;
        if (inDlg)
            idx = HitTestItemDlgClient(dlgPt);
        if (idx >= 0)
        {
            m_dialogSelected = false;

            // Selection behavior:
            // - Shift: additive (always add indices)
            // - Ctrl: toggle (existing behavior)
            // - none: replace selection
            auto cBefore = m_dialog->vecCtrlInfo[idx];
            if (cBefore)
            {
                // Use index-based selection to handle multiple STATICs with same nIDC (-1)
                SelectControlByIndex(idx, ctrlDown, shiftDown);
            }

            // If ctrl-click toggled selection off, do not start drag.
            if (m_selectedIndex < 0)
            {
                // selection was cleared
                return;
            }

            auto c = m_dialog->vecCtrlInfo[m_selectedIndex];
            if (!c)
                return;

            CRect startDlgPx = DluToPxRect(c->ctrlRect, m_baseUnits);
            CRect startCanvas = DialogRectToCanvas(startDlgPx);
            const bool isCombo = IsComboClass(c->csClassName);
            const bool isComboExpanded = isCombo && IsActiveCombo(c->nIDC);
            if (isCombo && !isComboExpanded)
            {
                // For interactions (hit, drag, clamp), combos should behave as collapsed height.
                startDlgPx.bottom = startDlgPx.top + GetComboCollapsedHeightPx(startDlgPx);
                startCanvas = DialogRectToCanvas(startDlgPx);
            }

            // Combo dropdown button toggles an expanded preview, without entering drag mode.
            if (isCombo)
            {
                // Use original dropped rect for the dropdown toggle visuals.
                CRect dropped = GetComboDroppedRectCanvas(DialogRectToCanvas(DluToPxRect(c->ctrlRect, m_baseUnits)));
                CRect collapsed = GetComboCollapsedRectCanvas(dropped);
                CRect btn = GetComboDropButtonRectCanvas(collapsed);
                if (btn.PtInRect(canvasPt))
                {
                    if (m_activeComboId == c->nIDC)
                        m_activeComboId = 0;
                    else
                        m_activeComboId = c->nIDC;
                    m_activeComboDrawRectCanvas = (m_activeComboId != 0) ? dropped : CRect(0, 0, 0, 0);
            NotifySelectionChanged(ESurfaceNotify::SelectionChanged);
                    Invalidate(FALSE);
                    return;
                }
            }

            if (m_levelManagerConfigMode)
            {
                // Config mode: allow selecting, but block drag/resize.
                m_dragging = false;
                m_previewActive = false;
                m_dragMode = DragMode::None;
                NotifySelectionChanged(0);
                Invalidate(FALSE);
                ReleaseCapture();
                return;
            }

            DragMode dm = HitTestHandleOnly(canvasPt, startCanvas);
            if (dm == DragMode::None)
                dm = DragMode::Move;

            m_activeComboId = 0;
            m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);

            CanvasPointToDialogClient(canvasPt, dlgPt);
            m_dragging = true;
            m_dragOnDialog = false;
            m_dragOnComboDrop = false;
            m_previewActive = true;
            m_dragMode = dm;
            m_dragStartPt = dlgPt;
            m_dragStartRect = startDlgPx;
            m_previewItemRect = startDlgPx;
                NotifySelectionChanged(ESurfaceNotify::SelectionChanged);
            Invalidate(FALSE);
            return;
        }
        // If click was not on any item but inside dialog client, mark possible marquee start
        if (GetDialogRectCanvas().PtInRect(canvasPt))
        {
            // Save restore selection; don't start marquee until mouse moves
            m_marqueePossible = true;
            m_restoreSelectedIndex = m_selectedIndex;
            m_restoreMultiSelected = m_multiSelected;
            m_marqueeStartCanvasPt = canvasPt;
            // compute dlg-client point
            CanvasPointToDialogClient(canvasPt, dlgPt);
            m_marqueeStartDlgPt = dlgPt;
            // keep selection state unchanged now; actual marquee begins on move
            // fall through so clicking still selects dialog when appropriate
        }

		// click inside dialog selects dialog
		if (GetDialogRectCanvas().PtInRect(canvasPt))
		{
			if (m_levelManagerConfigMode)
			{
				// Config mode: allow selecting dialog, but no drag.
				m_activeComboId = 0;
				m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
				m_dialogSelected = true;
				m_selectedIndex = -1;
				m_multiSelected.clear();
				NotifySelectionChanged(0);
				Invalidate(FALSE);
				ReleaseCapture();
				return;
			}
			m_activeComboId = 0;
			m_activeComboDrawRectCanvas = CRect(0, 0, 0, 0);
			m_dialogSelected = true;
			m_selectedIndex = -1;
			m_multiSelected.clear();
                NotifySelectionChanged(ESurfaceNotify::SelectionChanged);
			Invalidate(FALSE);
			return;
		}

        // Check if click is on toolbar area when showing toolbar
        if (m_showingToolbar && m_activeToolbar)
        {
            CRect cli = GetDialogClientRectCanvas();
            const int toolbar_margin = 8;
            const int button_padding = 4;
            const int button_gap = 4;
            int btnW = 16, btnH = 16;
            auto trItEnd = m_toolbarRenders.find(m_activeToolbar->nToolbarID);
            if (trItEnd != m_toolbarRenders.end())
            {
                btnW = trItEnd->second.btnW;
                btnH = trItEnd->second.btnH;
            }
            else
            {
                btnW = m_activeToolbar->wWidth > 0 ? m_activeToolbar->wWidth : btnW;
                btnH = m_activeToolbar->wHeight > 0 ? m_activeToolbar->wHeight : btnH;
            }
            size_t count = m_activeToolbar->vecItemIDs.size();
            int totalButtonsWidth = (int)count * btnW + (int)std::max<size_t>(0, count > 0 ? count - 1 : 0) * button_gap;
            CRect tbRect(cli.left + toolbar_margin, cli.top + toolbar_margin, cli.left + toolbar_margin + totalButtonsWidth + button_padding * 2, cli.top + toolbar_margin + btnH + button_padding * 2);
            if (tbRect.PtInRect(canvasPt))
            {
                int x = canvasPt.x - (tbRect.left + button_padding);
                int idx = x / (btnW + button_gap);
                if (idx >= 0 && idx < (int)count)
                {
                    // Ignore clicks on placeholder separators (ID==0)
                    UINT itemId = m_activeToolbar->vecItemIDs.size() > (size_t)idx ? m_activeToolbar->vecItemIDs[idx] : 0;
                    if (itemId != 0)
                    {
                        m_toolbarPressedIndex = idx;
                        Invalidate(FALSE);
                    }
                    return; // consume click
                }
            }
        }

        ClearSelection();
	}

	void CRcDesignSurfaceWnd::OnLButtonUp(UINT nFlags, CPoint point)
	{
		UNREFERENCED_PARAMETER(nFlags);
		UNREFERENCED_PARAMETER(point);
		if (::GetCapture() == GetSafeHwnd())
			ReleaseCapture();

	// clear possible marquee flag on mouse up
	m_marqueePossible = false;

	if (!m_dragging)
		return;

	// If marquee selection was active, commit selection and skip other commit logic
	if (m_marqueeSelecting)
	{
		m_marqueeSelecting = false;
		CRect selRectDlg = NormalizeRect(m_previewItemRect);
		// if user just clicked without dragging, restore previous selection
		if (selRectDlg.Width() < 4 && selRectDlg.Height() < 4)
		{
			m_selectedIndex = m_restoreSelectedIndex;
			m_multiSelected = m_restoreMultiSelected;
			NotifySelectionChanged(0);
		}
		else
		{
			// support Shift-additive marquee: if shift held, add hits to existing selection
			bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			std::vector<int> hits;
			if (m_dialog)
			{
				for (int i = 0; i < (int)m_dialog->vecCtrlInfo.size(); ++i)
				{
					auto c = m_dialog->vecCtrlInfo[i];
					if (!c || (m_levelManagerConfigMode && IsLevelAuditControlHidden(*c))) continue;
					CRect rpx = DluToPxRect(c->ctrlRect, m_baseUnits);
					if (!(selRectDlg.right <= rpx.left || selRectDlg.left >= rpx.right || selRectDlg.bottom <= rpx.top || selRectDlg.top >= rpx.bottom))
						hits.push_back(i);
				}
			}

			if (!shiftDown)
			{
				m_multiSelected.clear();
				for (int idx : hits)
					m_multiSelected.push_back(idx);
				if (!m_multiSelected.empty())
					m_selectedIndex = m_multiSelected.front();
				else
					m_selectedIndex = -1;
			}
			else
			{
				// additive: append hits that are not already selected
				for (int idx : hits)
				{
					auto it = std::find(m_multiSelected.begin(), m_multiSelected.end(), idx);
					if (it == m_multiSelected.end())
						m_multiSelected.push_back(idx);
				}
				if (!m_multiSelected.empty() && m_selectedIndex < 0)
					m_selectedIndex = m_multiSelected.front();
			}
			NotifySelectionChanged(0);
		}
		m_previewActive = false;
		m_dragging = false;
		Invalidate(FALSE);
		return;
	}

		if (m_previewActive)
		{
			if (m_dragOnDialog)
			{
				// commit dialog size back to DLU
				if (m_dialog)
				{
					auto pxToDluX = [&](int px) { return ::MulDiv(px, 4, std::max(1L, m_baseUnits.cx)); };
					auto pxToDluY = [&](int px) { return ::MulDiv(px, 8, std::max(1L, m_baseUnits.cy)); };
					const int bx = GetBorderX();
					const int by = GetBorderY();
					const int cap = GetCaptionHeight();
					int clientWpx = std::max(1, m_previewDialogRectCanvas.Width() - bx * 2);
					int clientHpx = std::max(1, m_previewDialogRectCanvas.Height() - by * 2 - cap);
					int wDlu = std::max(1, pxToDluX(clientWpx));
					int hDlu = std::max(1, pxToDluY(clientHpx));
					m_dialog->rectDlg.right = m_dialog->rectDlg.left + wDlu;
					m_dialog->rectDlg.bottom = m_dialog->rectDlg.top + hDlu;
					NotifySelectionChanged(3);
				}
			}
			else if (m_dialog && m_selectedIndex >= 0)
			{
				auto c = m_dialog->vecCtrlInfo[m_selectedIndex];
				if (c)
				{
					auto pxToDluX = [&](int px) { return ::MulDiv(px, 4, std::max(1L, m_baseUnits.cx)); };
					auto pxToDluY = [&](int px) { return ::MulDiv(px, 8, std::max(1L, m_baseUnits.cy)); };
					int x = pxToDluX(m_previewItemRect.left);
					int y = pxToDluY(m_previewItemRect.top);
					int w = pxToDluX(m_previewItemRect.Width());
					int h = pxToDluY(m_previewItemRect.Height());

					// ComboBox:
					// - Normal move/size keeps dropped height
					// - Dropdown resize drag updates dropped height
					if (IsComboClass(c->csClassName))
					{
						if (!m_dragOnComboDrop)
							h = std::max(1, c->ctrlRect.Height());
						else
							h = std::max(1, h);
					}

					c->ctrlRect = CRect(x, y, x + w, y + h);
					NotifySelectionChanged(2);
				}
			}
		}

		m_dragging = false;
		m_previewActive = false;
		m_dragOnDialog = false;
		m_dragOnComboDrop = false;
		m_dragMode = DragMode::None;
		m_guides = Guides{};

		// If a toolbar button was pressed, treat as click and notify host via selection change
		if (m_toolbarPressedIndex >= 0 && m_activeToolbar)
		{
			int idx = m_toolbarPressedIndex;
			m_toolbarPressedIndex = -1;
			// Send host message: use code 100 to indicate toolbar button click and pass item id in lParam
			UINT itemId = m_activeToolbar->vecItemIDs.size() > (size_t)idx ? m_activeToolbar->vecItemIDs[idx] : 0;
			HWND h = ::GetParent(GetSafeHwnd());
            if (h && ::IsWindow(h))
                NotifySelectionChanged(ESurfaceNotify::ToolbarButtonClicked, (LPARAM)itemId);
			Invalidate(FALSE);
		}
		UpdateDialogRect();
		UpdateScrollbars();
		Invalidate(FALSE);
	}

	void CRcDesignSurfaceWnd::OnMouseMove(UINT nFlags, CPoint point)
	{
		bool ctrlDown = (nFlags & MK_CONTROL) != 0;
		CPoint canvasPt = ClientToCanvas(point);

		// If we're doing marquee selection, update preview rectangle even when no dlg/control selected
		if (m_marqueeSelecting)
		{
			// derive dlg client point; allow dragging outside dialog by clamping to dialog client bounds
			CPoint dlgPt;
			CRect cliCanvas = GetDialogClientRectCanvas();
			if (CanvasPointToDialogClient(canvasPt, dlgPt))
			{
				// dlgPt already in dialog client coords
			}
			else
			{
				// clamp to dialog client rect in canvas coords
				int x = std::max(cliCanvas.left, std::min(cliCanvas.right, canvasPt.x));
				int y = std::max(cliCanvas.top, std::min(cliCanvas.bottom, canvasPt.y));
				dlgPt.x = x - cliCanvas.left;
				dlgPt.y = y - cliCanvas.top;
			}

			CRect r(m_dragStartPt, dlgPt);
			r = NormalizeRect(r);
			m_previewItemRect = r;
			Invalidate(FALSE);
			return;
		}

	// If marquee was possible (mouse down in empty area) and mouse moved enough, start marquee
	if (m_marqueePossible)
	{
		// begin marquee selection
		m_marqueePossible = false;
		m_marqueeSelecting = true;
		m_previewActive = true;
		m_dragging = true;
		m_dragOnDialog = false;
		m_dragMode = DragMode::None;
		m_dragStartPt = m_marqueeStartDlgPt;
		// fall through to update rectangle immediately
		// derive dlg client point now
		CPoint dlgPt2;
		CRect cliCanvas2 = GetDialogClientRectCanvas();
		if (CanvasPointToDialogClient(canvasPt, dlgPt2)) {}
		else
		{
			int x = std::max(cliCanvas2.left, std::min(cliCanvas2.right, canvasPt.x));
			int y = std::max(cliCanvas2.top, std::min(cliCanvas2.bottom, canvasPt.y));
			dlgPt2.x = x - cliCanvas2.left;
			dlgPt2.y = y - cliCanvas2.top;
		}

		CRect r2(m_dragStartPt, dlgPt2);
		r2 = NormalizeRect(r2);
		m_previewItemRect = r2;
		Invalidate(FALSE);
		return;
	}

		if (!m_dragging || !m_previewActive)
			return;

		if (m_dragOnDialog)
		{
			CPoint delta(canvasPt.x - m_dragStartPtCanvas.x, canvasPt.y - m_dragStartPtCanvas.y);
			delta = MaybeSnap(delta);
			CRect next = ApplyDragDialogCanvas(m_dragStartDialogRectCanvas, delta, m_dragMode);
			if (next != m_previewDialogRectCanvas)
			{
				CRect inv = m_previewDialogRectCanvas;
				inv.UnionRect(&inv, &next);
				inv.InflateRect(16, 16);
				m_previewDialogRectCanvas = next;
				InvalidateRect(&inv, FALSE);
			}
			return;
		}

		if (!m_dialog || m_selectedIndex < 0)
			return;

		CPoint dlgPt;
		if (!CanvasPointToDialogClient(canvasPt, dlgPt))
			return;
		CPoint delta(dlgPt.x - m_dragStartPt.x, dlgPt.y - m_dragStartPt.y);
		if (!ctrlDown)
			delta = MaybeSnap(delta);
		CRect next = ApplyDragItem(m_dragStartRect, delta, m_dragMode);
		if (!m_dragOnComboDrop)
		{
			UpdateGuidesForMove(next);
			int dx = 0;
			int dy = 0;
			if (m_guides.hasV)
			{
				const int ax = (m_dragMode == DragMode::Move) ? next.left : next.right;
				// Align the moving rect (left/right/center) to guide line in dialog client coords.
				// We don't know which anchor matched; choose the closest among left/right/center.
				const int want = m_guides.x - GetDialogClientOriginCanvas().x;
				const int anchors[] = { next.left, next.right, (next.left + next.right) / 2 };
				int best = anchors[0];
				int bestAbs = std::abs(want - anchors[0]);
				for (int a : anchors)
				{
					int ad = std::abs(want - a);
					if (ad < bestAbs)
					{
						bestAbs = ad;
						best = a;
					}
				}
				dx = want - best;
				UNREFERENCED_PARAMETER(ax);
			}
			if (m_guides.hasH)
			{
				const int want = m_guides.y - GetDialogClientOriginCanvas().y;
				const int anchors[] = { next.top, next.bottom, (next.top + next.bottom) / 2 };
				int best = anchors[0];
				int bestAbs = std::abs(want - anchors[0]);
				for (int a : anchors)
				{
					int ad = std::abs(want - a);
					if (ad < bestAbs)
					{
						bestAbs = ad;
						best = a;
					}
				}
				dy = want - best;
			}
			next.OffsetRect(dx, dy);
		}
		else
			m_guides = Guides{};
		if (next != m_previewItemRect)
		{
			CRect oldCanvas = DialogRectToCanvas(m_previewItemRect);
			CRect newCanvas = DialogRectToCanvas(next);
			CRect inv = oldCanvas;
			inv.UnionRect(&inv, &newCanvas);
			inv.InflateRect(16, 16);
			m_previewItemRect = next;
			// Guides are drawn across the whole surface; invalidate full client to avoid remnants.
			Invalidate(FALSE);
		}
	}

	BOOL CRcDesignSurfaceWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
	{
		UNREFERENCED_PARAMETER(pWnd);
		UNREFERENCED_PARAMETER(nHitTest);
		UNREFERENCED_PARAMETER(message);

		POINT pt{};
		::GetCursorPos(&pt);
		ScreenToClient(&pt);
		CPoint canvasPt = ClientToCanvas(CPoint(pt));

		// Combo dropdown bottom-edge resize cursor
		if (m_activeComboId != 0 && !m_activeComboDrawRectCanvas.IsRectEmpty() && m_activeComboDrawRectCanvas.PtInRect(canvasPt))
		{
			const int edge = 4;
			if (std::abs(canvasPt.y - m_activeComboDrawRectCanvas.bottom) <= edge)
			{
				if (m_levelManagerConfigMode)
				{
					::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
					return TRUE;
				}
				::SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
				return TRUE;
			}
		}

		DragMode dmDlg = HitTestDialogBorderCanvas(canvasPt);
		if (dmDlg != DragMode::None)
		{
			if (m_levelManagerConfigMode)
			{
				::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
				return TRUE;
			}
			::SetCursor(CursorForDragMode(dmDlg));
			return TRUE;
		}

		int idx = -1;
		CPoint dlgPtForMouse;
		if (CanvasPointToDialogClient(canvasPt, dlgPtForMouse))
			idx = HitTestItemDlgClient(dlgPtForMouse);
		if (idx >= 0 && m_dialog)
		{
			auto c = m_dialog->vecCtrlInfo[idx];
			if (c)
			{
				CRect dlgPx = DluToPxRect(c->ctrlRect, m_baseUnits);
				// Cursor / handle hit-test should match visual: collapsed when not expanded.
				if (IsComboClass(c->csClassName) && !IsActiveCombo(c->nIDC))
					dlgPx.bottom = dlgPx.top + GetComboCollapsedHeightPx(dlgPx);
				CRect canvas = DialogRectToCanvas(dlgPx);
				if (m_levelManagerConfigMode)
				{
					::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
					return TRUE;
				}
				DragMode dm = HitTestHandleOnly(canvasPt, canvas);
				if (dm == DragMode::None)
					dm = DragMode::Move;
				::SetCursor(CursorForDragMode(dm));
				return TRUE;
			}
		}

		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
		return TRUE;
	}
}
