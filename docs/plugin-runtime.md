# 内置插件运行时接入约定

日期：2026-10-23
范围：宿主源码内部的当前实现；截图、笔记功能本身尚未加入

本文面向 TypeGo 维护者，接入方式需要修改并重新编译宿主。客户插件的公开接口要求见 [plugin-api.md](plugin-api.md)；本文中的内部类型与登记方法不构成 SDK 兼容承诺。

## 登记和身份

在 `src/plugins/BuiltinPlugins.cpp` 登记 `PluginDescriptor` 与工厂。新增插件使用 `registerPlugin(descriptor, factory)`，不在 `main.cpp` 提前创建实例。

- `PluginDescriptor.id` 是稳定应用身份，同时用于启用设置；不能随命令改名而更换。
- `PluginCommand.id` 是应用内部的稳定操作身份。`keyword` 是输入词，`aliases` 是同一操作的等价入口。
- 一个应用可以有多条不同命令。插件通过 `PluginInvocation.commandId` 分流业务，不需要宿主识别插件名字。
- 描述、是否有设置页、自动进入和呈现方式由元信息声明；读取它们不会创建实例。
- 工厂接收 QObject 父对象，管理器持有运行实例；工厂和构造函数均应轻量。数据库、正文、图片和编辑器继续按需加载。
- `PluginDescriptor.background` 为 true 时管理器在 `startBackgroundPlugins()`（启动）和插件重新启用时调用 `activate`，用于剪贴板监听这类必须持续工作的插件；禁用或退出时插件应在 `prepareToLeave` 停止后台工作。其余插件仍保持首次使用才创建。

已有插件保留 `registerPlugin(IPlugin *)`、字符串查询及 map 动作的适配接口。旧插件的 `command()` 和 `description()` 暂时仍是 `IPlugin` 的必需接口，登记元信息需与之保持一致。

## 查询

实现 `query(const PluginQueryRequest &request)`。请求包含会话 ID、完整调用身份、输入参数、上下文与结果上限。

同步结果返回 `{request.id, items, false}`；异步结果先返回 `{request.id, placeholderItems, true}`，随后发出 `queryFinished({request.id, items, false})`。异步批次以完整快照替换上一批，不是追加；中间快照可保持 `pending = true`。

管理器只接受当前查询、正确插件及启用状态匹配的结果，默认最多 200 条，上限 1000 条。即使查询文字相同，新会话也不能复用旧 ID。`cancelQuery()` 使旧结果失效，插件应停止可取消任务；不能中断的底层操作允许结束，但不得回填旧结果。

目前宿主只有一个前台查询会话。后台全文索引等长期任务由业务服务管理，不能占用这个会话。Everything 保持单任务运行和一个最新待查请求，SDK 全局状态不能并发调用。

在后台线程做耗时计算与 I/O，GUI 线程只接收结果和更新视图；不要直接在工作线程操作 QWidget。查询不得截图、创建文档或执行 shell 等产生业务副作用。

结果使用现有 QVariantMap 契约：`id`、`title`、`subtitle`、`icon`、`action` 等。提供稳定 `id` 有利于异步更新保留选择。只传摘要、文档 ID 或资源引用，不传全文、原始图像和编辑器对象。存储层分页需要应用另行实现，当前结果上限不等于数据库分页。

## 执行

实现 `execute(const PluginInvocation &invocation)`，返回 `PluginExecutionResult`。

| 字段 | 含义 |
| --- | --- |
| `Status::Succeeded` | 本次同步操作完成 |
| `Status::Failed` | 失败，`error` 可提供可读说明 |
| `Status::Pending` | 异步处理中，不立即改变宿主界面 |
| `HostAction::Keep` | 保留启动器 |
| `HostAction::Hide` | 请求保存检查通过后隐藏启动器 |
| `HostAction::OpenView` | 打开该调用对应的应用界面 |

异步完成用原 `invocation.operationId` 发出 `executionFinished(token, result)`。不要自行生成令牌；管理器会校验所属插件并只接收一次最终结果。禁用和正常退出会清除待处理动作。

`directActivation = true` 的命令可以从快捷键直接执行，不先显示启动器；适用于截图等入口。普通列表插件继续通过选中结果的动作执行。新建文档等动作应在明确执行后发生。

当前基座不代替业务模块实现任务并发配额、取消和去重。异步命令必须限制自身运行数量，重复触发时复用或拒绝已有操作，不得无限创建任务。

## 界面与所有权

`PluginCommand.presentation` 支持 `Embedded`、`Window`、`Overlay`。列表由 `LaunchMode::List` 表示；`FullView` 和带参数的 `QueryView` 可以请求界面。独立窗口不继承主窗口失焦隐藏设置。

实现 `createView(QWidget *parent)` 并使用宿主传入的父对象；宿主统一管理界面销毁。旧 `viewUrl()` 仍由默认实现适配。插件使用 QPointer 或销毁信号维护观察引用，不能让原始缓存指针在界面销毁后失效。

`activate(invocation)` 在显示前调用，旧插件适配为 `activateCommand()` 和 `activateQuery()`。`deactivate()`、`releaseInactiveResources()` 允许退出当前界面时清理工作状态与可丢弃缓存；不能直接删除仍由宿主缓存的 QWidget。

当前每个应用只缓存一个界面，不应声明需要同时使用的多种呈现方式，也不应在多个窗口之间移动同一个 QWidget。多文档优先在应用窗口内部以标签页承载。独立窗口大小目前为默认值，没有几何持久化；覆盖层只提供全屏承载，截屏、多屏 DPI 与焦点恢复仍由截图模块实现。

## 离开、禁用和退出

实现 `prepareToLeave(reason, token)`，原因区分返回导航、隐藏、禁用、退出。

- 没有待保存工作时返回 `Ready`。
- 保存失败或不能离开时返回 `Refused`，保留编辑内容，并向用户说明如何恢复。
- 异步保存返回 `Pending`，完成后发出 `leaveFinished(token, accepted)`。

相同原因的离开请求会合并，不同原因在当前离开结束前被拒绝。异步回调必须使用原令牌，不能让旧保存结果批准新的离开请求。正常流程不依赖 `aboutToQuit` 才启动异步保存。

设置页通过 `PluginManager::setPluginEnabled()` 修改启用状态；不要直接写 Settings 绕过离开检查。保存拒绝后保留启用状态并恢复快捷键。托盘退出与重启使用 `requestShutdown()`，只询问已创建的实例，不会为退出初始化尚未使用的应用。

大型插件接入前，仍需补齐后台进程的退出收尾、更新安装退出协调和资源回收，并针对这些路径做专项验证。

## 验证

纯逻辑与协议用 Qt Test，入口使用 `QTEST_GUILESS_MAIN`。测试通过 `QTemporaryDir` 和独立 ini 隔离数据，不能访问真实用户目录和注册表。

统一运行 `test.bat` 与 `build.bat`，界面通过 `run.bat` 在本机验证。当前协议测试覆盖懒加载、命令冲突与身份、禁用入口、查询取消及令牌校验、异步动作完成、离开拒绝与合并、退出等待和工厂重入。
