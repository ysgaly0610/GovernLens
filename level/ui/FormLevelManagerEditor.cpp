#include "stdafx.h"
#include "FormLevelManagerEditor.h"

#include <CommCtrl.h>

#include "../../utils/JsonUtils.h"
#include <algorithm>
#include <functional>
#include "LevelLocalize.h"
#include "LevelAuditIgnore.h"

IMPLEMENT_DYNAMIC(CFormLevelManagerEditor, CFormRcEditor)

#ifndef WM_APP_DESIGNSURFACE
#define WM_APP_DESIGNSURFACE (WM_APP + 210)
#endif
#ifndef WM_APP_SYNC_TREE_CHECKS
#define WM_APP_SYNC_TREE_CHECKS (WM_APP + 211)
#endif

// 辅助：在静态对话框映射或动态对话框映射中查找对话框
static std::shared_ptr<rceditor::SDialogInfo> FindDialogInResource(const std::shared_ptr<rceditor::SSingleResourceInfo>& res, UINT dlgId)
{
    if (!res) return nullptr;
    auto it = res->mapLangAndDialogInfo.find(dlgId);
    if (it != res->mapLangAndDialogInfo.end()) return it->second;
    auto it2 = res->mapDynamicDialogInfo.find(dlgId);
    if (it2 != res->mapDynamicDialogInfo.end()) return it2->second;
    return nullptr;
}

BEGIN_MESSAGE_MAP(CFormLevelManagerEditor, CFormRcEditor)
	ON_NOTIFY(TVN_SELCHANGED, 5001 /*IDC_TREE_RCED*/, &CFormLevelManagerEditor::OnTvnSelchangedTree)
	ON_NOTIFY(TVN_ITEMCHANGED, 5001 /*IDC_TREE_RCED*/, &CFormLevelManagerEditor::OnTvnItemchangedTree)
	ON_COMMAND(ID_LM_OPEN_JSON, &CFormLevelManagerEditor::OnFileOpenLevelJson)
	ON_COMMAND(ID_LM_SAVE_JSON, &CFormLevelManagerEditor::OnFileSaveJson)
    ON_COMMAND(ID_RCED_FILE_SAVE, &CFormLevelManagerEditor::OnFileSaveJson)
    ON_COMMAND(ID_RCED_FILE_SAVE_AS, &CFormLevelManagerEditor::OnFileSaveAsLevelJson)
	//ON_REGISTERED_MESSAGE(AFX_WM_PROPERTY_CHANGED, &CFormLevelManagerEditor::OnPropertyChanged)
	ON_NOTIFY(NM_CLICK, 5001 /*IDC_TREE_RCED*/, &CFormLevelManagerEditor::OnNMClickTree)
END_MESSAGE_MAP()

CFormLevelManagerEditor::CFormLevelManagerEditor(CWnd* pParent)
	: CFormRcEditor(pParent)
{
}

BOOL CFormLevelManagerEditor::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_SPACE &&
        pMsg->hwnd == m_tree.GetSafeHwnd() && !m_closing && !m_inTreeUpdate &&
        (ToggleDialogTreeChecked(m_tree.GetSelectedItem()) ||
         ToggleToolbarTreeChecked(m_tree.GetSelectedItem())))
        return TRUE;
    // Handle arrow keys in search mode to navigate matched controls
    if (m_isSearchMode && pMsg && pMsg->message == WM_KEYDOWN && m_surface)
    {
        int vk = (int)pMsg->wParam;
        if (vk == VK_UP || vk == VK_LEFT || vk == VK_DOWN || vk == VK_RIGHT)
        {
            if (m_searchMatchedIndices.empty())
                return CFormRcEditor::PreTranslateMessage(pMsg);
            // find currently selected index in matched list
            auto curSel = m_surface->GetSelectedCtrl();
            int curIdx = -1;
            if (curSel)
            {
                // determine selected control index within current dialog
                auto curDlg = m_surface->GetDialogInfo();
                if (curDlg && curDlg->nIDD == m_searchMatchedDlgId)
                {
                    for (int i = 0; i < (int)curDlg->vecCtrlInfo.size(); ++i)
                    {
                        auto c = curDlg->vecCtrlInfo[i];
                        if (!c) continue;
                        if (c->nIDC == curSel->nIDC)
                        {
                            // find this index in matched list
                            for (int j = 0; j < (int)m_searchMatchedIndices.size(); ++j)
                            {
                                if (m_searchMatchedIndices[j] == i)
                                {
                                    curIdx = j; break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            // if no current selection, treat as -1
            int next = curIdx;
            if (vk == VK_UP || vk == VK_LEFT)
                next = (curIdx <= 0) ? ((int)m_searchMatchedIndices.size() - 1) : (curIdx - 1);
            else
                next = (curIdx < 0 || curIdx + 1 >= (int)m_searchMatchedIndices.size()) ? 0 : (curIdx + 1);

            // switch to dialog and select control by index
            // find resource/dlg for m_searchMatchedDlgId
            std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = m_pRes;
            for (const auto& kv : m_openedResources)
            {
                auto r = kv.second;
                if (r && (r->mapLangAndDialogInfo.count(m_searchMatchedDlgId) || r->mapDynamicDialogInfo.count(m_searchMatchedDlgId)))
                {
                    resPtr = r; break;
                }
            }
            auto dlg = FindDialogInResource(resPtr, m_searchMatchedDlgId);
            if (dlg)
            {
                int controlIdx = m_searchMatchedIndices[next];
                m_surface->SetDialogInfo(dlg);
                m_surface->SetCurrentLevel(m_curLevel);
                m_surface->EnsureControlVisibleByIndex(controlIdx);
                m_surface->SelectControlByIndex(controlIdx, false, false);
                UpdatePropertyGrid();
                UpdateBadgeUiFromSelection();
                return TRUE; // consumed
            }
        }
    }
    return CFormRcEditor::PreTranslateMessage(pMsg);
}

// Helper to map dialog id + index to control nIDC. In search mode we encoded index-based selection as index.
static UINT dlgCtrlIdFromIndex(UINT dlgId, int idx)
{
    UNREFERENCED_PARAMETER(dlgId);
    // caller will compare by control index against selected control's nIDC elsewhere; return 0 here.
    return 0u;
}

// 使用完整资源模型确定业务子树，搜索过滤或折叠不改变整组选取的范围。
bool CFormLevelManagerEditor::GetDialogTreeCheckTargets(const TreeNodeData& data,
    std::vector<std::shared_ptr<rceditor::SDialogInfo>>& targets) const
{
    targets.clear();
    if (!m_showLevelUi) return false;
    const bool group = data.kind == TreeNodeData::NK_Category &&
        (data.dlgId == LevelGroupDocument || data.dlgId == LevelGroupAdvanced ||
         data.dlgId == LevelGroupLight || data.dlgId == LevelGroupColor ||
          data.dlgId == LevelGroupDebugPages);
    if (group)
    {
        for (const auto& resource : m_openedResources)
        {
            if (!resource.second) continue;
            auto collect = [&](const std::map<UINT, std::shared_ptr<rceditor::SDialogInfo>>& dialogs) {
                for (const auto& entry : dialogs)
                    if (entry.second && !ShouldIgnoreLevelAuditDialog(resource.first, *entry.second) &&
                        GetLevelAuditDialogGroup(resource.first, *entry.second) == data.dlgId)
                        targets.push_back(entry.second);
            };
            collect(resource.second->mapLangAndDialogInfo);
            collect(resource.second->mapDynamicDialogInfo);
        }
        return true;
    }
    const bool imageAvi = IsLevelAuditModule(data.filePath, _T("ImageAVI"));
    const bool dialog = data.kind == TreeNodeData::NK_Dialog;
    const bool dynamic = data.kind == TreeNodeData::NK_DynamicWnd;
    if (!dialog && !dynamic) return false;
    auto resource = m_openedResources.find(data.filePath);
    if (resource == m_openedResources.end() || !resource->second) return true;
    const auto& res = *resource->second;
    const bool docPage = imageAvi && dialog && GetLevelAuditDocPage(data.dlgId);
    if (docPage)
    {
        for (const auto& entry : res.mapLangAndDialogInfo)
        {
            const auto& dlg = entry.second;
            if (!dlg || dlg->m_blIsDynamic || ShouldIgnoreLevelAuditDialog(data.filePath, *dlg))
                continue;
            const auto info = GetLevelAuditDocPage(dlg->nIDD);
            if (info && (info->id == data.dlgId || info->parentId == data.dlgId))
                targets.push_back(dlg);
        }
    }
    else if (dialog)
    {
        auto entry = res.mapLangAndDialogInfo.find(data.dlgId);
        if (entry != res.mapLangAndDialogInfo.end() && entry->second &&
            !ShouldIgnoreLevelAuditDialog(data.filePath, *entry->second))
            targets.push_back(entry->second);
    }
    if (dynamic)
    {
        auto entry = res.mapDynamicDialogInfo.find(data.dlgId);
        if (entry != res.mapDynamicDialogInfo.end() && entry->second &&
            !ShouldIgnoreLevelAuditDialog(data.filePath, *entry->second))
            targets.push_back(entry->second);
    }
    return true;
}

bool CFormLevelManagerEditor::SetDialogTreeChecked(const TreeNodeData& data, bool checked)
{
    if (!rceditor::gbIsLevelAdmin)
        return false;
    std::vector<std::shared_ptr<rceditor::SDialogInfo>> targets;
    if (!GetDialogTreeCheckTargets(data, targets))
        return false;
    // 内部 m_curLevel 仍为 1..8；界面显示与用户等级一致的 0..7。
    const int userLevel = (std::max)(0, (std::min)(7, m_curLevel - 1));
    const UINT allowedMask = 0xFFu & ~((1u << (userLevel + 1)) - 1u);
    bool changed = false;
    for (const auto& dlg : targets)
    {
        if (dlg->m_blIsInLevelManager != checked)
        {
            dlg->m_blIsInLevelManager = checked;
            changed = true;
        }
        if (checked)
        {
            // 勾选：当前等级及以下禁止，更高等级允许；重设范围以消除旧的低等级授权。
            for (const auto& ctrl : dlg->vecCtrlInfo)
                if (ctrl && ctrl->uiLevelMask != allowedMask)
                {
                    ctrl->uiLevelMask = allowedMask;
                    changed = true;
                }
        }
        // 取消：退出等级管理，保留权限数据；再次勾选时按当时选择的等级重设。
    }
    changed = SetToolbarTreeChecked(data, checked) || changed;
    return changed;
}

bool CFormLevelManagerEditor::ToggleDialogTreeChecked(HTREEITEM item)
{
    if (!item) return false;
    auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
    std::vector<std::shared_ptr<rceditor::SDialogInfo>> targets;
    if (!data || !GetDialogTreeCheckTargets(*data, targets))
        return false;
    // 点击复选框不会自动选择该行，先通过选择通知同步高亮和右侧预览。
    const bool checked = m_tree.GetCheck(item) == FALSE;
    if (m_tree.GetSelectedItem() != item)
        m_tree.SelectItem(item);
    // NM_CLICK 在默认勾选之前到达，先更新模型，默认处理结束后再统一刷新。
    if (SetDialogTreeChecked(*data, checked))
    {
        if (m_surface) m_surface->Invalidate(FALSE);
        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
    }
    PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
    return true;
}
bool CFormLevelManagerEditor::GetToolbarTreeCheckTargets(const TreeNodeData& data,
    std::vector<std::shared_ptr<rceditor::SToolbarInfo>>& targets) const
{
    targets.clear();
    if (!m_showLevelUi) return false;
    const bool group = data.kind == TreeNodeData::NK_Category &&
        (data.dlgId == LevelGroupToolbars || data.dlgId == LevelGroupDebugPages);
    const bool page = data.kind == TreeNodeData::NK_DynamicWnd &&
        IsLevelAuditModule(data.filePath, _T("ImageAVI")) && DebugPageLevel::PageOrder(data.dlgId) >= 0;
    if (!group && !page && data.kind != TreeNodeData::NK_Toolbar) return false;
    for (const auto& resource : m_openedResources)
    {
        if (!resource.second || (!group && resource.first != data.filePath)) continue;
        for (const auto& entry : resource.second->mapLangAndToolbarInfo)
        {
            if (!entry.second || ShouldIgnoreLevelAuditToolbar(resource.first, entry.first)) continue;
            UINT owner = GetLevelAuditToolbarPage(resource.first, entry.first);
            if ((group && ((data.dlgId == LevelGroupDebugPages) == (owner != 0))) ||
                (page && owner == data.dlgId) || (!group && !page && entry.first == data.dlgId))
                targets.push_back(entry.second);
        }
    }
    return true;
}

bool CFormLevelManagerEditor::SetToolbarTreeChecked(const TreeNodeData& data, bool checked)
{
    if (!rceditor::gbIsLevelAdmin) return false;
    std::vector<std::shared_ptr<rceditor::SToolbarInfo>> targets;
    if (!GetToolbarTreeCheckTargets(data, targets)) return false;
    // 与文档树一致：勾选时禁止当前等级及以下，允许更高等级。
    const int userLevel = (std::max)(0, (std::min)(7, m_curLevel - 1));
    const UINT allowedMask = 0xFFu & ~((1u << (userLevel + 1)) - 1u);
    bool changed = false;
    for (const auto& tb : targets)
    {
        if (tb->m_blIsInLevelManager != checked)
        {
            tb->m_blIsInLevelManager = checked;
            changed = true;
        }
        if (checked)
        {
            std::vector<UINT> masks(tb->vecItemIDs.size(), allowedMask);
            for (const auto& resource : m_openedResources)
            {
                if (!resource.second) continue;
                auto entry = resource.second->mapLangAndToolbarInfo.find(tb->nToolbarID);
                if (entry == resource.second->mapLangAndToolbarInfo.end() || entry->second != tb) continue;
                for (size_t i = 0; i < tb->vecItemIDs.size(); ++i)
                    if (IsLevelExemptToolbarItem(resource.first, tb->nToolbarID, tb->vecItemIDs[i]))
                        masks[i] = 0xFFu;
            }
            if (tb->vecLevelMask != masks)
            {
                tb->vecLevelMask = masks;
                changed = true;
            }
        }
        // 取消仅退出管理，保留既有等级数据。
    }
    return changed;
}

bool CFormLevelManagerEditor::ToggleToolbarTreeChecked(HTREEITEM item)
{
    if (!item) return false;
    auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
    std::vector<std::shared_ptr<rceditor::SToolbarInfo>> targets;
    if (!data || !GetToolbarTreeCheckTargets(*data, targets)) return false;
    const bool checked = m_tree.GetCheck(item) == FALSE;
    if (m_tree.GetSelectedItem() != item)
        m_tree.SelectItem(item);
    if (SetToolbarTreeChecked(*data, checked))
    {
        if (m_surface) m_surface->Invalidate(FALSE);
        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
    }
    // 等待默认勾选结束后再同步父子显示，兼容不发送 TVN_ITEMCHANGED 的环境。
    PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
    return true;
}

// 保留原始索引及其它按钮的配置，仅修正登录的历史权限。
void CFormLevelManagerEditor::NormalizeExemptToolbarItems()
{
    for (const auto& resource : m_openedResources)
    {
        if (!resource.second) continue;
        for (const auto& entry : resource.second->mapDynamicDialogInfo)
            if (entry.second) RemoveLevelAuditNavigation(resource.first, *entry.second);
        for (const auto& entry : resource.second->mapLangAndToolbarInfo)
        {
            const auto& tb = entry.second;
            if (!tb) continue;
            for (size_t i = 0; i < tb->vecItemIDs.size(); ++i)
                if (IsLevelExemptToolbarItem(resource.first, tb->nToolbarID, tb->vecItemIDs[i]))
                {
                    // 缺失掩码在运行时默认允许，补齐时保留其它按钮的这一行为。
                    if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
                        tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0xFFu);
                    tb->vecLevelMask[i] = 0xFFu;
                }
        }
    }
}

void CFormLevelManagerEditor::SyncLevelTreeParentChecks()
{
    std::function<void(HTREEITEM)> sync = [&](HTREEITEM first)
    {
        for (HTREEITEM item = first; item; item = m_tree.GetNextSiblingItem(item))
        {
            auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
            std::vector<std::shared_ptr<rceditor::SDialogInfo>> targets;
            std::vector<std::shared_ptr<rceditor::SToolbarInfo>> toolbars;
            const bool dialogsHandled = data && GetDialogTreeCheckTargets(*data, targets);
            const bool toolbarsHandled = data && GetToolbarTreeCheckTargets(*data, toolbars);
            if (dialogsHandled || toolbarsHandled)
            {
                const bool all = (!targets.empty() || !toolbars.empty()) &&
                    std::all_of(targets.begin(), targets.end(), [](const auto& dlg) { return dlg->m_blIsInLevelManager; }) &&
                    std::all_of(toolbars.begin(), toolbars.end(), [](const auto& tb) { return tb->m_blIsInLevelManager; });
                m_tree.SetCheck(item, all ? TRUE : FALSE);
            }
            sync(m_tree.GetChildItem(item));
        }
    };
    sync(m_tree.GetRootItem());
}

void CFormLevelManagerEditor::OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (pResult) *pResult = 0;
    if (m_closing || m_inTreeUpdate)
        return;
    NMTVITEMCHANGE* pChange = reinterpret_cast<NMTVITEMCHANGE*>(pNMHDR);
    if (!pChange)
    {
        if (pResult) *pResult = 0;
        return;
    }

    // 只有状态位发生变化时才处理
    if (!(pChange->uChanged & TVIF_STATE))
    {
        if (pResult) *pResult = 0;
        return;
    }

    HTREEITEM hItem = reinterpret_cast<HTREEITEM>(pChange->hItem);
    if (!hItem)
    {
        if (pResult) *pResult = 0;
        return;
    }

    // 窗口与工具栏树由输入入口修改模型；通知仅恢复模型对应的显示。
    auto node = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
    std::vector<std::shared_ptr<rceditor::SDialogInfo>> targets;
    std::vector<std::shared_ptr<rceditor::SToolbarInfo>> toolbars;
    if (node && (GetDialogTreeCheckTargets(*node, targets) || GetToolbarTreeCheckTargets(*node, toolbars)))
    {
        if ((pChange->uStateOld & TVIS_STATEIMAGEMASK) !=
            (pChange->uStateNew & TVIS_STATEIMAGEMASK))
            PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
        return;
    }
    // 非管理员：立即恢复为模型状态，防止短暂切换
    if (!rceditor::gbIsLevelAdmin)
    {
        TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
        if (!data)
        {
            if (pResult) *pResult = 0;
            return;
        }

        std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
        auto itRes = m_openedResources.find(data->filePath);
        if (itRes != m_openedResources.end())
            resPtr = itRes->second;
        if (!resPtr)
            resPtr = m_pRes;

        // 仅对 Dialog 和 Toolbar 节点显示/恢复复选框状态
        if (resPtr)
        {
            if (data->kind == TreeNodeData::NK_Dialog)
            {
                // static dialog: only consult static dialog map to reflect model state
                auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
                    m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_DynamicWnd)
            {
                // dynamic dialog: only consult dynamic dialog map
                auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
                if (itDlg != resPtr->mapDynamicDialogInfo.end() && itDlg->second)
                    m_tree.SetCheck(hItem, itDlg->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
            else if (data->kind == TreeNodeData::NK_Toolbar)
            {
                auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                    m_tree.SetCheck(hItem, itTb->second->m_blIsInLevelManager ? TRUE : FALSE);
            }
        }
    }

    
    if (pResult) *pResult = 0;
}

// 普通列表和搜索共用业务分类，资源身份与保存格式保持不变。
void CFormLevelManagerEditor::RebuildSearchTree()
{
    RebuildTree();
}

void CFormLevelManagerEditor::RebuildTree()
{
    EnsureToolHistogramControls(m_openedResources);
    if (!m_tree.GetSafeHwnd()) return;
    const bool hadTree = m_tree.GetRootItem() != nullptr;
    TreeNodeData selected = {};
    auto oldSelection = m_tree.GetSelectedItem();
    const bool hadSelection = oldSelection && m_tree.GetItemData(oldSelection);
    if (hadSelection) selected = *reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(oldSelection));
    std::vector<TreeNodeData> expanded;
    std::function<void(HTREEITEM)> remember = [&](HTREEITEM first) {
        for (auto item = first; item; item = m_tree.GetNextSiblingItem(item))
        {
            auto data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(item));
            if (data && (m_tree.GetItemState(item, TVIS_EXPANDED) & TVIS_EXPANDED))
                expanded.push_back(*data);
            remember(m_tree.GetChildItem(item));
        }
    };
    remember(m_tree.GetRootItem());
    m_searchMatchedIndices.clear();
    m_searchMatchedDlgId = 0;
    m_inTreeUpdate = true;
    m_tree.DeleteAllItems();
    m_treeNodeData.clear();
    m_lastSelectedTreeItem = nullptr;

    CString search = m_isSearchMode ? m_searchText : CString();
    search.MakeLower();
    auto matches = [&](CString text) { text.MakeLower(); return text.Find(search) >= 0; };
    auto insert = [&](TreeNodeData::NodeKind kind, UINT id, const CString& path,
        const CString& text, HTREEITEM parent, bool checked) {
        auto data = std::make_unique<TreeNodeData>();
        data->kind = kind; data->dlgId = id; data->ctrlId = 0; data->filePath = path;
        HTREEITEM item = m_tree.InsertItem(text, parent, TVI_LAST);
        m_tree.SetItemData(item, reinterpret_cast<DWORD_PTR>(data.get()));
        m_tree.SetCheck(item, checked ? TRUE : FALSE);
        m_treeNodeData.push_back(std::move(data));
        return item;
    };
    std::map<UINT, HTREEITEM> groups;
    auto getGroup = [&](UINT id) {
        auto found = groups.find(id);
        if (found != groups.end()) return found->second;
        HTREEITEM item = insert(TreeNodeData::NK_Category, id, CString(),
            ML(GetLevelAuditGroupText(id, gbCurNeedShowLan)), TVI_ROOT, false);
        if (id == LevelGroupOther) m_tree.SetItemState(item, 0, TVIS_STATEIMAGEMASK);
        groups[id] = item;
        return item;
    };
    std::function<HTREEITEM(const CString&, const rceditor::SDialogInfo&)> addDialog;
    addDialog = [&](const CString& path, const rceditor::SDialogInfo& dlg) -> HTREEITEM {
        auto kind = dlg.m_blIsDynamic ? TreeNodeData::NK_DynamicWnd : TreeNodeData::NK_Dialog;
        HTREEITEM existing = FindTreeItemForNode(kind, dlg.nIDD, path);
        if (existing) return existing;
        UINT group = GetLevelAuditDialogGroup(path, dlg);
        HTREEITEM parent = getGroup(group);
        // 文档页的嵌套子窗口保留原来的父页，搜索只命中子页时也补齐父页。
        const auto page = group == LevelGroupDocument && !dlg.m_blIsDynamic ?
            GetLevelAuditDocPage(dlg.nIDD) : nullptr;
        if (page && page->parentId)
        {
            const auto& res = m_openedResources.at(path);
            auto entry = res->mapLangAndDialogInfo.find(page->parentId);
            if (entry != res->mapLangAndDialogInfo.end() && entry->second &&
                !ShouldIgnoreLevelAuditDialog(path, *entry->second))
                parent = addDialog(path, *entry->second);
        }
        return insert(kind, dlg.nIDD, path, GetLevelAuditDialogText(path, dlg), parent,
            dlg.m_blIsInLevelManager);
    };
    struct DialogRow
    {
        CString path;
        std::shared_ptr<rceditor::SDialogInfo> dialog;
        UINT order;
    };
    HTREEITEM firstLeaf = nullptr;
    const UINT groupOrder[] = {LevelGroupDocument, LevelGroupAdvanced, LevelGroupLight,
        LevelGroupColor, LevelGroupDebugPages, LevelGroupToolbars, LevelGroupOther};
    for (UINT group : groupOrder)
    {
        const bool groupMatches = matches(ML(GetLevelAuditGroupText(group, gbCurNeedShowLan)));
        std::vector<DialogRow> rows;
        for (const auto& resource : m_openedResources)
        {
            const CString& path = resource.first;
            const auto& res = resource.second;
            if (!res) continue;
            auto collect = [&](const std::map<UINT, std::shared_ptr<rceditor::SDialogInfo>>& dialogs) {
                for (const auto& entry : dialogs)
                {
                    auto dlg = entry.second;
                    UINT order = 0;
                    if (!dlg || ShouldIgnoreLevelAuditDialog(path, *dlg) ||
                        GetLevelAuditDialogGroup(path, *dlg, &order) != group) continue;
                    CString id; id.Format(_T("%u"), dlg->nIDD);
                    bool match = groupMatches || matches(GetLevelAuditDialogText(path, *dlg)) || matches(id);
                    const auto page = group == LevelGroupDocument && !dlg->m_blIsDynamic ?
                        GetLevelAuditDocPage(dlg->nIDD) : nullptr;
                    if (page && page->parentId)
                    {
                        auto parent = res->mapLangAndDialogInfo.find(page->parentId);
                        if (parent != res->mapLangAndDialogInfo.end() && parent->second)
                            match = match || matches(GetLevelAuditDialogText(path, *parent->second));
                    }
                    for (const auto& ctrl : dlg->vecCtrlInfo)
                        if (ctrl && !IsLevelAuditControlHidden(*ctrl))
                            match = match || matches(CString(ctrl->csText)) || matches(CString(ctrl->csClassName));
                    if (match) rows.push_back({path, dlg, order});
                }
            };
            collect(res->mapLangAndDialogInfo);
            collect(res->mapDynamicDialogInfo);
        }
        std::sort(rows.begin(), rows.end(), [](const DialogRow& a, const DialogRow& b) {
            if (a.order != b.order) return a.order < b.order;
            const int pathOrder = a.path.CompareNoCase(b.path);
            if (pathOrder) return pathOrder < 0;
            return a.dialog->m_blIsDynamic < b.dialog->m_blIsDynamic;
        });
        for (const auto& row : rows)
        {
            auto item = addDialog(row.path, *row.dialog);
            if (!firstLeaf) firstLeaf = item;
            if (m_isSearchMode)
                for (auto parent = m_tree.GetParentItem(item); parent; parent = m_tree.GetParentItem(parent))
                    m_tree.Expand(parent, TVE_EXPAND);
        }
        if (group == LevelGroupToolbars || group == LevelGroupDebugPages)
        {
            for (const auto& resource : m_openedResources)
            {
                const auto& res = resource.second;
                if (!res) continue;
                for (const auto& entry : res->mapLangAndToolbarInfo)
                {
                    auto tb = entry.second;
                    const auto& path = resource.first;
                    if (!tb || ShouldIgnoreLevelAuditToolbar(path, tb->nToolbarID)) continue;
                    UINT pageId = GetLevelAuditToolbarPage(path, tb->nToolbarID);
                    if ((group == LevelGroupDebugPages) != (pageId != 0)) continue;
                    auto page = res->mapDynamicDialogInfo.find(pageId);
                    CString title = GetLevelAuditToolbarText(path, *tb, ML(UI_TOOLBARS(gbCurNeedShowLan)));
                    CString id; id.Format(_T("%u"), tb->nToolbarID);
                    bool match = groupMatches || matches(title) || matches(id);
                    if (page != res->mapDynamicDialogInfo.end() && page->second)
                        match = match || matches(CString(page->second->csCaption));
                    for (const auto& text : tb->vecItemText) match = match || matches(CString(text));
                    if (!match) continue;
                    HTREEITEM parent = page != res->mapDynamicDialogInfo.end() && page->second ?
                        addDialog(path, *page->second) : getGroup(group);
                    auto item = insert(TreeNodeData::NK_Toolbar, tb->nToolbarID, path, title,
                        parent, tb->m_blIsInLevelManager);
                    if (!firstLeaf) firstLeaf = item;
                    if (m_isSearchMode) m_tree.Expand(parent, TVE_EXPAND);
                }
            }
        }
    }
    for (const auto& data : expanded)
    {
        auto item = FindTreeItemForNode(data.kind, data.dlgId, data.filePath);
        if (item) m_tree.Expand(item, TVE_EXPAND);
    }
    if (!hadTree || m_isSearchMode)
        for (const auto& group : groups) m_tree.Expand(group.second, TVE_EXPAND);
    HTREEITEM target = hadSelection ? FindTreeItemForNode(selected.kind, selected.dlgId, selected.filePath) : nullptr;
    if (!target) target = firstLeaf;
    m_inTreeUpdate = false;
    PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
    if (target)
    {
        m_tree.SelectItem(target); // 使用同一选择处理，同步模块身份和右侧预览。
        m_lastSelectedTreeItem = target;
    }
    else
    {
        m_activeDynamicDialog.reset();
        m_activeDynamicFilePath.Empty();
        if (m_surface)
        {
            m_surface->ClearToolbarInfo();
            m_surface->SetDialogInfo(nullptr);
        }
    }
    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
}

/*! 
 * \brief 设置初始文件列表（PE资源和JSON配置）
 * \param binPaths 需要打开的PE文件路径列表
 * \param jsonPath 可选JSON配置文件路径
 */
void CFormLevelManagerEditor::SetInitialFiles(const std::vector<CString>& binPaths, const CString& jsonPath)
{
    // 如果已经通过 SetOpenedResources 设置了共享资源，则跳过 OpenFile 步骤
    // 否则将提供的文件路径通过基类的 OpenFile 立即打开并存储
    const bool hasSharedResources = !m_openedResources.empty();
    if (!hasSharedResources)
    {
        for (const auto& p : binPaths)
        {
            if (!p.IsEmpty())
                OpenFile(p);
        }
    }

    if (!jsonPath.IsEmpty())
    {
        m_jsonPath = jsonPath;
        // 尝试加载 JSON；忽略失败（可能是新文件或空文件）
        if (!hasSharedResources) LoadLevelConfigJson(jsonPath);
    }
    EnsureToolHistogramControls(m_openedResources);
    NormalizeExemptToolbarItems();
}

/*!
 * \brief 设置已打开资源的共享指针（直接共享外部 m_openedResources）
 * \param pOpenedResources 指向已打开资源映射的指针
 */
void CFormLevelManagerEditor::SetOpenedResources(std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>* pOpenedResources,
    const CString& loadedJsonPath)
{
    if (!pOpenedResources)
        return;

    // 直接复制共享指针的内容到本地 m_openedResources
    // 由于 shared_ptr 共享所有权，修改会反映到外部
    m_openedResources = *pOpenedResources;
    m_loadedJsonPath = loadedJsonPath;
    EnsureToolHistogramControls(m_openedResources);

    // 如果有资源，设置第一个为当前活动资源
    if (!m_openedResources.empty())
    {
        auto it = m_openedResources.begin();
        m_currentIniPath = it->first;
        m_pRes = it->second;
    }
}


/*! 
 * \brief 创建等级管理器相关的UI控件（全选复选框、等级下拉框）
 */
void CFormLevelManagerEditor::CreateLevelUiControls()
{
    CRect rc(0,0,0,0);
    if (!m_checkAll.GetSafeHwnd())
    {
        m_checkAll.Create(ML(UI_SELECT_ALL(gbCurNeedShowLan)), WS_CHILD | WS_VISIBLE | BS_3STATE, rc, this, IDC_CHECK_ALL_RCED);
        m_checkAll.SetFont(&m_font);
    }
    if (!m_comboBadge.GetSafeHwnd())
    {
        m_comboBadge.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS, rc, this, IDC_COMBO_BADGE_RCED);
        m_comboBadge.SetFont(&m_font);
        m_comboBadge.ResetContent();
        for (int i = 0; i < 8; ++i)
        {
            CString s;
            s.Format(_T("%d"), i);
            m_comboBadge.AddString(s);
        }
        m_comboBadge.SetCurSel(0);
        m_comboBadge.EnableWindow(TRUE);
        m_comboBadge.SetItemHeight(-1, 18);
        m_comboBadge.SetItemHeight(0, 18);
        int dropW = this->CalcComboBestWidth(m_comboBadge);
        m_comboBadge.SendMessage(CB_SETDROPPEDWIDTH, (WPARAM)dropW, 0);
    }
}

/*! 
 * \brief 切换等级管理器UI控件的可见性
 * \param visible 是否显示
 */
void CFormLevelManagerEditor::OnLevelUiVisibilityChanged(bool visible)
{
    if (m_checkAll.GetSafeHwnd())
        m_checkAll.ShowWindow(visible ? SW_SHOW : SW_HIDE);
    if (m_comboBadge.GetSafeHwnd())
        m_comboBadge.ShowWindow(visible ? SW_SHOW : SW_HIDE);
}

/*! 
 * \brief 布局等级管理器相关UI控件
 * \param rcCheck 全选复选框区域
 * \param rcCombo 等级下拉框区域
 */
void CFormLevelManagerEditor::LayoutLevelUiControls(const CRect& rcCheck, const CRect& rcCombo)
{
    if (m_checkAll.GetSafeHwnd())
        m_checkAll.MoveWindow(rcCheck);
    if (m_comboBadge.GetSafeHwnd())
    {
        m_comboBadge.SetWindowPos(nullptr, rcCombo.left, rcCombo.top, rcCombo.Width(), rcCombo.Height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
        this->UpdateBadgeComboMetrics();
    }
}

/*! 
 * \brief 处理全选复选框点击事件
 */
void CFormLevelManagerEditor::HandleCheckAllClicked()
{
    if (!m_surface)
        return;
    if (m_inBadgeUpdate)
        return;
    // 根据复选框点击确定需要执行的操作。由于消息顺序问题，`GetCheck()` 可能返回
    // 之前的状态，因此通过翻转当前返回的状态来计算新状态。将不确定视为未选中 -> 选中。
    int prevState = m_checkAll.GetCheck();
    int st = (prevState == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED;
    // 立即更新可视状态以匹配解释后的点击。
    m_checkAll.SetCheck(st);

    // If we are in search mode, ensure the surface is bound to the dialog for the
    // currently selected tree node (if any) so SelectAllControls/ClearSelection works
    // even when the UI is showing a filtered/search tree.
    if (m_isSearchMode)
    {
        HTREEITEM hSel = m_tree.GetSelectedItem();
        if (hSel)
        {
            TreeNodeData* selData = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hSel));
            // If selected node is a control, use its parent dialog node
            if (selData && selData->ctrlId > 0)
            {
                HTREEITEM hParent = m_tree.GetParentItem(hSel);
                if (hParent)
                {
                    TreeNodeData* parentData = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hParent));
                    if (parentData)
                        selData = parentData;
                }
            }

            if (selData)
            {
                // Find resource for this file path
                std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
                auto itRes = m_openedResources.find(selData->filePath);
                if (itRes != m_openedResources.end())
                    resPtr = itRes->second;
                if (!resPtr)
                    resPtr = m_pRes;

                if (resPtr)
                {
                    auto dlg = FindDialogInResource(resPtr, selData->dlgId);
                    if (dlg)
                    {
                        // Bind surface to this dialog so selection applies
                        m_pRes = resPtr;
                        auto itEditor = m_openedEditors.find(selData->filePath);
                        m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                        m_currentIniPath = selData->filePath;
                        m_surface->SetDialogInfo(dlg);
                        m_surface->SetCurrentLevel(m_curLevel);
                        if (st == BST_CHECKED)
                            m_surface->SelectAllControls();
                        else
                            m_surface->ClearSelection();
                        m_surface->Invalidate(FALSE);
                        if (!m_isSearchMode)
                            RefreshCurrentTree();
                        else
                        {
                            UpdatePropertyGrid();
                            UpdateBadgeUiFromSelection();
                        }
                        m_checkAll.SetCheck(st);
                        return;
                    }
                }
            }
        }
    }

    auto dlg = m_surface->GetDialogInfo();
    // 优先处理当设计面板正在显示工具栏预览时的全选/反选。
    // 注意：GetDialogInfo() 可能返回上一次的对话框信息，因此不能用其为空来判断是否为工具栏视图。
    auto activeTb = m_surface->GetActiveToolbar();
    if (activeTb)
    {
        if (st == BST_CHECKED)
        {
            std::vector<int> allIdx;
            allIdx.reserve(activeTb->vecItemIDs.size());
            for (size_t i = 0; i < activeTb->vecItemIDs.size(); ++i)
                allIdx.push_back((int)i);
            m_surface->SetSelectedToolbarIndices(allIdx);
        }
        else
        {
            m_surface->SetSelectedToolbarIndices(std::vector<int>());
        }

        // 刷新 UI
        m_surface->Invalidate(FALSE);
        if (!m_isSearchMode)
            RefreshCurrentTree();  // 在非搜索模式下重建树
        else
        {
            UpdatePropertyGrid();
            UpdateBadgeUiFromSelection();
        }
        m_checkAll.SetCheck(st);    //! 最后确保复选框状态正确
        return;
    }

    // 如果既不是工具栏视图也没有对话框，则仅更新 Badge UI
    if (!dlg)
    {
        UpdateBadgeUiFromSelection();
        return;
    }

    // 新行为："全选" 仅影响 UI 选择状态。请勿在此修改等级掩码。
    if (st == BST_CHECKED)
        m_surface->SelectAllControls();
    else
        m_surface->ClearSelection();

    // 刷新表面和属性 UI。属性网格在多选时会显示多选编辑器，
    // 这是在选择多个控件时修改等级掩码的唯一支持方式。
    m_surface->Invalidate(FALSE);
    if (!m_isSearchMode)
        RefreshCurrentTree();  // 在非搜索模式下重建树
    else
    {
        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
    }
	m_checkAll.SetCheck(st);    //! 最后确保复选框状态正确

}

/*! 
 * \brief 处理等级下拉框选择变化事件
 */
void CFormLevelManagerEditor::HandleBadgeSelChange()
{
    if (m_inBadgeUpdate || !m_comboBadge.GetSafeHwnd())
        return;

    int sel = m_comboBadge.GetCurSel();
    if (sel < 0 || sel >= 8)
        return;

    // 只切换预览等级；不改权限掩码、参与管控标志或配置脏状态。
    m_curLevel = sel + 1;
    if (m_surface)
    {
        m_surface->SetCurrentLevel(m_curLevel);
        m_surface->Invalidate(FALSE);
    }
    this->RefreshCurrentTree();  // 使用 RefreshCurrentTree 保持搜索模式
    this->UpdatePropertyGrid();
    this->UpdateBadgeUiFromSelection();
}

/*! \brief 析构函数 */
CFormLevelManagerEditor::~CFormLevelManagerEditor() = default;

/*! 
 * \brief 在控件创建后初始化等级管理器相关UI和菜单
 */
void CFormLevelManagerEditor::OnAfterControlsCreated()
{
    // 仅在派生类中启用等级管理器 UI
    SetDesignSurfaceLevelConfigMode(true);
    CreateLevelUiControls();
    OnLevelUiVisibilityChanged(true);
    
    // 重新设置属性列表控件的回调，使用派生类的处理方法
    if (m_propList.GetSafeHwnd())
    {
        m_propList.SetPropertyChangedCallback([this](int nIndex, const CPropertyListCtrl::SPropertyItem& item) {
            OnPropertyListChanged(nIndex, item);
        });
    }
    
	if (GetSafeHwnd() && ::IsWindow(m_hWnd))
	{
		CMenu* pMenu = GetMenu();
		if (pMenu)
		{
			CMenu* pFile = pMenu->GetSubMenu(0);
			if (pFile)
			{
                // 文件菜单：打开、分隔符、保存、另存为、分隔符、退出。
                // 将菜单项替换为本地化文本
                pFile->ModifyMenu(1, MF_BYPOSITION | MF_STRING, ID_LM_OPEN_JSON, ML(UI_OPEN_JSON_MENU(gbCurNeedShowLan)));
                pFile->ModifyMenu(3, MF_BYPOSITION | MF_STRING, ID_LM_SAVE_JSON, ML(UI_SAVE_JSON_MENU(gbCurNeedShowLan)));
                // 禁用二进制导出/保存命令，防止从等级管理器写出 PE。
                pFile->EnableMenuItem(ID_RCED_FILE_SAVE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
                pFile->EnableMenuItem(ID_RCED_FILE_SAVE_AS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
                // 同时禁用/隐藏工具栏中的保存/另存为按钮（当等级管理器激活时）
				if (m_toolbar.GetSafeHwnd())
				{
					m_toolbar.SendMessage(TB_ENABLEBUTTON, ID_RCED_FILE_SAVE, MAKELONG(FALSE, 0));
					m_toolbar.SendMessage(TB_ENABLEBUTTON, ID_RCED_FILE_SAVE_AS, MAKELONG(FALSE, 0));
				}

                // 附加 JSON 专用的工具栏按钮（打开/保存），使等级管理器具有明显的 JSON 操作。
				if (m_toolbar.GetSafeHwnd())
				{
                    // 添加 JSON 按钮的本地化字符串
					CString jsonStrs;
					jsonStrs += ML(UI_OPEN_JSON_MENU(gbCurNeedShowLan));
					jsonStrs += (TCHAR)'\0';
					jsonStrs += ML(UI_SAVE_JSON_MENU(gbCurNeedShowLan));
					jsonStrs += (TCHAR)'\0';
					int jsonStrBase = (int)m_toolbar.SendMessage(TB_ADDSTRING, 0, (LPARAM)(LPCTSTR)jsonStrs);
					TBBUTTON jsonBtns[2] = {};
					// 打开 JSON 按钮
					jsonBtns[0].iBitmap = I_IMAGENONE;
					jsonBtns[0].idCommand = ID_LM_OPEN_JSON;
					jsonBtns[0].fsState = TBSTATE_ENABLED;
					jsonBtns[0].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
					jsonBtns[0].iString = jsonStrBase + 0;
					// 保存 JSON 按钮
					jsonBtns[1].iBitmap = I_IMAGENONE;
					jsonBtns[1].idCommand = ID_LM_SAVE_JSON;
					jsonBtns[1].fsState = TBSTATE_ENABLED;
					jsonBtns[1].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
					jsonBtns[1].iString = jsonStrBase + 1;
					m_toolbar.SendMessage(TB_ADDBUTTONS, 2, (LPARAM)&jsonBtns);
				}

                // 确保 JSON 保存命令在工具栏/菜单中可见：将 LM 保存映射到相同的 UI 区域
                pFile->ModifyMenu(1, MF_BYPOSITION | MF_STRING, ID_LM_OPEN_JSON, ML(UI_OPEN_JSON_MENU(gbCurNeedShowLan)));
                pFile->ModifyMenu(3, MF_BYPOSITION | MF_STRING, ID_LM_SAVE_JSON, ML(UI_SAVE_JSON_MENU(gbCurNeedShowLan)));
			}
		}
	}
    SetMenu(nullptr);
    DrawMenuBar();
    m_toolbar.ShowWindow(SW_HIDE);
    const CRect rect(0, 0, 1, 1);
    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW;
    m_openResourceButton.icon = ParameterEditorStyle::CButton::Folder;
    m_openResourceButton.Create(ML(UI_OPEN(gbCurNeedShowLan)), buttonStyle, rect, this, ID_RCED_FILE_OPEN);
    m_openJsonButton.primary = true;
    m_openJsonButton.icon = ParameterEditorStyle::CButton::Folder;
    m_openJsonButton.Create(ML(UI_OPEN_JSON_MENU(gbCurNeedShowLan)), buttonStyle, rect, this, ID_LM_OPEN_JSON);
    m_saveJsonButton.primary = true;
    m_saveJsonButton.Create(ML(UI_SAVE(gbCurNeedShowLan)), buttonStyle, rect, this, ID_LM_SAVE_JSON);
    m_saveAsButton.Create(ML(UI_SAVE_AS(gbCurNeedShowLan)), buttonStyle, rect, this, ID_RCED_FILE_SAVE_AS);
    m_closeButton.Create(ML(UI_EXIT(gbCurNeedShowLan)), buttonStyle, rect, this, ID_RCED_FILE_EXIT);
    m_levelLabel.Create(ML(UI_PROPLIST_LEVEL_SETTINGS(gbCurNeedShowLan)), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        rect, this);
    CWnd* controls[] = { &m_openResourceButton, &m_openJsonButton, &m_saveJsonButton,
        &m_saveAsButton, &m_closeButton, &m_levelLabel };
    for (CWnd* control : controls) control->SetFont(&m_font);
    m_btnSearch.icon = ParameterEditorStyle::CButton::Search;
    m_btnSearch.iconOnly = true;
    m_editSearch.ModifyStyle(0, WS_BORDER);
    m_tree.ModifyStyle(0, WS_TABSTOP);
    m_checkAll.ModifyStyle(0, WS_TABSTOP);
    m_tree.SetBkColor(RGB(255,255,255));
    m_tree.SetTextColor(ParameterEditorStyle::Text());
    m_propList.SetExtendedStyle(m_propList.GetExtendedStyle() | LVS_EX_DOUBLEBUFFER);
    m_propList.SetBkColor(RGB(255,255,255));
    CHeaderCtrl* header = m_propList.GetHeaderCtrl();
    if (header) m_styledPropertyHeader.SubclassWindow(header->GetSafeHwnd());
    m_fileStatus.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS | SS_NOPREFIX | SS_NOTIFY,
        rect, this);
    m_fileStatus.SetFont(&m_font);
    RefreshFileDisplay();
}

CStringW CFormLevelManagerEditor::GetInitialCaption() const
{
    CStringW title = CStringW(ML(UI_WINDOW_TITLE_LEVEL_MANAGER_PREFIX(gbCurNeedShowLan)));
    title += m_loadedJsonPath.IsEmpty()
        ? CStringW(ParameterEditorStyle::FileStatusText(ParameterEditorStyle::NoLoadedFile, gbCurNeedShowLan - 1))
        : NormalizeJsonDisplayPath(m_loadedJsonPath);
    if (m_dirty)
        title += ParameterEditorStyle::FileStatusText(ParameterEditorStyle::UnsavedChanges, gbCurNeedShowLan - 1);
    return title;
}

void CFormLevelManagerEditor::RefreshFileDisplay()
{
    SetWindowCaption(GetInitialCaption());
    if (!m_fileStatus.GetSafeHwnd()) return;
    CStringW text = ParameterEditorStyle::FileStatusText(m_savedToTarget && !m_dirty
        ? ParameterEditorStyle::SavedTo : ParameterEditorStyle::SaveTo, gbCurNeedShowLan - 1);
    text += m_jsonPath.IsEmpty()
        ? CStringW(ParameterEditorStyle::FileStatusText(ParameterEditorStyle::NoSaveTarget, gbCurNeedShowLan - 1))
        : NormalizeJsonDisplayPath(m_jsonPath);
    const CString display = rceditor::CStringConverter::ConvertToT(text);
    m_fileStatus.SetWindowText(display);
    SetParameterPathTooltip(m_fileStatus, display);
}

void CFormLevelManagerEditor::LayoutControls()
{
    if (!m_closeButton.GetSafeHwnd()) return;
    CRect client; GetClientRect(client);
    const int margin = StylePx(12), gap = StylePx(12), row = StylePx(32);
    const int top = ParameterCaptionHeight() + margin;
    const int footer = client.bottom - StylePx(76);
    const int treeWidth = (std::max)(StylePx(240), (std::min)(StylePx(330), client.Width()/4));
    const int right = margin + treeWidth + gap;
    const int bandHeight = StylePx(52);
    const int bodyTop = top + bandHeight + StylePx(10);
    const int propertyWidth = (std::max)(StylePx(240), (std::min)(StylePx(340), client.Width()/4));
    const int propertyLeft = client.right - margin - propertyWidth;
    SetParameterToolbarRect(CRect(right, top, client.right-margin, top+bandHeight));
    m_editSearch.MoveWindow(margin, top, treeWidth-row+1, row);
    m_btnSearch.MoveWindow(margin+treeWidth-row, top, row, row);
    m_tree.MoveWindow(margin, top+row+StylePx(10), treeWidth, footer-StylePx(8)-top-row-StylePx(10));
    if (m_surface && m_surface->GetSafeHwnd())
        m_surface->MoveWindow(right, bodyTop, (std::max)(1, propertyLeft-gap-right), footer-StylePx(8)-bodyTop);
    m_propList.MoveWindow(propertyLeft, bodyTop, propertyWidth, footer-StylePx(8)-bodyTop);
    const int y = top + StylePx(10);
    m_levelLabel.MoveWindow(right+StylePx(12), y, StylePx(92), row);
    LayoutLevelUiControls(CRect(right+StylePx(12), footer+StylePx(6), right+StylePx(122), footer+StylePx(6)+row),
        CRect(right+StylePx(108), y, right+StylePx(180), y+row));
    const int openWidth = (std::max)(1, (std::min)(StylePx(224), static_cast<int>(client.right)-margin-right-StylePx(322)));
    m_openJsonButton.MoveWindow(client.right-margin-StylePx(10)-openWidth, y, openWidth, row);
    m_openResourceButton.MoveWindow(margin, footer+StylePx(6), StylePx(124), row);
    m_saveAsButton.MoveWindow(margin+StylePx(136), footer+StylePx(6), StylePx(110), row);
    m_closeButton.MoveWindow(client.right-margin-StylePx(188), footer+StylePx(6), StylePx(88), row);
    m_saveJsonButton.MoveWindow(client.right-margin-StylePx(88), footer+StylePx(6), StylePx(88), row);
    m_fileStatus.MoveWindow(margin, footer+StylePx(44), client.Width()-2*margin, StylePx(24));
}

/*! 
 * \brief 对话框关闭前的处理（如保存JSON、关闭等级管理模式）
 */
void CFormLevelManagerEditor::OnBeforeDialogClose()
{
	SetDesignSurfaceLevelConfigMode(false);
}

/*! 
 * \brief 弹出文件选择对话框，选择要打开的JSON文件
 * \param outPath 输出选中的文件路径
 * \return 是否成功选择文件
 */
bool CFormLevelManagerEditor::PickJsonOpenFile(CString& outPath) const
{
    CFileDialog dlg(TRUE, _T("json"), NULL,
        OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_ENABLESIZING,
        (LPCTSTR)ML(UI_JSON_FILE_FILTER(gbCurNeedShowLan)),
        (const_cast<CFormLevelManagerEditor*>(this)));
	if (dlg.DoModal() != IDOK)
		return false;
	outPath = dlg.GetPathName();
	return true;
}

/*! 
 * \brief 弹出文件保存对话框，选择保存JSON的路径
 * \param outPath 输出选中的保存路径
 * \return 是否成功选择路径
 */
bool CFormLevelManagerEditor::PickJsonSaveFile(CString& outPath) const
{
    CFileDialog dlg(FALSE, _T("json"), NULL,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
        (LPCTSTR)ML(UI_JSON_FILE_FILTER(gbCurNeedShowLan)),
        (const_cast<CFormLevelManagerEditor*>(this)));
	if (dlg.DoModal() != IDOK)
		return false;
	outPath = dlg.GetPathName();
	return true;
}

/*! 
 * \brief 加载等级管理器JSON配置
 * \param jsonPath JSON文件路径
 * \return 加载是否成功
 */
bool CFormLevelManagerEditor::LoadLevelConfigJson(const CString& jsonPath)
{
    EnsureToolHistogramControls(m_openedResources);
    Json::Value root;
    if (!rceditor::JsonUtils::LoadJson(jsonPath, root))
        return false;

    if (!root.isArray())
        return false;

    for (Json::Value::ArrayIndex i = 0; i < root.size(); ++i)
    {
        const Json::Value& jFile = root[i];
        std::string binNameUtf8;
        if (jFile.isMember("BinFileName") && jFile["BinFileName"].isString())
            binNameUtf8 = jFile["BinFileName"].asString();

        // 寻找目标资源：嵌入式资源或与已打开资源按文件名/编辑器名匹配的资源
        std::shared_ptr<rceditor::SSingleResourceInfo> targetRes;
        if (binNameUtf8 == std::string(LM_BINNAME_EMBEDDED))
        {
            if (m_pRes)
                targetRes = m_pRes;
        }
        else
        {
            for (const auto& kv : m_openedResources)
            {
                const CString& path = kv.first;
                if (IsSameLevelResourceModule(path, binNameUtf8))
                {
                    targetRes = kv.second;
                    break;
                }
                auto itEd = m_openedEditors.find(path);
                if (itEd != m_openedEditors.end() && itEd->second)
                {
                    CString edName = itEd->second->GetResourceFileName();
                    if (IsSameLevelResourceModule(edName, binNameUtf8))
                    {
                        targetRes = kv.second;
                        break;
                    }
                }
            }
        }

        if (!targetRes)
            continue;

        // 动态窗口由独立数组导出，不能依赖静态 WndInfos 是否存在。
        if (jFile.isMember("DynamicWndInfos") && jFile["DynamicWndInfos"].isArray())
        {
            const Json::Value& dynArray = jFile["DynamicWndInfos"];
            for (Json::Value::ArrayIndex j = 0; j < dynArray.size(); ++j)
            {
                const Json::Value& jWnd = dynArray[j];
                if (!jWnd.isObject())
                    continue;
                const UINT dlgId = rceditor::JsonUtils::SafeGetUInt(jWnd, "IDD", 0u);
                if (dlgId == 0u)
                    continue;

                auto& dlg = targetRes->mapDynamicDialogInfo[dlgId];
                if (!dlg)
                {
                    dlg = std::make_shared<rceditor::SDialogInfo>();
                    dlg->nIDD = dlgId;
                    dlg->m_blIsDynamic = true;
                }
                if (jWnd.isMember("InLevelManager") && jWnd["InLevelManager"].isBool())
                    dlg->m_blIsInLevelManager = jWnd["InLevelManager"].asBool();
                if (jWnd.isMember("ClassPath") && jWnd["ClassPath"].isString())
                    dlg->csClassPath = rceditor::JsonUtils::Utf8ToCStringW(jWnd["ClassPath"].asString());
                // 保留已注册的本地化文字；仅为新建或缺少文字的对象补齐预览信息。
                if (dlg->csCaption.IsEmpty())
                {
                    if (jWnd.isMember("Caption") && jWnd["Caption"].isString())
                        dlg->csCaption = rceditor::JsonUtils::Utf8ToCStringW(jWnd["Caption"].asString());
                    else if (jWnd.isMember("WndName") && jWnd["WndName"].isString())
                        dlg->csCaption = rceditor::JsonUtils::Utf8ToCStringW(jWnd["WndName"].asString());
                }
                if (!jWnd.isMember("CtrlInfos") || !jWnd["CtrlInfos"].isArray())
                    continue;
                const Json::Value& ctrlArray = jWnd["CtrlInfos"];
                for (Json::Value::ArrayIndex k = 0; k < ctrlArray.size(); ++k)
                {
                    const Json::Value& jCtrl = ctrlArray[k];
                    if (!jCtrl.isObject())
                        continue;
                    const UINT ctrlId = rceditor::JsonUtils::SafeGetUInt(jCtrl, "CtrlID", 0u);
                    if (ctrlId == 0u)
                        continue;
                    std::shared_ptr<rceditor::SCtrlInfo> ctrl;
                    for (const auto& c : dlg->vecCtrlInfo)
                    {
                        if (c && c->nIDC == ctrlId)
                        {
                            ctrl = c;
                            break;
                        }
                    }
                    if (!ctrl)
                    {
                        ctrl = std::make_shared<rceditor::SCtrlInfo>();
                        ctrl->nIDC = ctrlId;
                        dlg->vecCtrlInfo.push_back(ctrl);
                    }
                    ctrl->uiLevelMask = rceditor::JsonUtils::SafeGetUInt(jCtrl, "LevelMask", ctrl->uiLevelMask);
                    if (ctrl->csClassName.IsEmpty() && jCtrl.isMember("CtrlType") && jCtrl["CtrlType"].isString())
                        ctrl->csClassName = rceditor::JsonUtils::Utf8ToCStringW(jCtrl["CtrlType"].asString());
                    if (ctrl->csText.IsEmpty() && jCtrl.isMember("Text") && jCtrl["Text"].isString())
                        ctrl->csText = rceditor::JsonUtils::Utf8ToCStringW(jCtrl["Text"].asString());
                }
            }
        }
        // 处理窗口条目：对话框级属性、控件信息
        if (!jFile.isMember("WndInfos") || !jFile["WndInfos"].isArray())
            continue;

        const Json::Value& wndArray = jFile["WndInfos"];
        for (Json::Value::ArrayIndex j = 0; j < wndArray.size(); ++j)
        {
            const Json::Value& jWnd = wndArray[j];
            const UINT dlgId = rceditor::JsonUtils::SafeGetUInt(jWnd, "IDD", 0u);
            if (dlgId == 0u)
                continue;

            auto itDlg = targetRes->mapLangAndDialogInfo.find(dlgId);
            if (itDlg == targetRes->mapLangAndDialogInfo.end() || !itDlg->second)
                continue;
            auto& dlg = itDlg->second;

            // 对话框属性
            if (jWnd.isMember("InLevelManager") && jWnd["InLevelManager"].isBool())
                dlg->m_blIsInLevelManager = jWnd["InLevelManager"].asBool();
            if (jWnd.isMember("ClassPath") && jWnd["ClassPath"].isString())
                dlg->csClassPath = rceditor::JsonUtils::Utf8ToCStringW(jWnd["ClassPath"].asString());

            // 控件信息
            if (jWnd.isMember("CtrlInfos") && jWnd["CtrlInfos"].isArray())
            {
                const Json::Value& ctrlArray = jWnd["CtrlInfos"];
                std::vector<std::pair<UINT, UINT>> histogramMasks;
                for (Json::Value::ArrayIndex k = 0; k < ctrlArray.size(); ++k)
                {
                    const Json::Value& jCtrl = ctrlArray[k];
                    const UINT ctrlId = rceditor::JsonUtils::SafeGetUInt(jCtrl, "CtrlID", 0u);
                    const UINT mask = rceditor::JsonUtils::SafeGetUInt(jCtrl, "LevelMask", 0u);
                    histogramMasks.emplace_back(ctrlId, mask);
                    if (ctrlId == 0u)
                        continue;
                    for (auto& c : dlg->vecCtrlInfo)
                    {
                        if (!c || c->nIDC != ctrlId)
                            continue;
                        c->uiLevelMask = mask;
                        break;
                    }
                }
                MigrateToolHistogramMasks(dlgId, *dlg, histogramMasks);
            }

            // 窗口下的 ToolbarInfos（可选）
            if (jWnd.isMember("ToolbarInfos") && jWnd["ToolbarInfos"].isArray())
            {
                const Json::Value& tbArray = jWnd["ToolbarInfos"];
                for (Json::Value::ArrayIndex t = 0; t < tbArray.size(); ++t)
                {
                    const Json::Value& jTb = tbArray[t];
                    const UINT tbId = rceditor::JsonUtils::SafeGetUInt(jTb, "ToolbarID", 0u);
                    if (tbId == 0u)
                        continue;
                    auto itTb = targetRes->mapLangAndToolbarInfo.find(tbId);
                    if (itTb == targetRes->mapLangAndToolbarInfo.end() || !itTb->second)
                        continue;
                    auto& tb = itTb->second;
                    if (jTb.isMember("InLevelManager") && jTb["InLevelManager"].isBool())
                        tb->m_blIsInLevelManager = jTb["InLevelManager"].asBool();
                    if (jTb.isMember("ItemInfos") && jTb["ItemInfos"].isArray())
                    {
                        const Json::Value& itemArray = jTb["ItemInfos"];
                        for (Json::Value::ArrayIndex ii = 0; ii < itemArray.size(); ++ii)
                        {
                            const Json::Value& jItem = itemArray[ii];
                            int idx = rceditor::JsonUtils::SafeGetInt(jItem, "Index", -1);
                            if (idx < 0)
                                continue;
                            if ((size_t)idx >= tb->vecItemIDs.size())
                                continue;
                            if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
                                tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0);
                            tb->vecLevelMask[idx] = rceditor::JsonUtils::SafeGetUInt(jItem, "LevelMask", 0u);
                            if (jItem.isMember("Text") && jItem["Text"].isString())
                            {
                                std::string itext = jItem["Text"].asString();
                                CStringW tw = rceditor::JsonUtils::Utf8ToCStringW(itext);
                                if (tb->vecItemText.size() < tb->vecItemIDs.size())
                                    tb->vecItemText.resize(tb->vecItemIDs.size());
                                // Only sync JSON text if no text was found from RT_STRING resources
                                if (tb->vecItemText[idx].IsEmpty())
                                    tb->vecItemText[idx] = tw;
                            }
                        }
                    }
                }
            }
        }

        // 文件级的 ToolbarInfos（部分 JSON 使用文件范围）
        if (jFile.isMember("ToolbarInfos") && jFile["ToolbarInfos"].isArray())
        {
            const Json::Value& fileTbArray = jFile["ToolbarInfos"];
            for (Json::Value::ArrayIndex t = 0; t < fileTbArray.size(); ++t)
            {
                const Json::Value& jTb = fileTbArray[t];
                const UINT tbId = rceditor::JsonUtils::SafeGetUInt(jTb, "ToolbarID", 0u);
                if (tbId == 0u)
                    continue;
                auto itTb = targetRes->mapLangAndToolbarInfo.find(tbId);
                if (itTb == targetRes->mapLangAndToolbarInfo.end() || !itTb->second)
                    continue;
                auto& tb = itTb->second;
                if (jTb.isMember("InLevelManager") && jTb["InLevelManager"].isBool())
                    tb->m_blIsInLevelManager = jTb["InLevelManager"].asBool();
                if (jTb.isMember("ItemInfos") && jTb["ItemInfos"].isArray())
                {
                    const Json::Value& itemArray = jTb["ItemInfos"];
                    for (Json::Value::ArrayIndex ii = 0; ii < itemArray.size(); ++ii)
                    {
                        const Json::Value& jItem = itemArray[ii];
                        int idx = rceditor::JsonUtils::SafeGetInt(jItem, "Index", -1);
                        if (idx < 0)
                            continue;
                        if ((size_t)idx >= tb->vecItemIDs.size())
                            continue;
                        if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
                            tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0);
                        tb->vecLevelMask[idx] = rceditor::JsonUtils::SafeGetUInt(jItem, "LevelMask", 0u);
                        if (jItem.isMember("Text") && jItem["Text"].isString())
                        {
                            std::string itext = jItem["Text"].asString();
                            CStringW tw = rceditor::JsonUtils::Utf8ToCStringW(itext);
                            if (tb->vecItemText.size() < tb->vecItemIDs.size())
                                tb->vecItemText.resize(tb->vecItemIDs.size());
                            // Only sync JSON text if no text was found from RT_STRING resources
                            if (tb->vecItemText[idx].IsEmpty())
                                tb->vecItemText[idx] = tw;
                        }
                    }
                }
            }
        }
    }

    NormalizeExemptToolbarItems();
    m_loadedJsonPath = jsonPath;
    m_jsonPath = jsonPath;
    m_savedToTarget = false;
    m_dirty = false;
    RefreshFileDisplay();

    RebuildTree();
    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
    if (m_surface)
        m_surface->Invalidate(FALSE);
    return true;
}

/*! 
 * \brief 保存当前等级管理器配置到JSON文件
 * \param jsonPath 目标JSON文件路径
 * \return 保存是否成功
 */
bool CFormLevelManagerEditor::SaveLevelConfigJson(const CString& jsonPath)
{
    NormalizeExemptToolbarItems();
    Json::Value root(Json::arrayValue);

    // 如果存在已打开的资源，为每个资源导出一条 JSON 条目
    if (!m_openedResources.empty())
    {
        for (const auto& kv : m_openedResources)
        {
            const CString& path = kv.first;
            auto res = kv.second;
            if (!res)
                continue;

            // BinFileName 应为文件名部分。
            int pos = path.ReverseFind(_T('\\'));
            CString fname = (pos >= 0) ? path.Mid(pos + 1) : path;
            std::string fnameUtf8 = rceditor::JsonUtils::CStringWToUtf8((CStringW)fname);

            Json::Value jFile;
            jFile["BinFileName"] = Json::Value(fnameUtf8);
            Json::Value wndArray(Json::arrayValue);

            // Export static dialogs (WndInfos)
            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dialogs;
            dialogs.reserve(res->mapLangAndDialogInfo.size());
            for (const auto& kv2 : res->mapLangAndDialogInfo)
                dialogs.push_back(kv2);
            std::sort(dialogs.begin(), dialogs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

            for (const auto& kv2 : dialogs)
            {
                auto dlg = kv2.second;
                if (!dlg)
                    continue;

                Json::Value jWnd;
                jWnd["IDD"] = (int)dlg->nIDD;
                jWnd["LangID"] = (unsigned int)dlg->langID;
                jWnd["WndName"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csCaption).c_str());
                jWnd["ClassPath"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csClassPath).c_str());
                jWnd["InLevelManager"] = Json::Value(dlg->m_blIsInLevelManager);

                Json::Value ctrlArray(Json::arrayValue);
                for (const auto& ctrl : dlg->vecCtrlInfo)
                {
                    if (!ctrl)
                        continue;
                    Json::Value jCtrl;
                    jCtrl["CtrlID"] = (unsigned int)ctrl->nIDC;
                    jCtrl["CtrlType"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csClassName).c_str());
                    jCtrl["CtrlText"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csText).c_str());
                    jCtrl["LevelMask"] = Json::Value((Json::UInt)ctrl->uiLevelMask);
                    ctrlArray.append(jCtrl);
                }
                jWnd["CtrlInfos"] = ctrlArray;
                wndArray.append(jWnd);
            }

            // Export dynamic dialogs separately into DynamicWndInfos
            Json::Value dynArrayOut(Json::arrayValue);
            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dd;
            dd.reserve(res->mapDynamicDialogInfo.size());
            for (const auto& kv2 : res->mapDynamicDialogInfo)
                dd.push_back(kv2);
            std::sort(dd.begin(), dd.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
            for (const auto& kv2 : dd)
            {
                auto dlg = kv2.second;
                if (!dlg) continue;
                Json::Value jDyn;
                jDyn["IDD"] = (int)dlg->nIDD;
                jDyn["LangID"] = (unsigned int)dlg->langID; // include lang for parity with static WndInfos
                jDyn["InLevelManager"] = Json::Value(dlg->m_blIsInLevelManager);
                // save both WndName and Caption for compatibility
                jDyn["WndName"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csCaption).c_str());
                jDyn["Caption"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csCaption).c_str());
                jDyn["ClassPath"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csClassPath).c_str());
                Json::Value ctrlArray(Json::arrayValue);
                    for (const auto& ctrl : dlg->vecCtrlInfo)
                {
                    if (!ctrl) continue;
                    Json::Value jCtrl;
                    jCtrl["CtrlID"] = (unsigned int)ctrl->nIDC;
                    // include control class/type to preserve CtrlType info
                    jCtrl["CtrlType"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csClassName).c_str());
                    jCtrl["CtrlTypeOriginal"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csClassName).c_str());
                    jCtrl["LevelMask"] = Json::Value((Json::UInt)ctrl->uiLevelMask);
                    jCtrl["Text"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csText).c_str());
                    ctrlArray.append(jCtrl);
                }
                jDyn["CtrlInfos"] = ctrlArray;
                dynArrayOut.append(jDyn);
            }
            if (!dynArrayOut.empty())
                jFile["DynamicWndInfos"] = dynArrayOut;

            jFile["WndInfos"] = wndArray;

            // 将工具栏一次性按文件导出到 jFile["ToolbarInfos"] 下。
            Json::Value fileTbArray(Json::arrayValue);
            for (const auto& tbkv : res->mapLangAndToolbarInfo)
            {
                auto tb = tbkv.second;
                if (!tb) continue;
                Json::Value jTb;
                jTb["ToolbarID"] = (unsigned int)tb->nToolbarID;
                jTb["InLevelManager"] = Json::Value(tb->m_blIsInLevelManager);
                Json::Value itemArray(Json::arrayValue);
                for (size_t ii = 0; ii < tb->vecItemIDs.size(); ++ii)
                {
                    Json::Value jItem;
                    jItem["Index"] = (int)ii;
                    jItem["ItemID"] = (unsigned int)tb->vecItemIDs[ii];
                    UINT mask = 0;
                    if (ii < tb->vecLevelMask.size()) mask = tb->vecLevelMask[ii];
                    jItem["LevelMask"] = (Json::UInt)mask;
                    std::string txt = "";
                    if (ii < tb->vecItemText.size())
                    {
                        // If the text equals the one from RT_STRING resources, omit it to avoid duplicating resource-derived text.
                        CStringW cur = tb->vecItemText[ii];
                        UINT cmdId = tb->vecItemIDs[ii];
                        UINT blockId = (cmdId / 16) + 1;
                        auto sit = res->mapLangAndStringTableInfo.find(blockId);
                        bool sameAsRes = false;
                        if (sit != res->mapLangAndStringTableInfo.end() && sit->second)
                        {
                            auto& mp = sit->second->mapIDAndString;
                            auto mit = mp.find(cmdId);
                            if (mit != mp.end() && mit->second == cur)
                                sameAsRes = true;
                        }
                        if (!sameAsRes)
                            txt = rceditor::JsonUtils::CStringWToUtf8(cur);
                    }
                    jItem["Text"] = txt;
                    itemArray.append(jItem);
                }
                jTb["ItemInfos"] = itemArray;
                fileTbArray.append(jTb);
            }
            jFile["ToolbarInfos"] = fileTbArray;
            root.append(jFile);
        }
    }
    else
    {
    // 兜底：如果没有已打开的外部资源，则导出当前嵌入式活动资源（如果存在）
        auto res = GetEditableResource();
        if (res)
        {
            Json::Value jFile;
            jFile["BinFileName"] = Json::Value("<embedded>");
            Json::Value wndArray(Json::arrayValue);

            std::vector<std::pair<UINT, std::shared_ptr<rceditor::SDialogInfo>>> dialogs;
            // include dynamic dialogs for embedded resource export as well
            dialogs.reserve(res->mapLangAndDialogInfo.size() + res->mapDynamicDialogInfo.size());
            for (const auto& kv : res->mapLangAndDialogInfo)
                dialogs.push_back(kv);
            for (const auto& kv : res->mapDynamicDialogInfo)
                dialogs.push_back(kv);
            std::sort(dialogs.begin(), dialogs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

            for (const auto& kv : dialogs)
            {
                auto dlg = kv.second;
                if (!dlg)
                    continue;

                Json::Value jWnd;
                jWnd["IDD"] = (int)dlg->nIDD;
                jWnd["LangID"] = (unsigned int)dlg->langID;
                jWnd["WndName"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csCaption).c_str());
                jWnd["ClassPath"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(dlg->csClassPath).c_str());
                jWnd["InLevelManager"] = Json::Value(dlg->m_blIsInLevelManager);

                Json::Value ctrlArray(Json::arrayValue);
                for (const auto& ctrl : dlg->vecCtrlInfo)
                {
                    if (!ctrl)
                        continue;
                    Json::Value jCtrl;
                    jCtrl["CtrlID"] = (unsigned int)ctrl->nIDC;
                    jCtrl["CtrlType"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csClassName).c_str());
                    jCtrl["CtrlText"] = Json::Value(rceditor::JsonUtils::CStringWToUtf8(ctrl->csText).c_str());
                    jCtrl["LevelMask"] = Json::Value((Json::UInt)ctrl->uiLevelMask);
                    ctrlArray.append(jCtrl);
                }
                jWnd["CtrlInfos"] = ctrlArray;
                wndArray.append(jWnd);
            }

            jFile["WndInfos"] = wndArray;
            root.append(jFile);
        }
    }

    return rceditor::JsonUtils::SaveJson(jsonPath, root);
}

/*! \brief 标记当前数据为已修改（脏） */
void CFormLevelManagerEditor::MarkDirty()
{
	m_dirty = true;
    RefreshFileDisplay();
}

/*! 
 * \brief 如果有未保存的修改则保存JSON
 * \param forcePickPath 是否强制弹出保存路径选择
 * \return 保存是否成功
 */
bool CFormLevelManagerEditor::SaveIfDirty(bool forcePickPath)
{
	if (!m_dirty && !forcePickPath)
		return true;
	CString path = m_jsonPath;
	if (path.IsEmpty() || forcePickPath)
	{
		if (!PickJsonSaveFile(path))
			return false;
	}
	if (!SaveLevelConfigJson(path))
		return false;
	m_jsonPath = path;
	m_dirty = false;
    m_savedToTarget = true;
    RefreshFileDisplay();
	return true;
}

/*! \brief 打开等级管理器JSON文件 */
void CFormLevelManagerEditor::OnFileOpenLevelJson()
{
	CString path;
	if (!PickJsonOpenFile(path))
		return;
	if (LoadLevelConfigJson(path))
	{
		AfxMessageBox(ML(UI_MSG_JSON_LOADED(gbCurNeedShowLan)), MB_ICONINFORMATION);
		return;
	}
	AfxMessageBox(ML(UI_MSG_JSON_LOAD_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
}

/*! \brief 另存为等级管理器JSON文件 */
void CFormLevelManagerEditor::OnFileSaveAsLevelJson()
{
    if (SaveIfDirty(true))
    {
        AfxMessageBox(ML(UI_MSG_JSON_SAVED(gbCurNeedShowLan)), MB_ICONINFORMATION);
        return;
    }
    AfxMessageBox(ML(UI_MSG_JSON_SAVE_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
}

/*! \brief 保存等级管理器JSON文件（如有路径则直接保存） */
void CFormLevelManagerEditor::OnFileSaveJson()
{
    // 写成功后才关闭
    CString path = m_jsonPath;
    if (path.IsEmpty() && !PickJsonSaveFile(path))
        return;
    if (!SaveLevelConfigJson(path))
    {
        AfxMessageBox(ML(UI_MSG_JSON_SAVE_FAILED(gbCurNeedShowLan)), MB_ICONERROR);
        return;
    }
    m_jsonPath = path;
    m_dirty = false;
    m_savedToTarget = true;
    EndDialog(IDOK);
}

/*! 
 * \brief 属性网格属性变更处理，仅允许等级相关属性修改
 * \param wParam 未使用
 * \param lParam 属性指针
 * \return 处理结果
 */
LRESULT CFormLevelManagerEditor::OnPropertyChanged(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    // 属性网格已移除，此消息处理保留为空
    // 属性变更通过 CPropertyListCtrl 的回调处理
    return 0;
}

/*! 
 * \brief 树控件点击事件处理，标记数据为已修改
 * \param pNMHDR 通知消息头
 * \param pResult 结果指针
 */
void CFormLevelManagerEditor::OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    if (pResult) *pResult = 0;
    if (m_closing)
        return;
    if (m_inTreeUpdate)
        return;

    // 获取点击位置并判断是否点击在状态图标上
    CPoint pt;
    ::GetCursorPos(&pt);
    m_tree.ScreenToClient(&pt);

    UINT flags = 0;
    HTREEITEM hItem = m_tree.HitTest(pt, &flags);

    // 如果点击不是在状态图标上，委托基类处理（选择等行为）
    if (!hItem || (flags & TVHT_ONITEMSTATEICON) == 0)
    {
        CFormRcEditor::OnNMClickTree(pNMHDR, pResult);
        return;
    }

    // 处理复选框点击：非管理员不允许修改勾选状态
    if (ToggleDialogTreeChecked(hItem) || ToggleToolbarTreeChecked(hItem))
        return;
    TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
    if (!rceditor::gbIsLevelAdmin)
    {
        // 恢复模型中的状态
        std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
        auto itRes = m_openedResources.find(data ? data->filePath : CString());
        if (itRes != m_openedResources.end())
            resPtr = itRes->second;
        if (!resPtr)
            resPtr = m_pRes;

        if (resPtr && data)
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
            else if (data->kind == TreeNodeData::NK_Control || data->kind == TreeNodeData::NK_DynamicItem)
            {
            auto dlg = FindDialogInResource(resPtr, data->dlgId);
            if (dlg)
            {
                    int levelIdx = (m_curLevel > 0) ? (m_curLevel - 1) : 0;
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
            else if (data->kind == TreeNodeData::NK_ToolbarItem)
            {
                auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
                if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second)
                {
                    auto tb = itTb->second;
                    size_t idx = data->ctrlId;
                    int levelIdx = (m_curLevel > 0) ? (m_curLevel - 1) : 0;
                    if (levelIdx > 7) levelIdx = 7;
                    BOOL checked = FALSE;
                    if (idx < tb->vecLevelMask.size() && (tb->vecLevelMask[idx] & (1u << levelIdx)) != 0)
                        checked = TRUE;
                    m_tree.SetCheck(hItem, checked);
                }
            }
        }

        return;
    }

    // 管理员：执行切换并与模型/Surface交互
    // 处理顺序：获取当前可见状态 -> 确认资源/刷新 surface 与 data 上下文 -> 取反当前状态作为目标状态 -> 应用到模型并刷新树
    BOOL curChecked = m_tree.GetCheck(hItem);
    const bool curState = (curChecked != FALSE);

    if (!data)
        return;

    // Determine resource for this node (do not change m_pRes here)
    std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = nullptr;
    auto itRes = m_openedResources.find(data->filePath);
    if (itRes != m_openedResources.end())
        resPtr = itRes->second;
    if (!resPtr)
        resPtr = m_pRes;
    if (!resPtr)
        return;

    // 先确保 surface 与 data 处于与该节点对应的上下文（dialog/toolbar）
    if (data->kind == TreeNodeData::NK_Control || data->kind == TreeNodeData::NK_Dialog )
    {
        auto dlg = FindDialogInResource(resPtr, data->dlgId);
        if (dlg && m_surface)
        {
            m_surface->SetDialogInfo(dlg);
            m_surface->SetCurrentLevel(m_curLevel);
            m_surface->Invalidate(FALSE);
        }
    }
    else if (data->kind == TreeNodeData::NK_Toolbar || data->kind == TreeNodeData::NK_ToolbarItem)
    {
        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
        if (itTb != resPtr->mapLangAndToolbarInfo.end() && itTb->second && m_surface)
        {
            // If surface is showing this toolbar, update its preview
            if (m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == itTb->second->nToolbarID)
            {
                m_surface->SetToolbarInfo(itTb->second);
                m_surface->Invalidate(FALSE);
            }
        }
    }
    else if (data->kind == TreeNodeData::NK_DynamicItem || data->kind == TreeNodeData::NK_DynamicWnd)
    {
        // 对于动态窗口/项，我们不再将其作为工具栏预览显示。
        // 改为在 Surface 上设置对话框模型，以便 Surface 正确渲染。
        auto dlg = FindDialogInResource(resPtr, data->dlgId);
        if (dlg && m_surface)
        {
            m_surface->SetDialogInfo(dlg);
            m_surface->SetCurrentLevel(m_curLevel);
            m_surface->Invalidate(FALSE);
        }
    }

    // 目标状态为当前可见状态的取反（用户点击意图）
    // 注意：Tree 在处理 NM_CLICK 时其可视状态可能尚未完成切换，
    // 因此我们仅根据当前可见状态计算目标状态，并只更新模型数据，
    // 让控件自身完成可视切换以避免闪烁。
    const bool newChecked = !curState;

    if (data->kind == TreeNodeData::NK_Dialog || data->kind == TreeNodeData::NK_Control)
    {
        auto dlg = FindDialogInResource(resPtr, data->dlgId);
        if (!dlg)
            return;

        if (data->kind == TreeNodeData::NK_Dialog)
        {
            // 仅更新模型，不立即强制刷新 Tree 的勾选状态，避免闪烁
            dlg->m_blIsInLevelManager = newChecked;
        }
        else // control
        {
            if (!newChecked)
            {
                // 不允许取消控件的单独勾选（与 CLevelManagerViewer 行为一致）
                // 这里需要强制恢复为选中状态，因为用户意图是取消但我们不允许。
                m_tree.SetCheck(hItem, TRUE);
                return;
            }

            // 允许将控件标记为参与等级管理：更新模型的标志和等级掩码
            if (!dlg->m_blIsInLevelManager)
                dlg->m_blIsInLevelManager = true;

            const UINT storedId = data->ctrlId;
            int levelIdx = (m_curLevel > 0) ? (m_curLevel - 1) : 0;
            if (m_isSearchMode)
            {
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
    else if (data->kind == TreeNodeData::NK_Toolbar)
    {
        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
        if (itTb == resPtr->mapLangAndToolbarInfo.end() || !itTb->second)
            return;
        auto tb = itTb->second;
        tb->m_blIsInLevelManager = newChecked;
        // 不强制 SetCheck，保留控件自身的视觉更新以避免闪烁

        if (m_surface && m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == tb->nToolbarID)
        {
            m_surface->SetToolbarInfo(tb);
            m_surface->Invalidate(FALSE);
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
        return;
    }
    else if (data->kind == TreeNodeData::NK_DynamicWnd)
    {
        // Toggle dynamic dialog participation in level manager
        auto itDlg = resPtr->mapDynamicDialogInfo.find(data->dlgId);
        if (itDlg == resPtr->mapDynamicDialogInfo.end() || !itDlg->second)
            return;
        auto dlg = itDlg->second;
        dlg->m_blIsInLevelManager = newChecked;
        // Refresh surface if it's currently showing this dynamic dialog.
        if (m_surface)
        {
            // If surface currently displays this dialog (dynamic), update dialog model so
            // the surface will render dynamic window correctly (we no longer treat dynamic
            // windows as toolbars).
            auto cur = m_surface->GetDialogInfo();
            if (cur && cur->nIDD == dlg->nIDD)
            {
                m_surface->SetDialogInfo(dlg);
                m_surface->SetCurrentLevel(m_curLevel);
                m_surface->Invalidate(FALSE);
            }
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
        return;
    }
    else if (data->kind == TreeNodeData::NK_ToolbarItem)
    {
        auto itTb = resPtr->mapLangAndToolbarInfo.find(data->dlgId);
        if (itTb == resPtr->mapLangAndToolbarInfo.end() || !itTb->second)
            return;
        auto tb = itTb->second;
        size_t idxItem = data->ctrlId;
        if (idxItem >= tb->vecItemIDs.size())
            return;
        if (IsLevelExemptToolbarItem(data->filePath, tb->nToolbarID, tb->vecItemIDs[idxItem]))
        {
            PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
            return;
        }
        if (tb->vecLevelMask.size() < tb->vecItemIDs.size())
            tb->vecLevelMask.resize(tb->vecItemIDs.size(), 0);

        int levelIdx = (m_curLevel > 0) ? (m_curLevel - 1) : 0;
        const UINT bit = (1u << levelIdx);
        if (newChecked)
            tb->vecLevelMask[idxItem] |= bit;
        else
            tb->vecLevelMask[idxItem] &= ~bit;
        // 不强制 SetCheck，保留控件自身的视觉更新以避免闪烁

        if (!tb->m_blIsInLevelManager && tb->vecLevelMask[idxItem] != 0)
            tb->m_blIsInLevelManager = true;

        if (m_surface && m_surface->GetActiveToolbar() && m_surface->GetActiveToolbar()->nToolbarID == tb->nToolbarID)
        {
            m_surface->SetToolbarInfo(tb);
            m_surface->Invalidate(FALSE);
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
        return;
    }
    else if (data->kind == TreeNodeData::NK_DynamicItem )
    {
        // dynamic item behaves like toolbar item but updates dialog's ctrl level mask
        auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
        if (itDlg == resPtr->mapLangAndDialogInfo.end() || !itDlg->second)
            return;
        auto dlg = itDlg->second;
        int idx = (int)data->ctrlId - 1; // ctrlId stored as index+1 for dynamic items
        if (idx < 0 || idx >= (int)dlg->vecCtrlInfo.size())
            return;
        auto c = dlg->vecCtrlInfo[idx];
        if (!c) return;

        int levelIdx = (m_curLevel > 0) ? (m_curLevel - 1) : 0;
        const UINT bit = (1u << levelIdx);
        if (newChecked)
            c->uiLevelMask |= bit;
        else
            c->uiLevelMask &= ~bit;

        if (!dlg->m_blIsInLevelManager && c->uiLevelMask != 0)
            dlg->m_blIsInLevelManager = true;

        // Refresh the toolbar-like preview from dialog
        if (m_surface)
        {
            auto tb = std::make_shared<rceditor::SToolbarInfo>();
            tb->nToolbarID = (int)dlg->nIDD;
            tb->m_blIsInLevelManager = dlg->m_blIsInLevelManager;
            tb->vecItemIDs.clear(); tb->vecItemText.clear(); tb->vecLevelMask.clear();
            for (size_t i = 0; i < dlg->vecCtrlInfo.size(); ++i)
            {
                auto cc = dlg->vecCtrlInfo[i];
                if (!cc) continue;
                tb->vecItemIDs.push_back((uint16_t)cc->nIDC);
                tb->vecItemText.push_back(cc->csText);
                tb->vecLevelMask.push_back(cc->uiLevelMask);
            }
            m_surface->SetToolbarInfo(tb);
            int selIdx = idx;
            if (selIdx >= 0)
                m_surface->SetSelectedToolbarIndices(std::vector<int>{selIdx});
            m_surface->Invalidate(FALSE);
        }

        UpdatePropertyGrid();
        UpdateBadgeUiFromSelection();
        MarkDirty();
        return;
    }

    // 如果受影响的对话框正在 surface 上显示，则刷新 surface
    if (m_surface && m_surface->GetDialogInfo() && data && m_surface->GetDialogInfo()->nIDD == data->dlgId)
    {
        auto itDlg = resPtr->mapLangAndDialogInfo.find(data->dlgId);
        if (itDlg != resPtr->mapLangAndDialogInfo.end() && itDlg->second)
        {
            m_surface->SetDialogInfo(itDlg->second);
            m_surface->SetCurrentLevel(m_curLevel);
            m_surface->Invalidate(FALSE);
        }
    }
    if(m_tree.GetSafeHwnd())
		m_tree.Invalidate(FALSE);
    UpdatePropertyGrid();
    UpdateBadgeUiFromSelection();
    MarkDirty();
}

/*! 
 * \brief 树控件选择变化事件处理，支持搜索结果控件的自动选中
 * \param pNMHDR 通知消息头
 * \param pResult 结果指针
 */
void CFormLevelManagerEditor::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    // 委托给基类行为，但如果选择的是由搜索创建的控件节点（搜索结果的子项），
    // 我们希望确保设计面板选中该控件并滚动至可见。
    if (m_inTreeUpdate)
    {
        if (pResult) *pResult = 0;
        return;
    }

    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    HTREEITEM hItem = pNMTreeView->itemNew.hItem;
    if (!hItem)
    {
        if (pResult) *pResult = 0;
        return;
    }

    TreeNodeData* data = reinterpret_cast<TreeNodeData*>(m_tree.GetItemData(hItem));
    if (data && m_surface)
    {
        m_searchMatchedIndices.clear();
        m_searchMatchedDlgId = 0;
        auto resource = m_openedResources.find(data->filePath);
        if (resource != m_openedResources.end())
        {
            m_pRes = resource->second;
            auto editor = m_openedEditors.find(data->filePath);
            m_pe = editor != m_openedEditors.end() ? editor->second : nullptr;
            m_currentIniPath = data->filePath;
        }
        // When in search mode and node corresponds to a dialog with matched controls,
        // automatically select the first matched control on the design surface.
        if (m_isSearchMode && (data->kind == TreeNodeData::NK_Dialog || data->kind == TreeNodeData::NK_Control))
        {
            // collect matching control indices stored for this dialog in the search tree
            // In our search tree we do not add control child nodes; instead matching controls
            // were used only to decide to show the dialog node. We need to find the matching
            // indices by re-evaluating the dialog's controls against m_searchText.
            auto itRes = m_openedResources.find(data->filePath);
            std::shared_ptr<rceditor::SSingleResourceInfo> resPtr = (itRes != m_openedResources.end()) ? itRes->second : m_pRes;
            if (resPtr)
            {
                auto dlg = FindDialogInResource(resPtr, data->dlgId);
                if (dlg)
                {
                    CString s = m_searchText;
                    s.MakeLower();
                    std::vector<int> matched;
                    for (int i = 0; i < (int)dlg->vecCtrlInfo.size(); ++i)
                    {
                        auto c = dlg->vecCtrlInfo[i];
                        if (!c || IsLevelAuditControlHidden(*c)) continue;
                        CString txt = CString(c->csText).MakeLower();
                        CString cls = CString(c->csClassName).MakeLower();
                        if (txt.Find(s) >= 0 || cls.Find(s) >= 0)
                            matched.push_back(i);
                    }
                    if (!matched.empty())
                    {
                        // store matched indices in member for keyboard navigation
                        m_searchMatchedIndices = std::move(matched);
                        m_searchMatchedDlgId = data->dlgId;
                        // select first matched control
                        m_activeDynamicDialog.reset();
                        m_activeDynamicFilePath.Empty();
                        m_surface->ClearToolbarInfo();
                        m_surface->SetDialogInfo(dlg);
                        m_surface->SetCurrentLevel(m_curLevel);
                        m_surface->EnsureControlVisibleByIndex(m_searchMatchedIndices.front());
                        m_surface->SelectControlByIndex(m_searchMatchedIndices.front(), false, false);
                        UpdatePropertyGrid();
                        UpdateBadgeUiFromSelection();
                        if (pResult) *pResult = 0;
                        return;
                    }
                }
            }
        }
        // 如果这是由搜索创建的控件节点，ctrlId 使用 (index+1) 编码控件索引
        if (data->ctrlId > 0)
        {
            // 切换活动文件/资源
            auto itRes = m_openedResources.find(data->filePath);
            if (itRes != m_openedResources.end())
            {
                m_pRes = itRes->second;
                auto itEditor = m_openedEditors.find(data->filePath);
                m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                m_currentIniPath = data->filePath;
            }
            // 查找对话框并在设计面板上设置
            std::shared_ptr<rceditor::SDialogInfo> dlg;
            if (m_pRes)
            {
                auto itd = m_pRes->mapLangAndDialogInfo.find(data->dlgId);
                if (itd != m_pRes->mapLangAndDialogInfo.end())
                    dlg = itd->second;
            }
            if (dlg)
            {
                m_surface->SetDialogInfo(dlg);
                m_surface->SetCurrentLevel(m_curLevel);
                // 解码索引
                int ctrlIdx = (int)data->ctrlId - 1;
                if (ctrlIdx >= 0 && ctrlIdx < (int)dlg->vecCtrlInfo.size())
                {
                    m_surface->EnsureControlVisibleByIndex(ctrlIdx);
                    m_surface->SelectControlByIndex(ctrlIdx, false, false);
                }
            }
        }
        else
        {
            // 回退到基类处理（对话框/根节点选择）
            CFormRcEditor::OnTvnSelchangedTree(pNMHDR, pResult);
        }
    }

    if (pResult) *pResult = 0;
}

/*! 
 * \brief 属性列表变更处理，调用基类处理后标记数据为已修改
 * \param nIndex 变更的属性索引
 * \param item 属性数据
 */
void CFormLevelManagerEditor::OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item)
{
    // Note: do NOT set `m_inPropUpdate` here because the base implementation
    // checks it to decide whether to apply property changes. We'll only set
    // it temporarily while restoring selection to suppress re-entrant
    // UpdatePropertyList calls triggered by surface changes.

    // Preserve surface multi-selection and toolbar selection across base handling
    std::vector<int> savedCtrlSel;
    std::vector<int> savedTbSel;
    UINT savedDlgId = 0;
    int savedActiveTbId = 0;
    bool hadDialog = false;

    if (m_surface)
    {
        savedCtrlSel = m_surface->GetMultiSelectedIndices();
        savedTbSel = m_surface->GetSelectedToolbarIndices();
        auto curDlg = m_surface->GetDialogInfo();
        if (curDlg)
        {
            hadDialog = true;
            savedDlgId = curDlg->nIDD;
        }
        auto atb = m_surface->GetActiveToolbar();
        if (atb)
            savedActiveTbId = atb->nToolbarID;
    }

    // Preserve the select-all checkbox visual state when modifying multi-selection levels.
    // This prevents UpdateBadgeUiFromSelection from clearing the checkbox during level edits.
    bool prevPreserve = m_preserveBadgeCheck;
    if (savedCtrlSel.size() > 1)
        m_preserveBadgeCheck = true;

    // 调用基类处理实际的属性变更逻辑
    CFormRcEditor::OnPropertyListChanged(nIndex, item);

    // 标记数据为已修改
    MarkDirty();

    // Try to restore surface selection to avoid losing multi-select state when base updated surface
    if (m_surface)
    {
        // Prevent badge check updates while we restore selection
        m_preserveBadgeCheck = true;
        // Temporarily suppress property-change driven updates while we restore
        // surface selection/state to avoid re-entrancy. Save and restore original.
        bool prevInProp = m_inPropUpdate;
        m_inPropUpdate = true;
        // If toolbar was active before, restore toolbar selection.
        // If the surface lost the toolbar binding, try to rebind it first
        auto curAtb = m_surface->GetActiveToolbar();
        if (savedActiveTbId != 0)
        {
            bool restoredTb = false;
            bool restoredIsToolbar = false;
            if (curAtb && curAtb->nToolbarID == savedActiveTbId)
            {
                restoredTb = true;
                restoredIsToolbar = true;
            }
            else
            {
                // Try to find a real toolbar in opened resources matching savedActiveTbId
                for (const auto& kv : m_openedResources)
                {
                    auto res = kv.second;
                    if (!res) continue;
                    auto itTb = res->mapLangAndToolbarInfo.find(savedActiveTbId);
                    if (itTb != res->mapLangAndToolbarInfo.end() && itTb->second)
                    {
                        // bind this resource as active and set toolbar
                        m_pRes = res;
                        auto itEditor = m_openedEditors.find(kv.first);
                        m_pe = itEditor != m_openedEditors.end() ? itEditor->second : nullptr;
                        m_currentIniPath = kv.first;
                        m_surface->SetToolbarInfo(itTb->second);
                        // clear any dynamic dialog preview state
                        m_activeDynamicDialog.reset();
                        m_activeDynamicFilePath.Empty();
                        restoredTb = true;
                        restoredIsToolbar = true;
                        break;
                    }
                }
            }
            if (restoredTb && restoredIsToolbar && !savedTbSel.empty())
                m_surface->SetSelectedToolbarIndices(savedTbSel);
            // Ensure tree checkbox visuals reflect restored toolbar/dialog masks
            if (GetSafeHwnd())
                PostMessage(WM_APP_SYNC_TREE_CHECKS, 0, 0);
        }
        else if (hadDialog)
        {
            auto curDlg = m_surface->GetDialogInfo();
            if (curDlg && curDlg->nIDD == savedDlgId && !savedCtrlSel.empty())
            {
                // Rebuild multi-selection: select first, then ctrl-add others
                int first = savedCtrlSel.front();
                m_surface->SelectControlByIndex(first, false, false);
                for (size_t ii = 1; ii < savedCtrlSel.size(); ++ii)
                {
                    m_surface->SelectControlByIndex(savedCtrlSel[ii], true, false);
                }
            }
        }
        m_surface->Invalidate(FALSE);
        m_preserveBadgeCheck = false;
        // restore property update flag
        m_inPropUpdate = prevInProp;
    }
    // 同步更新属性列表以反映变更（仅一次）
    UpdatePropertyList();

    m_preserveBadgeCheck = prevPreserve;
}
