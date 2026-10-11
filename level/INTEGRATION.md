# 635D Level 接入清单

本清单用于把现有 MFC 代码接到 `GovernLens::Level::LevelManager`。核心库已经提供状态闭环，真正的资源和窗口行为仍需要 635D 宿主完成。

若目标是先在 635D 中快速复用原实现，可直接使用 `ui/mfc/LegacyMfcLevelHost.h/.cpp`。它把 `CLevelManagerControler` 注入为宿主并转调原资源解析、编辑器、语言刷新、动态页和 Hook；后续再把这些回调逐项替换为跨项目实现。

编译宿主适配器时，将 `GovernLens/utils/TextConversion.cpp` 和 `GovernLens/utils/JsonUtils.cpp` 加入目标；`JsonUtils.cpp` 继续链接 635D 使用的 `json_vc71` 库。只使用 `level.h` 的资源模型目标无需引入 JSON 工具头。

在 635D 的 MFC 工程中，接入对象关系可以保持原来的全局设置对象：

```cpp
CLevelManagerControler& oldController = g_AviData.m_pSetting->m_oLevelManagerControler;
std::shared_ptr<ILevelHost> host(new LegacyMfcLevelHost(oldController));
LevelManager level(host);
```

这样第一阶段只替换 `m_oLevelManagerControler` 对外调用的门面；`RcCore`、`FormLevelManagerEditor` 和原 Hook 仍然由旧控制器直接执行，便于先完成行为对齐，再拆出跨项目实现。

## 宿主适配器责任

1. `LoadResources`：复用 `CLevelManagerControler::EnsureOpenedResourcesLoaded`、`RcCore`、`FormRcEditor`，按 `RuntimeConfig.resourceModules` 打开资源，并保持 `BinFileName/moduleKey` 对应关系。
2. `LoadConfiguration` / `SaveConfiguration`：复用 `GovernLens/utils/JsonUtils` 或现有 JSON 模型，读写 `RuntimeConfig.jsonFilePath`。
3. `ShowEditor`：在 MFC UI 线程构造 `FormLevelManagerEditor`，把 `UserIdentity` 转成原编辑器需要的管理员/等级参数，返回 `Accepted/Canceled/Failed`。
4. `RefreshLanguageResources`：复用 `CImageAVIApp::ApplyLanguageResources` 的 DLL 加载/释放和 JSON 切换逻辑；缺 DLL、fallback、资源句柄恢复必须在宿主处理。
5. `AttachRuntime/DetachRuntime`：复用旧控制器的窗口消息/线程 Hook，但由宿主确保安装线程和销毁顺序。
6. `CanAccessResourceControl`：按 `moduleKey + dialogId + controlId` 查询 `RcCore` 静态对话框和工具栏定义；未找到定义时保持旧逻辑默认放行。
7. `RegisterDynamicDialog`：把 `DynamicDialog` 转成 `SDialogInfo/SCtrlInfo`，复用原动态页合并逻辑；保留 JSON 优先规则。
8. `IsControlAlive/SetControlEnabled`：把 `ControlHandle` 映射到 `HWND/CWnd`，执行 `IsWindow` 和 `EnableWindow`，不要在核心层保存 `CWnd*`。
9. `InvalidateDialogCache/UnloadResources`：分别清理资源匹配缓存和释放宿主资源。

## 推荐迁移顺序

先在 ImageAVI 中建立 `LegacyMfcBinding`，只迁移 `CLevelManagerControler` 的资源、JSON、编辑器和控件操作回调；保留原调用点。然后把 `m_oLevelManagerControler` 的初始化替换为 `LevelManager`，按以下顺序接入：

```text
imageavi.cpp 初始化
  -> ClearRcPaths/AddRcFileName/SetJsonFilePath/SetInterfaceLan
  -> StartLevelManager
登录完成
  -> SetIsAdmin/SetCurUserLevel
动态页创建
  -> RegisterDynamicDialog
控件刷新
  -> ApplyDynamicControlLevel + SetBusinessEnableState
用户打开等级编辑
  -> ShowLevelManagerDlg
切换语言
  -> SetInterfaceLan + RefreshLanguageResources
应用退出或关闭功能
  -> StopLevelManager
```

## 验证门槛

- 静态：每个资源文件都有明确 `moduleKey`；多个模块的相同 `dialogId` 使用带模块键接口。
- 启动：日志能看到资源加载、JSON 加载、Hook 安装、动态页注册四步，任一步失败都能释放已加载资源。
- 身份：管理员全放行；普通用户按 `LevelMask` 检查；登录等级改变后已绑定控件即时刷新。
- 状态：等级禁用和业务禁用同时存在时保持禁用；业务恢复不能覆盖等级禁用；`(0,0)` Tab 不因没有动态定义而被误禁用。
- 编辑：打开编辑器、确认保存 JSON、清缓存后重新查询权限；取消不保存。
- 语言：至少验证中文和一个外语 DLL 的加载、JSON 切换、资源句柄恢复、动态文本刷新和 fallback。
- 停止：解绑 Hook、释放 DLL/JSON/资源对象、清理控件状态；再次启动不能带入旧项目动态定义。

本目录的合同测试只证明核心接口和状态规则，不替代上述 MFC/PE/现场验证。
