# GovernLens Level

`GovernLens/level` 是从 635D `CLevelManagerControler` 提取并整理出的等级管控核心。提取范围保持原项目的业务顺序和方法语义，再把资源、JSON、语言 DLL、MFC 窗口和编辑器等项目依赖移到 `ILevelHost`，使核心库可以被 ImageAVI、ImageServer 或其他 MFC 工程复用。

原 Level 相关头文件现在已经全部合并到 [level.h](<G:/999-image/work/Avi/Avi635D/Avi635D/GovernLens/level/include/level.h>)，按资源模型、配置审计、编辑器、等级页面、用户授权和总控制器分类。新项目先包含这个入口即可看到原等级模块的完整声明，后续再从聚合文件中精炼掉项目耦合。

核心库使用 C++11，生成静态库 `GovernLensLevel`。核心层不包含 `afxwin.h`、`Setting.h`、`resource.h`、`CWnd`、`BCG` 或任意 635D 工程全局变量。MFC 代码应放在宿主适配器中，通过 `LevelHostCallbacks` 或 `ILevelHost` 接入。

## 已覆盖的 Level 闭环

调用顺序与原项目一致：

1. 宿主配置资源模块、JSON 路径和界面语言。
2. `StartLevelManager` 调用宿主加载资源和 JSON，安装宿主自己的 UI 事件钩子，并注册启动前登记的动态对话框。
3. 登录完成后调用 `SetIsAdmin`、`SetCurUserLevel`；已绑定动态控件会重新计算等级状态。
4. 宿主为动态页调用 `RegisterDynamicDialog`，控件调用 `ApplyDynamicControlLevel`；业务代码调用 `SetBusinessEnableState`。
5. 库按“等级允许且业务允许”合成最终 `SetControlEnabled` 状态。`(dialogId, controlId) == (0, 0)` 保留 635D 顶部 Tab 的放行语义。
6. `ShowLevelManagerDlg` 委托宿主显示编辑器，编辑器确认后统一保存 JSON 并清理对话框缓存。
7. 切换语言调用 `SetInterfaceLan` 和 `RefreshLanguageResources`；停止时调用宿主解绑和卸载资源。

## 最小接入示例

下面的回调只展示接入边界，具体 MFC 实现仍由项目负责。`oldController` 代表原工程中的资源/编辑器兼容对象，不能在核心库中直接引用。

```cpp
GovernLens::Level::LevelHostCallbacks callbacks;
callbacks.loadResources = [&](const RuntimeConfig& config, std::wstring& error) {
    // 复用原项目的 PE/资源加载、CLocalResource 和 opened resources 逻辑。
    return oldController.LoadResources(config, error);
};
callbacks.loadConfiguration = [&](const std::wstring& path, std::wstring& error) {
    return oldController.LoadJson(path, error);
};
callbacks.saveConfiguration = [&](const std::wstring& path, std::wstring& error) {
    return oldController.SaveJson(path, error);
};
callbacks.showEditor = [&](const RuntimeConfig& config,
                           const UserIdentity& identity,
                           std::wstring& error) {
    return oldController.ShowEditor(config, identity, error);
};
callbacks.attachRuntime = [&](std::wstring& error) {
    // 在宿主 UI 线程安装原 CLevelManagerControler 的消息/窗口钩子。
    return oldController.AttachRuntime(error);
};
callbacks.detachRuntime = [&]() { oldController.DetachRuntime(); };
callbacks.setControlEnabled = [&](ControlHandle handle, bool enabled) {
    return oldController.SetControlEnabled(handle, enabled);
};

std::shared_ptr<ILevelHost> host(new CallbackLevelHost(callbacks));
LevelManager level(host);
level.ClearRcPaths();
level.AddRcFileName(L"AviBaseClass.dll");
level.AddRcFileName(L"ImageAVI.exe");
level.SetJsonFilePath(L"Setting\\ImageAVI635Level.json");
level.SetInterfaceLan(0);
level.StartLevelManager(error);
```

多 DLL 项目应在 `DynamicDialog.moduleKey` 中填写对应的 `ResourceModule.moduleKey`（默认取资源文件名本身，例如 `AviBaseClass.dll`），并使用带 `moduleKey` 的 `CanAccessControl` / `ApplyDynamicControlLevel` 重载。无 `moduleKey` 的重载保留旧项目兼容性：同一 `dialogId` 只有一个动态定义时自动回退；多个模块 ID 重复时要求宿主改用带模块键的重载。

## 目录

- `include/GovernLens/Level/LevelTypes.h`：资源、动态页、身份、生命周期 DTO。
- `include/GovernLens/Level/LevelHost.h`：MFC/资源/JSON/编辑器/控件操作的宿主接口。
- `include/GovernLens/Level/LevelManager.h`：从 `CLevelManagerControler` 提取的可复用门面。
- `src/LevelHost.cpp`、`src/LevelManager.cpp`：不依赖 MFC 的实现。
- `ui/LevelUiBridge.h`：MFC 工程绑定说明和回调容器。
- `ui/mfc/LegacyMfcLevelHost.*`：直接复用 635D `CLevelManagerControler` 的 MFC 兼容适配器，不进入框架无关静态库目标。
- `tests/LevelManagerContractTest.cpp`：框架无关的 C++11 闭环合同测试。
- `../utils/TextConversion.*`、`../utils/JsonUtils.*`、`../utils/ResourceUtils.h`：MFC 字符串/UTF-8 转换、等级 JSON 读写、安全数值读取和资源偏移量对齐。
- `LEVEL_EXTRACTION.md`：逐项记录原项目代码复制到接口/方法的来源、改写和未迁移边界。
- `INTEGRATION.md`：635D 接入顺序、宿主责任和验证清单。

资源解析器、`RcCore`、`FormRcEditor`、`FormLevelManagerEditor` 和 `LevelAuditIgnore` 没有伪装成通用实现；它们仍由宿主回调提供。这样可以直接复用原 MFC 代码，同时避免把 ImageAVI 的模块名、资源 ID 和全局状态固化进库。
