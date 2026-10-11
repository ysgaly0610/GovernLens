#pragma once

#include <afxwin.h>
#include "ParameterEditorStyle.h"

namespace ParamRange {

/*!
 * \brief RC 编辑器对话框基类
 * \details 提供对话模板缓冲区管理和基本生命周期钩子，供派生窗口使用
 */
class /*PARAMMANAGER_API*/ CFormNoRcBase : public CDialog
{
    DECLARE_DYNAMIC(CFormNoRcBase)
public:
    /*! \brief 构造函数
     *  \param pParent 父窗口指针，可为 nullptr
     */
    explicit CFormNoRcBase(CWnd* pParent = nullptr);

    /*! \brief 析构函数，释放内部缓冲区 */
    virtual ~CFormNoRcBase();

protected:
    void EnableParameterEditorStyle();
    virtual bool ShowParameterCloseButton() const { return true; }
    static CStringW NormalizeJsonDisplayPath(const CString& path);
    bool HasParameterEditorStyle() const { return m_parameterStyle; }
    int StylePx(int value) { return ParameterEditorStyle::Scale(this, value); }
    int ParameterCaptionHeight() { return m_parameterStyle ? StylePx(34) : 0; }
    void SetParameterToolbarRect(const CRect& rect);
    void SetParameterPathTooltip(CWnd& control, const CString& text);
    virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;

private:
    bool m_parameterStyle = false;
    CRect m_parameterToolbar = CRect(0, 0, 0, 0);
    CBrush m_parameterBackground;
    CBrush m_parameterBandBrush;
    CFont m_parameterCaptionFont;
    CToolTipCtrl m_parameterPathTips;
    void UpdateParameterCaptionTooltip();
    ParameterEditorStyle::CButton m_parameterClose;
    void PaintParameterBackground(CDC& dc);
    void LayoutParameterCaption();
    enum { ID_PARAMETER_CLOSE = 0x7E10 };

protected:
    /*! \brief 派生类可覆盖：返回窗口初始标题 */
    virtual CStringW GetInitialCaption() const { return L"RC Editor"; }
    /*! \brief 派生类可覆盖：在控件创建完成后被调用 */
    virtual void OnAfterControlsCreated() {}
    /*! \brief 派生类可覆盖：在对话框关闭前被调用 */
    virtual void OnBeforeDialogClose() {}

    /*! \brief 对话模板缓冲区（用于运行时构建对话模板） */
    WORD* m_pWndBuf = nullptr;
    /*! \brief 缓冲区最大长度（以 WORD 计） */
    static const int knMaxBufferSize = 10240;  //! 对话模板缓冲区最大长度（以 WORD 计）

    /*! \brief 在内部缓冲区中构造一个 DLGTEMPLATE 并返回其指针
     *  \param dwStyle 对话框 Style
     *  \param rect 对话框位置与尺寸
     *  \param caption 对话框标题
     *  \param dwStyleEx 扩展样式（可选）
     *  \return 指向内部缓冲区的 DLGTEMPLATE 指针
     */
    DLGTEMPLATE* CreateTemplate(DWORD dwStyle, CRect& rect, CStringW caption, DWORD dwStyleEx = 0);
};

} // namespace ParamRange
