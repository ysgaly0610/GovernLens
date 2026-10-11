#pragma once

#include <afxwin.h>
#include <afxcmn.h>
#include <commctrl.h>
#include <map>
#include "CDesignSurfaceWnd.h"
#include "RcEditorModel.h"
#include "CPropertyListCtrl.h"


#include "CFormRcBase.h"


/*!
 * \brief RC 编辑器主窗口基类，支持多资源文件、等级管理、属性编辑等功能
 * \details 提供树控件、设计面板、属性网格等核心 UI 及相关操作钩子
 */
class CFormRcEditor : public ParamRange::CFormNoRcBase
{
    DECLARE_DYNAMIC(CFormRcEditor)

public:
    /*!
     * \brief 构造函数
     * \param pParent 父窗口指针，可为 nullptr
     */
    explicit CFormRcEditor(CWnd* pParent = nullptr);
    /*! \brief 析构函数 */
    virtual ~CFormRcEditor();

protected:
    /*!
     * \brief 初始化对话框（MFC 钩子）
     */
    virtual BOOL OnInitDialog() override;

    /*!
     * \brief 获取窗口初始标题
     */
    virtual CStringW GetInitialCaption() const { return L"RC Editor"; }

    /*!
     * \brief 控件创建后初始化 UI 和菜单（派生类可重写）
     */
    virtual void OnAfterControlsCreated() {}

    // 内部等级索引不变，允许派生编辑器采用用户界面的等级编号。
    virtual int GetLevelDisplayNumber(int levelIndex) const { return levelIndex + 1; }

    /*!
     * \brief 对话框关闭前的处理（派生类可重写）
     */
    virtual void OnBeforeDialogClose() {}

    // ---- 派生窗口可重写的钩子 ----
    /*!
     * \brief 启用 / 禁用等级管理器配置模式
     * \param enable 是否启用
     */
    void SetDesignSurfaceLevelConfigMode(bool enable);

    /*!
     * \brief 重新创建树控件以启用 / 禁用复选框
     * \param enable 是否启用复选框
     */
    void SetTreeCheckboxesEnabled(bool enable);

    /*!
     * \brief 设置窗口标题
     * \param caption 标题内容
     */
    virtual void SetWindowCaption(const CStringW& caption);
    
    // ---- 供派生窗口使用的编辑器模型视图 ----
    /*!
     * \brief 获取当前可编辑的资源信息
     * \return 指向当前可编辑资源信息的共享指针（可能为 nullptr）
     */
    std::shared_ptr<rceditor::SSingleResourceInfo> GetEditableResource() const { return m_pRes; }

    /*!
     * \brief 获取当前等级（1-8）
     * \return 当前选中的等级
     */
    int GetCurrentLevel() const { return m_curLevel; }

public:
    /*!
     * \brief 获取已打开资源的引用
     * \return 已打开资源的 const 引用（键为文件路径）
     */
    const std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>& GetOpenedResources() const { return m_openedResources; }

protected:

    // 等级界面钩子（派生类可覆盖）
    /*!
     * \brief 创建等级管理器相关 UI 控件（复选框、等级下拉框等）
     */
    virtual void CreateLevelUiControls();
    /*!
     * \brief 切换等级管理器UI控件的可见性
     * \param visible 是否显示
     */
    virtual void OnLevelUiVisibilityChanged(bool visible);
    /*!
     * \brief 布局等级管理器相关UI控件
     * \param rcCheck 全选复选框区域
     * \param rcCombo 等级下拉框区域
     */
    virtual void LayoutLevelUiControls(const CRect& rcCheck, const CRect& rcCombo);
    /*!
     * \brief 处理全选复选框点击事件（派生类可重写）
     */
    virtual void HandleCheckAllClicked();
    /*!
     * \brief 处理等级下拉框选择变化事件（派生类可重写）
     */
    virtual void HandleBadgeSelChange();
    /*!
     * \brief 窗口大小变化事件
     */
    afx_msg void OnSize(UINT nType, int cx, int cy);

    /*!
     * \brief 擦除背景事件
     */
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    /*!
     * \brief 销毁窗口事件
     */
    afx_msg void OnDestroy();

    /*!
     * \brief 获取窗口最小/最大尺寸事件
     */
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    /*!
     * \brief 属性网格属性变更处理
     */
    afx_msg LRESULT OnPropertyChanged(WPARAM wParam, LPARAM lParam);
    /*!
     * \brief 树控件选择变化事件
     */
    afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 树控件项变化事件
     */
    afx_msg void OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 树控件点击事件
     */
    afx_msg void OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

protected:
	struct TreeNodeData
	{
        enum { ToolbarCategoryId = 0xF002 }; // 区分工具栏分类与其他分类
		// 节点种类，便于扩展不同资源类型（文件根、分类、对话框、控件、菜单、工具栏等）
        enum NodeKind { NK_FileRoot = 0, NK_Category = 1, NK_Dialog = 2, NK_Control = 3, NK_Menu = 4, NK_Toolbar = 5, NK_ToolbarItem = 6, NK_DynamicWnd = 7, NK_DynamicItem = 8, NK_Other = 100 } kind;
		// 对于 Dialog 节点：dlgId 保存对话框 ID；对于 Menu/Toolbar 节点：dlgId 保存对应资源 ID。
		UINT dlgId;
		// 对于 Control 节点：ctrlId 保存控件 ID；对于非控件节点使用 UINT_MAX。
		UINT ctrlId; // UINT_MAX => file root / category / non-control
		CString filePath; 	//! 所属文件的key
	};

	enum ECtrlsID
	{
		IDC_TOOLBAR_RCED = 5000,
		IDC_TREE_RCED = 5001,
		IDC_SURFACE_RCED = 5002,
		IDC_PROPGRID_RCED = 5003,
		IDC_EDIT_SEARCH_RCED = 5004,
		IDC_BTN_SEARCH_RCED = 5005,
	IDC_CHECK_ALL_RCED = 5006,
		IDC_COMBO_BADGE_RCED = 5007,
		IDC_PROPLIST_RCED = 5008,  //!< 属性列表控件ID（用于替代/补充属性网格）
		IDC_MENU_BASE_RCED = 6000,
	};

	enum ECmdID
	{
		ID_RCED_FILE_OPEN = IDC_MENU_BASE_RCED + 1,
		ID_RCED_FILE_SAVE = IDC_MENU_BASE_RCED + 2,
		ID_RCED_FILE_SAVE_AS = IDC_MENU_BASE_RCED + 3,
		ID_RCED_FILE_EXIT = IDC_MENU_BASE_RCED + 4,
	};

protected:

    // 核心 UI 资源，供派生类使用
    CFont m_font;
    CToolBarCtrl m_toolbar; //!< 工具栏控件
    CTreeCtrl m_tree; //!< 树视图
    CEdit m_editSearch;
    ParameterEditorStyle::CButton m_btnSearch;
    // 等级管理器 UI 使用的控件
    CButton m_checkAll;
    CComboBox m_comboBadge;
    // 设计面板和属性列表
    rceditor::CRcDesignSurfaceWnd* m_surface = nullptr;
    CPropertyListCtrl m_propList;  //!< 属性列表控件

    // 菜单对象
    CMenu m_menu;
    CMenu m_menuFile;
    CMenu m_menuEdit;

    CString m_currentIniPath;
    bool m_initialized = false;
    bool m_inPropUpdate = false;
    bool m_inTreeUpdate = false;
    bool m_inBadgeUpdate = false;
    bool m_closing = false;
    int m_curLevel = 1;
    // When true, UpdateBadgeUiFromSelection will not change the check state of m_checkAll.
    // Used to preserve the Select-All visual state during property-list driven updates.
    bool m_preserveBadgeCheck = false;
    CString m_searchText;
    HTREEITEM m_lastSelectedTreeItem = nullptr;

    // 为 true 时：树显示复选框，主 UI 显示用于等级管理的复选框/下拉选择
    bool m_showLevelUi = false;
    
    // 搜索模式标志：为 true 时表示当前显示的是搜索结果树，而非原型树
    // 仅通过搜索/清除按钮切换，其它操作（如勾选复选框、全选等）不会改变此标志
    bool m_isSearchMode = false;

    // When in search mode, store matched control indices for the currently selected dialog
    // to support keyboard navigation (up/left, down/right) between matches.
    std::vector<int> m_searchMatchedIndices;
    UINT m_searchMatchedDlgId = 0;

    std::shared_ptr<rceditor::PEFileEditor> m_pe;
    std::shared_ptr<rceditor::SSingleResourceInfo> m_pRes;
    // 当以类似工具栏的预览显示动态窗口时，记录其对话框模型
    std::shared_ptr<rceditor::SDialogInfo> m_activeDynamicDialog;
    CString m_activeDynamicFilePath;
    std::vector<std::unique_ptr<TreeNodeData>> m_treeNodeData; // 为 CTreeCtrl 保持指针稳定


	//! 支持多文件打开 - CString 为文件路径
    std::map<CString, std::shared_ptr<rceditor::PEFileEditor>> m_openedEditors;
    std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>> m_openedResources;

protected:
	bool OpenFile(const CString& path);        // 打开（或复用）并设为活动文件
	void SetActiveFile(const CString& path);   // 切换活动文件（更新 UI / surface / tree）
	void CloseFile(const CString& path);       // 可选：关闭并移除资源
	CString GetActiveFilePath() const { return m_currentIniPath; }

    // 向派生类暴露一些布局和更新帮助函数（由 CFormUIDesigner 提供）
    void CreateControls(); // 创建 UI 控件
    virtual void LayoutControls(); // 布局控件
    virtual bool UseParameterEditorStyle() const { return false; }

    void CreateMenus(); // 创建菜单
    void CreateToolbar(); // 创建工具栏
    void UpdateToolbarLayout(); // 更新工具栏布局（占位）

    // 主 Tab 放在文档设定分组下，子窗口挂到所属 Tab；搜索时按需补齐父节点。
    HTREEITEM GetLevelAuditGroup(HTREEITEM root, const CString& filePath);
    CString GetLevelAuditDialogText(const CString& filePath, const rceditor::SDialogInfo& dlg) const;
    HTREEITEM InsertDialogTreeItem(HTREEITEM root, HTREEITEM defaultParent,
        const CString& filePath, const rceditor::SDialogInfo& dlg);
    HTREEITEM GetLevelAuditParent(HTREEITEM root, HTREEITEM defaultParent,
        const CString& filePath, const rceditor::SDialogInfo& dlg);
    HTREEITEM FindTreeItemForNode(int kind, UINT id, const CString& filePath = CString()) const;
    virtual void RebuildTree(); // 重建原型树结构（等级管理器覆盖业务分类）
    virtual void SyncLevelTreeParentChecks() {} // 在模型勾选同步后更新业务父节点
    virtual void RebuildSearchTree(); // 重建搜索结果树（派生类可覆盖以实现自定义搜索逻辑）
    void RefreshCurrentTree(); // 根据当前模式刷新树（搜索模式刷新搜索树，否则刷新原型树）
    void UpdatePropertyGrid(); // 更新属性面板
    virtual void UpdatePropertyList(); // 更新属性列表控件
    virtual void OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item); // 属性列表变更回调
    virtual void UpdateBadgeUiFromSelection(); // 根据选择更新 Badge UI
    // 为搜索扩展提供钩子：向临时树（root下）追加菜单/工具栏匹配项
    bool AppendSearchMatchesForResource(const std::shared_ptr<rceditor::SSingleResourceInfo>& res,
        const CString& filePath, const CString& searchLower, HTREEITEM hRoot);
    int CalcComboBestWidth(CComboBox& combo) const; // 计算 combo 最佳宽度
    int CalcComboDroppedHeight(CComboBox& combo, int visibleItems) const; // 计算下拉高度
    void UpdateBadgeComboMetrics(); // 更新 badge combo 的尺寸/下拉高度

private:
    // （重建树时不保存视觉状态；保持逻辑简单）

    // 重建多选的属性网格（显示共享属性，例如等级掩码）
    void RebuildPropertyGridForSelection();
    HTREEITEM FindTreeItemForDialog(UINT dlgId, const CString& filePath = CString()) const;
	HTREEITEM FindTreeItemForControl(UINT dlgId, UINT ctrlId, const CString& filePath = CString()) const;
	void SyncTreeSelectionFromSurface();

    // 暴露给实现层的工具函数
    static bool FuzzyMatchNoCase(const CString& haystack, const CString& needle);

    bool PickOpenFile(CString& outPath) const;
    bool PickSaveFile(CString& outPath) const;

    static bool TryParseDword(const CString& s, DWORD& outValue);

private:
    // 命令处理函数
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	afx_msg void OnFileExit();

	afx_msg void OnSearchChanged();
	virtual void OnSearchTextChanged();
	afx_msg void OnSearchClicked();
	afx_msg void OnCheckAllClicked();
	afx_msg void OnBadgeSelChange();
	afx_msg LRESULT OnDesignSurfaceNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPostSyncTreeChecks(WPARAM wParam, LPARAM lParam);
};
