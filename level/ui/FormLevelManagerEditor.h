#pragma once

#include "FormRcEditor.h"

class CFormRcEditor;

class CFormLevelManagerEditor : public CFormRcEditor
{
    DECLARE_DYNAMIC(CFormLevelManagerEditor)

public:
    explicit CFormLevelManagerEditor(CWnd* pParent = nullptr);
    virtual ~CFormLevelManagerEditor();

    /*!
     * \brief 设置初始文件列表（PE资源和JSON配置）
     * \param binPaths 需要打开的PE文件路径列表
     * \param jsonPath 可选JSON配置文件路径
     */
    void SetInitialFiles(const std::vector<CString>& binPaths, const CString& jsonPath = CString());

    /*!
     * \brief 设置已打开资源的共享指针（直接共享外部 m_openedResources）
     * \param pOpenedResources 指向已打开资源映射的指针
     * \note 如果设置了此指针，将直接使用外部资源，而不是通过 OpenFile 加载
     */
    void SetOpenedResources(std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>* pOpenedResources,
        const CString& loadedJsonPath = CString());
    const CString& GetLoadedJsonPath() const { return m_loadedJsonPath; }

    // 获取当前（可能已修改的）打开资源映射，供外部同步
    const std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>& GetOpenedResources() const { return m_openedResources; }

protected:
    /*! \brief 获取窗口初始标题 */
    CStringW GetInitialCaption() const override;
    void SetWindowCaption(const CStringW&) override { CFormRcEditor::SetWindowCaption(GetInitialCaption()); }
    /*! \brief 控件创建后初始化等级管理器相关UI和菜单 */
    void OnAfterControlsCreated() override;
    int GetLevelDisplayNumber(int levelIndex) const override { return levelIndex; }
    bool UseParameterEditorStyle() const override { return true; }
    bool ShowParameterCloseButton() const override { return false; }
    void LayoutControls() override;
    /*! \brief 对话框关闭前的处理（如保存JSON、关闭等级管理模式） */
    void OnBeforeDialogClose() override;

    // Level UI overrides
    /*! \brief 创建等级管理器相关的UI控件（全选复选框、等级下拉框） */
    void CreateLevelUiControls() override;
    /*!
     * \brief 切换等级管理器UI控件的可见性
     * \param visible 是否显示
     */
    void OnLevelUiVisibilityChanged(bool visible) override;
    /*!
     * \brief 布局等级管理器相关UI控件
     * \param rcCheck 全选复选框区域
     * \param rcCombo 等级下拉框区域
     */
    void LayoutLevelUiControls(const CRect& rcCheck, const CRect& rcCombo) override;
    /*! \brief 处理全选复选框点击事件 */
    void HandleCheckAllClicked() override;
    /*! \brief 处理等级下拉框选择变化事件 */
    void HandleBadgeSelChange() override;

    /*! \brief 重建搜索结果树（覆盖基类实现，提供自定义搜索逻辑） */
    void RebuildTree() override;
    void RebuildSearchTree() override;
    void SyncLevelTreeParentChecks() override;
    bool GetDialogTreeCheckTargets(const TreeNodeData& data,
        std::vector<std::shared_ptr<rceditor::SDialogInfo>>& targets) const;
    bool SetDialogTreeChecked(const TreeNodeData& data, bool checked);
    bool ToggleDialogTreeChecked(HTREEITEM item);
    bool GetToolbarTreeCheckTargets(const TreeNodeData& data,
        std::vector<std::shared_ptr<rceditor::SToolbarInfo>>& targets) const;
    bool SetToolbarTreeChecked(const TreeNodeData& data, bool checked);
    bool ToggleToolbarTreeChecked(HTREEITEM item);
    void NormalizeExemptToolbarItems();

    /*!
     * \brief 属性列表变更处理，标记数据为已修改
     * \param nIndex 变更的属性索引
     * \param item 属性数据
     */
    void OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item);

    /*!
     * \brief 属性网格属性变更处理，仅允许等级相关属性修改
     * \param wParam 未使用
     * \param lParam 属性指针
     * \return 处理结果
     */
    LRESULT OnPropertyChanged(WPARAM wParam, LPARAM lParam);
    /*!
     * \brief 树控件点击事件处理，标记数据为已修改
     * \param pNMHDR 通知消息头
     * \param pResult 结果指针
     */
    void OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult);
    /*!
     * \brief 树控件选择变化事件处理，支持搜索结果控件的自动选中
     * \param pNMHDR 通知消息头
     * \param pResult 结果指针
     */
    afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);
    /*! \brief 树控件项状态变化处理（文档树联动及非管理员勾选恢复） */
    afx_msg void OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

    // Keyboard handling for navigating matched controls in search mode
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

private:
    ParameterEditorStyle::CButton m_openResourceButton;
    ParameterEditorStyle::CButton m_openJsonButton;
    ParameterEditorStyle::CButton m_saveJsonButton;
    ParameterEditorStyle::CButton m_saveAsButton;
    ParameterEditorStyle::CButton m_closeButton;
    ParameterEditorStyle::CHeader m_styledPropertyHeader;
    CStatic m_levelLabel;
    CStatic m_fileStatus;
    CString m_jsonPath;
    CString m_loadedJsonPath;
    bool m_savedToTarget = false;
    void RefreshFileDisplay();
    bool m_dirty = false; //!< 数据是否已修改
    enum { ID_LM_OPEN_JSON = 41000, ID_LM_SAVE_JSON = 41001 };

    /*!
     * \brief 弹出文件选择对话框，选择要打开的JSON文件
     * \param outPath 输出选中的文件路径
     * \return 是否成功选择文件
     */
    bool PickJsonOpenFile(CString& outPath) const;
    /*!
     * \brief 弹出文件保存对话框，选择保存JSON的路径
     * \param outPath 输出选中的保存路径
     * \return 是否成功选择路径
     */
    bool PickJsonSaveFile(CString& outPath) const;

    /*!
     * \brief 加载等级管理器JSON配置
     * \param jsonPath JSON文件路径
     * \return 加载是否成功
     */
    bool LoadLevelConfigJson(const CString& jsonPath);
    /*!
     * \brief 保存当前等级管理器配置到JSON文件
     * \param jsonPath 目标JSON文件路径
     * \return 保存是否成功
     */
    bool SaveLevelConfigJson(const CString& jsonPath);
    /*! \brief 标记当前数据为已修改（脏） */
    void MarkDirty();
    /*!
     * \brief 如果有未保存的修改则保存JSON
     * \param forcePickPath 是否强制弹出保存路径选择
     * \return 保存是否成功
     */
    bool SaveIfDirty(bool forcePickPath);

    /*! \brief 打开等级管理器JSON文件 */
    afx_msg void OnFileOpenLevelJson();
    /*! \brief 另存为等级管理器JSON文件 */
    afx_msg void OnFileSaveAsLevelJson();
    /*! \brief 保存等级管理器JSON文件（如有路径则直接保存） */
    afx_msg void OnFileSaveJson();
};
