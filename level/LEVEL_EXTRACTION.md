# Level 提取与优化记录

更新时间：2026-10-11  
来源工程：`G:\999-image\work\Avi\Avi635D\Avi635D`  
来源目录：`AviBaseClass\CLevelManagerControler.h/.cpp` 及 `ImageAVI` 的调用点

本文件记录本轮“把项目内代码复制到库中作为接口/方法”的内容。凡是从原项目的函数语义、状态规则或调用顺序提取到 `GovernLens/level` 的项目，都在下表登记；没有登记的原文件没有被复制进核心库。

## 聚合头文件

本轮已将 `635D_UserEdit_LevelRange_Pack/01_AviBaseClass_Level` 中的 41 个等级头文件，以及原编辑器直接依赖的 `CPropertyListCtrl.h`、`TabSheet.h`，按资源模型、配置审计、编辑器、等级页面、用户授权和总控制器分类，合并到 [level.h](<G:/999-image/work/Avi/Avi635D/Avi635D/GovernLens/level/include/level.h>)。`ParameterEditorStyle.h` 与 `CFormRcBase.h/.cpp` 已从聚合头拆到 `GovernLens/level/ui`，`level.h` 通过相对 include 继续提供兼容入口。

## 已复制并优化的部分

| 原项目代码 | GovernLens 目标 | 处理方式 |
| --- | --- | --- |
| `CLevelManagerControler::AddRcFileName`（`AviBaseClass/CLevelManagerControler.cpp:331`） | `LevelManager::AddRcFileName` | 保留旧的文件名/目录配置入口；增加 `moduleKey`，未传时从文件名去路径但保留 `.dll/.exe` 后缀生成，与 JSON `BinFileName` 对齐，避免旧调用链因键为空无法启动。 |
| `SetJsonFilePath`（362）及 `ClearRcPaths`（370） | 同名 `LevelManager` 方法 | 保留原配置构建顺序；运行中拒绝修改，配置替换时清理动态定义和控件状态，避免跨项目残留。 |
| `SetInterfaceLan`（721）、`RefreshLanguageResources`（855） | 同名方法及 `ILevelHost::RefreshLanguageResources` | 语言 DLL 的加载/释放留给宿主，库负责语言值传递和缓存失效。 |
| `SetIsAdmin`（891）、`SetCurUserLevel`（898） | `UserIdentity`、两个 setter | 登录身份不再读取 `CSetting/CUser`；身份变化后重新计算已绑定动态控件，保留原登录后刷新效果。 |
| `StartLevelManager`（1343）、`EnableLevelManager`（1364）、`StopLevelManager`（1373） | 同名生命周期方法 | 保留启动、暂停、停止状态；资源加载、JSON、Hook 和释放改为宿主回调，失败时统一进入 `Failed` 并释放已加载资源。 |
| `ShowLevelManagerDlg`（675） | `LevelManager::ShowLevelManagerDlg` + `ILevelHost::ShowEditor` | 保留“编辑器确认后保存”的闭环；去掉 `AfxGetMainWnd`、`CLocalResource` 和全局语言变量。 |
| `EnsureOpenedResourcesLoaded`（996） | `ILevelHost::LoadResources`、`UnloadResources` | 原 PE/资源解析逻辑没有在库内重写成假实现，宿主直接复用 `RcCore/FormRcEditor` 现有代码。 |
| `RegisterDynamicDialog`（116） | `DynamicDialog`/`DynamicControl` DTO + `RegisterDynamicDialog` | 保留动态页合并入口；增加 `moduleKey` 作为资源归属键，并在运行时注册后清缓存。 |
| `CanAccessControl`（407）、`CanAccessDynamicControl`（2107） | `CanAccessControl`、`CanAccessDynamicControl` | 保留管理员放行、未启动/暂停放行和 `(0,0)` 顶部 Tab 放行；动态定义优先，静态资源查询交给宿主。 |
| `ApplyDynamicControlLevel`（2115） | `ApplyDynamicControlLevel` | 保留窗口存活检查和等级状态绑定；支持带模块键重载，旧无键调用保留唯一 ID 回退。 |
| `SetBusinessEnableState`（2137）、`ApplyMergedState`（2196） | `SetBusinessEnableState` + 内部 `ApplyMergedState` | 直接复制“等级允许 AND 业务允许”的最终启用规则，控件句柄和 MFC `CWnd` 转换留给宿主。 |
| `ImageAVI/imageavi.cpp:354-365` | `RuntimeConfig`、配置 setter、`StartLevelManager` | 复制 635D 的资源模块、中文语言和 JSON 初始化顺序。 |
| `ImageAVI/imageavi.cpp:452-461` | 语言刷新回调 | 复制切换语言后清理/加载 DLL、切换 JSON、刷新等级缓存的边界；DLL 路径和 fallback 仍由宿主处理。 |
| `AviBaseClass/avibasedlg.cpp:1116-1120` | `SetIsAdmin`、`SetCurUserLevel` | 复制登录完成同步等级身份的入口，不再依赖 `CSetting`。 |
| `AviBaseClass/usereditdlg.cpp:223-235` | `ShowLevelManagerDlg` | 复制用户编辑窗口进入等级编辑器的功能入口。 |
| `ImageAVI/DocSettingSheet.cpp:261`、`debugbar.cpp:1192-1214`、`rgbstatisticctrl.cpp:175` | 动态注册、等级查询、控件状态合并接口 | 复制动态页、工具栏 Tab、RGB 控件的接入语义；MFC 句柄和文本转换由适配器完成。 |

## 有意留在宿主适配器的部分

以下代码没有直接搬进通用核心，因为它们绑定 635D 的资源格式、窗口消息或项目策略；宿主可以原样复用，并通过 `ILevelHost` 接入：

- `RcCore.cpp/.h`、`RcEditorModel.cpp/.h`：PE/资源解析、JSON 模型和资源对象生命周期。
- `FormRcEditor.cpp/.h`、`FormLevelManagerEditor.cpp/.h`、`CDesignSurfaceWnd.cpp/.h`：MFC/BCG 编辑器和 UI 线程。
- `LevelLocalize.h`、`LevelAuditIgnore.h`：语言资源和 ImageAVI/AviBaseClass 的模块、对话框、工具栏白名单。后者不能作为跨项目默认策略，应改为宿主策略回调。
- `user.cpp/.h`、`logindlg.cpp/.h`、`usereditdlg.cpp/.h`、各 `*Level.cpp/.h` 旧页：依赖 `CUser`、`CSetting`、`AviBaseDlg` 和 MFC 资源，作为兼容层保留。
- `InstallHook/UninstallHook` 及 `WM_ENABLE` 处理：核心只调用 `AttachRuntime/DetachRuntime`；宿主负责 UI 线程、窗口生命周期和重入保护。

## JSON 和资源键约定

原 JSON 根结构仍按 `BinFileName`（资源模块）包含 `WndInfos`、`DynamicWndInfos`、`ToolbarInfos` 等定义。库新增的 `ResourceModule.moduleKey` 对应 `BinFileName`，不再按“第一个打开的资源”猜测。使用多个 DLL 且对话框 ID 重复时，动态接口必须携带 `moduleKey`。

## 当前验证边界

- 已验证：`LevelManager` 的配置、启动、资源/JSON 回调、动态注册、管理员/等级状态、等级与业务状态合并、编辑器确认保存、停止释放；C++11 `-Wall -Wextra -Wpedantic` 合同测试通过。
- 尚未声称：真实 635D MFC 适配器已经接入、真实 PE/DLL 资源加载成功、真实编辑器窗口显示成功或现场语言切换成功。上述证据需要在宿主适配器落地后按 `INTEGRATION.md` 验证。
- `CFormNoRcBase` 与 `ParameterEditorStyle` 已按 635D 原文件复制到 `GovernLens/level/ui`；它们仍属于编辑器 UI 依赖，不计入 Level 核心业务实现。
- `ui/mfc/LegacyMfcLevelHost.*` 已提供 635D 的直接复用适配器：资源/JSON/语言/编辑器/Hook/动态页/控件操作均转调原 `CLevelManagerControler`。它需要在原 MFC 工程中编译，当前 C++11 合同测试不编译该适配器。
- 旧 `paraleveldlg.cpp:56` 的 `IDD_RINGHOLE_LEVEL` 注册错误、`CParaLevel::Copy` 未复制选择索引等旧页风险不属于新核心的默认行为，接入旧页时应单独修复并追加本文件记录。

## 通用工具迁移

本轮将聚合头文件中与业务模型无关的辅助代码移到 `GovernLens/utils`：

- `TextConversion.h/.cpp`：`CStringA`、`CStringW`、`CString`、UTF-8 与本地代码页之间的转换，以及 CString 哈希/相等比较；`level.h` 的 `ML` 兼容入口委托给它。
- `JsonUtils.h/.cpp`：JSON 文件加载、保存和安全整数读取；JSON 字段中的文本编码转换统一调用 `TextConversion`。
- `ResourceUtils.h`：资源缓冲区偏移量对齐等无状态资源辅助函数。

`level.h` 通过相对 include 继续提供字符串工具的兼容性；JSON 使用方按需 include `GovernLens/utils/JsonUtils.h`，新增目标也可以直接按需 include这些工具头。

