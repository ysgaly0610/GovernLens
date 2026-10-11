#pragma once
#include <afxwin.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "RcCore.h"

#ifndef WM_APP_DESIGNSURFACE
#define WM_APP_DESIGNSURFACE (WM_APP + 210)
#endif


namespace rceditor
{
    /*!\
     * \brief 设计画布窗口类，支持对话框/控件/工具栏的可视化编辑与交互\
     * \details 提供控件选择、属性编辑、等级标记、拖拽、滚动等功能\
     */
    class CRcDesignSurfaceWnd : public CWnd
    {
    public:
        /*! \brief 构造函数 */
        explicit CRcDesignSurfaceWnd() = default;
        /*! \brief 析构函数 */
        ~CRcDesignSurfaceWnd() override;

        /*!\
         * \brief 创建设计画布窗口\n
         * \param parent 父窗口\n
         * \param id 控件ID\n
         * \param rect 初始矩形\n
         * \return 创建成功返回 TRUE
         */
        BOOL Create(CWnd* parent, UINT id, const RECT& rect);

        /*! \brief 设置当前要编辑的对话框信息（数据源：RcCore::SDialogInfo） */
        void SetDialogInfo(std::shared_ptr<SDialogInfo> dlg);
        /*! \brief 显示指定工具栏资源于设计画布 */
        void SetToolbarInfo(std::shared_ptr<SToolbarInfo> tb);
        /*! \brief 清除当前活动的工具栏预览 */
        void ClearToolbarInfo();
        /*! \brief 获取当前对话框信息 */
        std::shared_ptr<SDialogInfo> GetDialogInfo() const { return m_dialog; }
        /*! 
			\brief 选中指定控件（支持Ctrl多选）
			\param nCtrlId 控件ID
			\param ctrlDown 是否Ctrl多选
		*/
        void SelectControl(UINT nCtrlId, bool ctrlDown = false);
        /*! \brief 选中指定控件（按索引）\n \param idx 控件索引\n \param ctrlDown 是否Ctrl多选\n \param shiftDown 是否Shift多选 */
        void SelectControlByIndex(int idx, bool ctrlDown = false, bool shiftDown = false);
        /*! \brief 选中工具栏子项（按索引）\n \param idx 按钮索引\n \param ctrlDown 是否Ctrl多选\n \param shiftDown 是否Shift多选 */
        void SelectToolbarByIndex(int idx, bool ctrlDown = false, bool shiftDown = false);
        /*! \brief 清空所有选中控件 */
        void ClearSelection();

        /*! \brief 选中所有控件（用于LevelManager全选） */
        void SelectAllControls();

        /*! \brief 获取当前多选的控件索引（相对于vecCtrlInfo的索引） */
        std::vector<int> GetMultiSelectedIndices() const;
        /*! \brief 设置当前等级（1-8），用于绘制等级标记 */
        void SetCurrentLevel(int level);
        /*! \brief 获取当前等级（1-8） */
        int GetCurrentLevel() const { return m_currentLevel; }

        /*! \brief 启用/禁用等级管理器配置模式\n \param enable 是否启用 */
        void SetLevelManagerConfigMode(bool enable);
        /*! \brief 是否为等级管理器配置模式 */
        bool IsLevelManagerConfigMode() const { return m_levelManagerConfigMode; }

        // ---- 属性网格集成相关 ----
        /*! \brief 是否选中对话框 */
        bool IsDialogSelected() const { return m_dialogSelected; }
        /*! \brief 获取当前选中控件索引 */
        int GetSelectedIndex() const { return m_selectedIndex; }
        /*! \brief 获取当前选中控件指针 */
        std::shared_ptr<SCtrlInfo> GetSelectedCtrl() const
        {
            if (!m_dialog || m_selectedIndex < 0 || m_selectedIndex >= (int)m_dialog->vecCtrlInfo.size())
                return nullptr;
            return m_dialog->vecCtrlInfo[m_selectedIndex];
        }
        /*! \brief 获取当前选中控件ID */
        UINT GetSelectedCtrlId() const
        {
            auto c = GetSelectedCtrl();
            return c ? c->nIDC : 0;
        }
        /*! \brief 设置对话框标题 */
        void SetDialogCaption(const CStringW& caption);
        /*! \brief 设置控件文本 */
        void SetCtrlText(UINT ctrlId, const CStringW& text);
        /*! \brief 设置控件类名 */
        void SetCtrlClass(UINT ctrlId, const CStringW& cls);
        /*! \brief 设置控件DLU矩形 */
        void SetCtrlRectDlu(UINT ctrlId, const CRect& dluRect);
        /*! \brief 设置控件风格 */
        void SetCtrlStyle(UINT ctrlId, DWORD style);
        /*! \brief 设置控件扩展风格 */
        void SetCtrlExStyle(UINT ctrlId, DWORD exStyle);
        /*! \brief 设置控件多行属性 */
        void SetCtrlMultiLine(UINT ctrlId, bool enable);

        /*! \brief 确保指定索引控件可见（自动滚动/居中） */
        void EnsureControlVisibleByIndex(int idx);

	protected:
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void OnPaint();
		afx_msg BOOL OnEraseBkgnd(CDC* pDC);
		afx_msg void OnDestroy();
		afx_msg void OnSize(UINT nType, int cx, int cy);
		afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
		afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
		DECLARE_MESSAGE_MAP()

	private:
        /*! ---- 预览窗口缓存 (WM_PRINT) ---- */
		struct PreviewItem
		{
			HWND hwnd = nullptr;
			HFONT hFont = nullptr; // per-preview HFONT (created with preview window DPI)
			CStringW cls;
			CStringW text;
			DWORD style = 0;
			DWORD exStyle = 0;
			CSize size{ 0,0 };
			bool inited = false;
		};

		HWND m_previewHostHwnd = nullptr;
		std::unordered_map<UINT, PreviewItem> m_preview;

        /*! ---- 工具栏渲染缓存 ---- */
		struct ToolbarRender
		{
			UINT id = 0;
			std::vector<HBITMAP> btnBmps; // per-button bitmap (owned)
			HBITMAP maskBmp = nullptr; // reusable white mask bitmap for disabled overlay
            /*! \brief 可选的隐藏 HWND，用于通过 WM_PRINT 实现像素级精确的按钮渲染（作为预览承载窗口的子窗口创建）。
             *  这些窗口由本类持有，清除渲染缓存时必须销毁。
             */
			std::vector<HWND> btnHwnds;
			int btnW = 16;
			int btnH = 16;
			// per-button source widths (when bitmap frames do not divide evenly)
			std::vector<int> btnWidths;
		};
		std::unordered_map<UINT, ToolbarRender> m_toolbarRenders; // key = toolbar resource id

        /*! \brief 在指定画布矩形上绘制工具栏资源 */
		void DrawToolbarCanvas(CDC& dc, const CRect& canvasRect, const SToolbarInfo& tb) const;
        /*! \brief 确保从 PE 资源加载工具栏位图到缓存（非常量方法） */
		bool EnsureToolbarRender(UINT toolbarId, const SToolbarInfo& tb);

        /*! \brief 当前激活的工具栏（设置后替代对话框显示） */
		std::shared_ptr<SToolbarInfo> m_activeToolbar;
		bool m_showingToolbar = false;
        /*! \brief 单个选中工具栏项的索引（便捷缓存） */
        int m_selectedToolbarIndex = -1;
        /*! \brief 鼠标按下时用于反馈的工具栏按钮索引（-1 表示无） */
        int m_toolbarPressedIndex = -1;
        /*! \brief 当工具栏处于激活状态时，当前选中的按钮索引集合（空表示无） */
        std::vector<int> m_selectedToolbarIndices;

        /*! \brief 工具栏选择访问器（供宿主/属性网格使用） */
    public:
        bool IsToolbarSelected() const { return m_showingToolbar && !m_selectedToolbarIndices.empty(); }
        // Return first selected index or -1
        int GetSelectedToolbarIndex() const { return m_selectedToolbarIndices.empty() ? -1 : m_selectedToolbarIndices.front(); }
        // Return all selected indices
        std::vector<int> GetSelectedToolbarIndices() const { return m_selectedToolbarIndices; }
        UINT GetSelectedToolbarItemId() const { int idx = GetSelectedToolbarIndex(); return (m_activeToolbar && idx >= 0 && (size_t)idx < m_activeToolbar->vecItemIDs.size()) ? m_activeToolbar->vecItemIDs[idx] : 0; }
        std::shared_ptr<SToolbarInfo> GetActiveToolbar() const { return m_activeToolbar; }
        // Set selected toolbar indices (replaces current selection). Notifies host.
        void SetSelectedToolbarIndices(const std::vector<int>& indices);
        /*! \brief 清除并释放所有已渲染的工具栏位图缓存 */
        void ClearToolbarRenders();
        //! 确保创建预览承载窗口（隐藏）
		void EnsurePreviewHost();
		//! 销毁预览承载窗口及其子预览控件
		void DestroyPreviewHost();
        //! 创建用于工具栏按钮渲染的隐藏 HWND（由本类持有）
		void CreateToolbarButtonHwnds(UINT toolbarId);
		//! 销毁单个预览控件
		void DestroyPreviewItem(PreviewItem& it);
		//! 确保创建/更新预览控件窗口
		bool EnsurePreviewHwnd(PreviewItem& it, DWORD exStyle, const CStringW& cls, const CStringW& text, DWORD style, const CSize& size);
		//! 按需初始化预览控件内部内容（Tab/Tree等）
		void InitPreviewContentIfNeeded(PreviewItem& it, const CStringW& cls);
        //! 将预览控件通过 WM_PRINT 绘制到指定 DC
        bool PrintPreviewToDc(CDC& dc, HWND hwnd, const CRect& targetCanvasRect) const;

        /*! ---- 模型 (Model) ---- */
		std::shared_ptr<SDialogInfo> m_dialog;

        /*! ---- 选择 (Selection) ---- */
		int m_selectedIndex = -1;
		std::vector<int> m_multiSelected;
		bool m_dialogSelected = false;
        /*! \brief Marquee（框选）选择状态 */
        bool m_marqueeSelecting = false;
        /*! \brief 可能的框选起点（在空白区域鼠标按下），拖动时激活 */
        bool m_marqueePossible = false;
        CPoint m_marqueeStartCanvasPt{}; // in canvas coords
        CPoint m_marqueeStartDlgPt{}; // in dialog-client coords
        /*! \brief 选择恢复点（在开始框选时保存） */
        int m_restoreSelectedIndex = -1;
        std::vector<int> m_restoreMultiSelected;

        /*! ---- 等级 / 徽章 (Level / badge) ---- */
		int m_currentLevel = 1;
		bool m_levelManagerConfigMode = false;

        /*! ---- 下拉组合框预览（类似 ResourceHacker） ---- */
		UINT m_activeComboId = 0; // 0 = none
		CRect m_activeComboDrawRectCanvas{}; // cached for dismiss/hit-test
		bool IsComboClass(const CStringW& cls) const;
		int GetComboCollapsedHeightPx(const CRect& droppedRect) const;
		CRect GetComboDroppedRectCanvas(const CRect& ctrlCanvasRect) const;
		CRect GetComboCollapsedRectCanvas(const CRect& droppedCanvasRect) const;
		CRect GetComboDropButtonRectCanvas(const CRect& collapsedCanvasRect) const;
		bool IsActiveCombo(UINT ctrlId) const { return m_activeComboId != 0 && m_activeComboId == ctrlId; }

        /*! ---- 度量 (Metrics) ---- */
		CSize m_baseUnits{ 0,0 };
        // Cached dialog HFONT (owned). If null, use stock DEFAULT_GUI_FONT.
        //! 缓存的对话框 HFONT（由本类持有）。若为空则使用系统默认 GUI 字体。
        HFONT m_hDialogFont = nullptr;

        //! 每个 DPI 下缓存的 HFONT（键为 LOGPIXELSY）
        std::unordered_map<int, HFONT> m_hDialogFontsPerDpi;

        /*! \brief 获取或创建指定 DPI 下的缓存 HFONT。返回存储在映射中的 HFONT（由本类持有）或 nullptr。*/
        HFONT GetCachedDialogFontForDPI(int logpixy);

        /*! \brief 根据 m_dialog->pFontInfo 更新缓存的 HFONT（按需创建/销毁） */
        void UpdateDialogFont();

        /*! \brief 从 SFontInfo 创建 HFONT（返回由调用方接管或 nullptr） */
        static HFONT CreateHfontFromSFontInfo(const SFontInfo* pfi);
        /*! \brief 在指定 DPI 下从 SFontInfo 创建 HFONT（由本类持有） */
        static HFONT CreateHfontFromSFontInfoWithDPI(const SFontInfo* pfi, int logpixy);

        /*! \brief 从给定 HFONT 计算对话框基础单位（若 hf==nullptr 则回退到系统单位） */
        static CSize ComputeBaseUnitsFromFont(HFONT hf);

        /*! ---- 滚动 (Scrolling) ---- */
		CPoint m_scroll{ 0,0 };
		CSize m_contentSize{ 0,0 };
		bool m_inUpdateScrollbars = false;

        /*! ---- 拖拽 (Drag) ---- */
		enum class DragMode
		{
			None,
			Move,
			SizeNW,
			SizeN,
			SizeNE,
			SizeE,
			SizeSE,
			SizeS,
			SizeSW,
			SizeW,
		};

		bool m_dragging = false;
		bool m_dragOnDialog = false;
		bool m_previewActive = false;
		bool m_dragOnComboDrop = false;
		CRect m_previewItemRect{};          // dialog client coords (px)
		CRect m_previewDialogRectCanvas{};  // canvas coords
		DragMode m_dragMode = DragMode::None;
		CPoint m_dragStartPt{};            // dialog client coords (px)
		CRect m_dragStartRect{};           // dialog client coords (px)
		CPoint m_dragStartPtCanvas{};
		CRect m_dragStartDialogRectCanvas{};

        /*! ---- 布局矩形 (Layout rects) ---- */
		CRect m_dialogRectCanvas{ 0,0,0,0 }; // window rect in canvas coords

        /*! ---- 帮助函数（从 CDesignSurfaceWnd 移植） ---- */
        //! 获取系统对话框 DLU 基准
		static CSize GetSystemDialogBaseUnits();
		//! DLU->像素（X轴）
		static int DluToPxX(int dluX, CSize baseUnits);
		//! DLU->像素（Y轴）
		static int DluToPxY(int dluY, CSize baseUnits);
		//! DLU 矩形->像素矩形
		static CRect DluToPxRect(const CRect& dluRect, CSize baseUnits);

		// 通知宿主窗口：选中/属性发生变化
		// 使用枚举替代纯数字以避免混淆，第二个参数用于携带可选数据（例如工具栏按钮的 itemId）
		enum class ESurfaceNotify : UINT
		{
			// 通用选择/刷新通知（无额外数据）
			SelectionChanged = 0,
			// 对话标题已修改（lParam 未使用）
			DialogCaptionChanged = 1,
			// 控件属性已修改（值已提交，lParam 未使用）
			CtrlPropertiesChanged = 2,
			// 对话大小/布局已修改（lParam 未使用）
			DialogSizeChanged = 3,
			// 工具栏视图已选择/切换（lParam 未使用）
			ToolbarSelectionChanged = 200,
			// 工具栏按钮被点击，lParam = itemId
			ToolbarButtonClicked = 100,
		};
		void NotifySelectionChanged(ESurfaceNotify code, LPARAM data = 0) const;
		// 兼容旧代码：接受 UINT 型的通知码并转换为枚举后转发
		void NotifySelectionChanged(UINT code, LPARAM data = 0) const { NotifySelectionChanged(static_cast<ESurfaceNotify>(code), data); }

		// 异步通知宿主（延迟/任务队列方式），用于在初始化/批量设置时避免同步回调导致的重入
		// 与 NotifySelectionChanged 不同，Deferred 版本会使用 PostMessage 发送消息，
		// 使宿主在当前调用栈返回后再处理通知。参考 RcCore 风格的注释（中文）。
		void NotifySelectionChangedDeferred(ESurfaceNotify code, LPARAM data = 0) const;

		//! 更新对话框窗口矩形（画布坐标）
		void UpdateDialogRect();
		//! 获取对话框窗口矩形（画布坐标，包含非客户区）
		CRect GetDialogRectCanvas() const;
		//! 获取对话框客户区矩形（画布坐标）
		CRect GetDialogClientRectCanvas() const;
		//! 获取对话框客户区原点（画布坐标）
		CPoint GetDialogClientOriginCanvas() const;
		//! 客户区坐标->画布坐标（考虑滚动偏移）
		CPoint ClientToCanvas(const CPoint& clientPt) const;
		//! 将画布坐标转换为对话框客户区坐标
		bool CanvasPointToDialogClient(const CPoint& canvasPt, CPoint& outDlgPt) const;
		//! 对话框客户区矩形（像素）->画布矩形
		CRect DialogRectToCanvas(const CRect& dlgRectPx) const;
		//! 获取窗口边框厚度（X）
		int GetBorderX() const;
		//! 获取窗口边框厚度（Y）
		int GetBorderY() const;
		//! 获取标题栏高度
		int GetCaptionHeight() const;

		//! 绘制对话框框架（含标题栏等）
		void DrawDialogFrame(CDC& dc) const;
		//! 更新滚动条（水平+垂直）
		void UpdateScrollbars();
		//! 更新水平滚动条
		void UpdateHScroll();
		//! 更新垂直滚动条
		void UpdateVScroll();

		int HitTestItem(const CPoint& canvasPt) const;
		int HitTestItemDlgClient(const CPoint& dlgPt) const;
		DragMode HitTestHandleOnly(const CPoint& canvasPt, const CRect& canvasRect) const;
		CRect HandleRectCanvas(DragMode mode, const CRect& canvasRect) const;
		DragMode HitTestDialogBorderCanvas(const CPoint& canvasPt) const;
		HCURSOR CursorForDragMode(DragMode mode) const;

		static CRect NormalizeRect(const CRect& r);
		CRect NormalizeAndClampItemRect(const CRect& dlgRectPx) const;
		CRect ApplyDragItem(const CRect& start, const CPoint& delta, DragMode mode) const;
		CRect ApplyDragDialogCanvas(const CRect& start, const CPoint& delta, DragMode mode) const;
		CPoint MaybeSnap(const CPoint& v) const;

		void DrawSelectionCanvas(CDC& dc, const CRect& canvasRect) const;
		void DrawPreviewRectCanvas(CDC& dc, const CRect& canvasRect) const;
		void DrawLevelBadgeOverlay(CDC& dc, const CRect& targetCanvasRect, int level, bool enabled) const;
		bool ShouldDrawLevelBadges() const;

        /*! ---- 网格与对齐参考线 ---- */
		struct Guides
		{
			bool hasV = false;
			int x = 0;
			bool hasH = false;
			int y = 0;
		};
		Guides m_guides;
		int m_gridStep = 8; // px
		bool m_showGrid = true;
		bool m_showGuides = true;
		void DrawGrid(CDC& dc, const CRect& dialogClientCanvas) const;
        /*! \brief 当 WM_PRINT 预览不可用时的备用控件渲染器。
         *  传入 style/exStyle 以便备用渲染能正确绘制 BS_CHECKBOX/BS_GROUPBOX/ES_MULTILINE 等类特定视觉效果。
         */
        void DrawFallbackControl(CDC& dc, const CRect& canvas, const CStringW& cls, const CStringW& text, DWORD style = 0, DWORD exStyle = 0) const;
		void DrawGuides(CDC& dc, const CRect& dialogClientCanvas) const;
		void UpdateGuidesForMove(const CRect& movingDlgPx);

		// ---- 动态窗口布局（为缺少位置信息的动态窗口生成默认布局） ----
		// 使用 VS RC 编辑器常见的默认控件大小，按行从左到右排列（每行最多 10 个），
		// 高个控件（list/tree 等）放置在后排，保证动态窗口在设计面板上可见且可交互。
		void PrepareDynamicDialogLayout();
		CSize GetDefaultDynamicCtrlSizeDlu(const CStringW& cls, bool& outTall) const;
	};
}
