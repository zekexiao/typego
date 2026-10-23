# TypeGo Plugin API 1.0

日期：2026-10-23
状态：SDK、外部加载器和独立示例已实现，尚未发版；窗口交互需本机验证

## 接入范围

客户仅需 SDK、匹配的 Qt/MSVC 工具链及自己的插件工程，即可独立编译并安装插件，无需下载或修改 TypeGo 宿主源码。第一版提供 Windows x64 的 Qt/C++ 原生接口，支持命令列表、异步查询与执行、QWidget 视图和离开保存检查。

插件与宿主运行在同一进程，具有宿主进程权限。元信息中的能力声明用于兼容协商，不提供安全沙箱；仅安装信任的原生插件。第一版不提供脚本运行时、热卸载、自动插件更新或市场服务。

## SDK 与最小工程

维护者运行仓库根目录 build.bat 后，SDK 产物位于 build/plugin-sdk，并生成可分发的 build/TypeGoPluginSDK-1.0.0-windows-x64.zip。目录包含 include/TypeGoPlugin/Plugin.h、lib/cmake/TypeGoPluginSDK、examples/plugin-demo 和本说明。

客户在已初始化的 x64 MSVC 开发者命令行中运行以下命令。SDK 可放在任意目录；Qt 路径需要指向与宿主匹配的安装。

~~~powershell
cmake -S C:/TypeGoSDK/examples/plugin-demo -B C:/work/plugin-build -G Ninja -DTypeGoPluginSDK_DIR=C:/TypeGoSDK/lib/cmake/TypeGoPluginSDK -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build C:/work/plugin-build
~~~

示例 CMakeLists.txt 仅使用已发布的 CMake 包：

~~~cmake
cmake_minimum_required(VERSION 3.20)
project(MyPlugin LANGUAGES CXX)
find_package(TypeGoPluginSDK 1.0 REQUIRED CONFIG)
typego_add_plugin(my_plugin
    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    SOURCES MyPlugin.cpp)
~~~

实现 QObject 与 TypeGo::PluginApi::Plugin，声明 Q_INTERFACES(TypeGo::PluginApi::Plugin)，并使用 Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")。SDK 的 typego_add_plugin 自动生成该文件、启用 AUTOMOC、使用动态 MSVC 运行库并输出 plugin.dll；源文件中的 Q_OBJECT 需要配合对应的 .moc 包含，完整写法见示例。

initialize(Host*) 在首次使用时执行。仅 initialize 与 shutdown 是必需实现，其他能力可按需覆盖。构造和初始化应轻量；数据库、正文、图片和编辑器继续按需加载。PluginManager、Settings、MainWindow 及 typego_core 均不属于公开 SDK，客户不得依赖这些内部实现。

## 安装、禁用与更新

关闭 TypeGo，将插件包放入以下位置，再通过正常入口启动 TypeGo：

~~~text
%LOCALAPPDATA%/TypeGo/plugins/org.example.myplugin/
    plugin.dll
    assets/...
~~~

目录名必须与元信息 id 完全一致。扫描仅读取 DLL 内嵌的 Qt 插件元数据，不加载 DLL 或执行客户构造函数；命令与设置列表可在未实例化插件时显示。注册失败会在启动时提示错误与路径，首次加载或初始化失败会写入日志；查询会展示错误，执行会返回失败。

设置页可禁用插件。禁用会先协调保存并阻止新调用，已加载二进制保留映射到进程结束。更新或删除插件前必须退出 TypeGo；不要覆盖正在运行的 DLL。卸载插件包不会自动删除独立保存在 %LOCALAPPDATA%/TypeGo/plugin-data/<id>/ 的用户数据。

## 元信息

plugin.json 由客户维护，SDK 在构建时添加 apiMajor、apiMinor 和 abi，无需手写这些兼容字段：

~~~json
{
  "id": "org.example.myplugin",
  "version": "1.0.0",
  "description": "My plugin",
  "requires": ["async", "widgets", "config"],
  "hasSettings": false,
  "commands": [
    {"id": "search", "keyword": "myplugin", "aliases": ["mp"]},
    {"id": "new", "keyword": "myplugin-new", "directActivation": true},
    {"id": "editor", "keyword": "myplugin-editor", "mode": "view", "presentation": "window"}
  ]
}
~~~

| 字段 | 规则与默认值 |
| --- | --- |
| id | 稳定插件身份，也是配置与目录身份；建议反向域名。小写字母开头，后接小写字母、数字，段之间可用点或短横线，最多 128 字符 |
| version | 三段非负数字版本，最多 32 字符；与 API 版本独立 |
| description | 必填显示描述，1–512 字符 |
| requires | 可选数组；当前支持 async、widgets、config。未知必需能力在加载前被拒绝 |
| hasSettings | 可选布尔值，默认 false；为 true 时实现 createSettingsView |
| commands | 1–32 条；每条 id 在插件内唯一，身份规则同插件 id |
| keyword / aliases | keyword 必填；aliases 默认空，最多 16 个；小写字母开头，其余为小写字母、数字或短横线，最多 64 字符；所有入口全局唯一 |
| mode | list（默认）、view、queryView；后者可配合 requiresArguments |
| presentation | embedded（默认）、window、overlay；仅影响视图承载 |
| requiresArguments | 布尔值，默认 false；无参数时保留列表 |
| autoEnter | 布尔值，默认 false；仍受用户的全局自动进入设置控制 |
| directActivation | 布尔值，默认 false；为 true 时快捷键直接执行，适合截图等操作 |

未知可选字段忽略。已知字段类型错误、身份/入口冲突和不兼容包拒绝登记，不留下部分命令。安装后修改包元信息需要重启；首次加载前会重新核对元信息，避免按旧命令表执行已替换的插件。

## 版本与二进制兼容

公开头文件命名空间为 TypeGo::PluginApi，接口 IID 为 org.typego.Plugin/1.0，当前仅接受 API 1.0。API 版本与 TypeGo 应用版本独立。

二进制兼容还要求 Windows x64、相同 Qt 完整版本、相同 MSVC _MSC_VER、动态运行库和相同 Debug/Release 模式。当前仓库构建组合为 Qt 6.10.3、MSVC 1951、/MD、Release；以目标宿主实际构建为准。nativeAbi() 返回编译当前代码的兼容标识，例如 windows-qt-6.10.3-msvc-1951-ptr-8-md-release。SDK 在客户工程生成兼容标识，宿主在实例化前比对。

API 1.0 的既有虚表、结构布局、枚举值不作兼容性修改；未来增加能力使用新的可选 Qt 接口，破坏性改动升级主版本。新版本必须同时更新支持矩阵、迁移说明和兼容测试。在已发布 SDK 中弃用接口时需公布替代方案与支持期限。

## 查询与结果

query(QueryRequest) 接收请求 id、Invocation、上下文和 limit。Invocation 包含稳定 pluginId、commandId、参数、来源和动作信息；不要按输入别名判断应用身份。参数最多 4096 个 UTF-16 字符；查询不得创建文档、截屏或执行 shell 等业务副作用。

同步返回 QueryResult{request.id, items, false}。异步先返回 pending=true，再经 Host::publishQuery 发布相同 id 的结果。每批是完整快照，会替换前一批；最终批 pending=false，最终完成只接收一次。当前宿主只有一个前台查询会话，新查询会取消旧查询。取消后即使底层计算仍运行，旧结果也会被丢弃，插件仍应停止可取消工作。

| ResultItem 字段 | 规则 |
| --- | --- |
| id | 必填稳定结果身份，1–128 字符，同批唯一；异步刷新用于保持选择 |
| title | 必填，1–512 字符 |
| subtitle | 可选，最多 2048 字符 |
| icon | 可选，最多 1024 字符；相对插件包的本地文件，宿主只接受包目录内已有文件；不支持网络地址 |
| actionId | 可选，最多 128 字符；空值表示只展示，有值时才可执行 |
| parameters | 可选标量参数表，执行时原样交给所属插件 |

最多返回 request.limit 条且不超过 200 条。违反字段契约的快照整体转为 InvalidArgument 错误。结果只传摘要、对象 ID 和资源引用，不传正文、原始图像、QObject 或编辑器状态。

parameters 最多 32 项，键长 1–128 字符。值只允许 QString、bool、int、uint、qlonglong、qulonglong 和有限 double；不接受嵌套容器、二进制数组、指针或自定义类型。大小预算为键和值的 UTF-16 字节数、每个数值或布尔值 8 字节，总计不超过 16384 字节。宿主自动绑定来源插件和命令，结果结构没有跨插件路由字段。

QueryResult.error 表示查询失败，失败结果成为最终错误快照。正常完成使用 None，取消通过请求失效处理；取消与后台任务结束是不同事件，插件不能以结果失效代替任务收尾。

## 执行与错误

execute(Invocation) 返回 ExecutionResult。状态为 Succeeded、Failed 或 Pending；宿主行为为 Keep、Hide 或 OpenView，只有成功完成才应用界面请求。Failed 应携带 Error，未提供时按 Internal 处理。

Pending 操作通过 Host::finishExecution(invocation.operationId, result) 最终完成。令牌由宿主分配，不自行生成；最终结果不能再次为 Pending。令牌匹配所属插件，只接收一次。同一 commandId 同时只允许一个待完成操作，每插件最多 8 个；超限返回 Busy。其他内部任务与线程也应自行设置并发和缓存上限，不能每次按键创建线程或无限排队。

错误码与用户可读 message 分开，码值固定；宿主展示信息最多保留 2048 字符。

| 错误码 | 值 | 含义与处理 |
| --- | --- | --- |
| None | 0 | 无错误 |
| InvalidArgument | 1 | 请求、元信息或结果字段不合法；检查契约 |
| Incompatible | 2 | API、ABI、接口或必需能力不匹配；用对应 SDK/工具链重编译 |
| Conflict | 3 | 身份或命令冲突、加载前元信息变化；修正后重启 |
| Disabled | 4 | 保留给禁用状态报告；当前宿主在调用插件前拦截禁用入口 |
| Cancelled | 5 | 操作取消；查询取消结果直接丢弃 |
| Busy | 6 | 命令仍在执行或达到待完成上限 |
| Internal | 7 | 加载、异常或未具体说明的操作失败 |
| IoError | 8 | 数据目录或写入失败；保留内容，修复后重试 |

加载/发现诊断同时记录路径、插件身份与错误码。宿主内部适配层将执行失败显示为 [code] message；客户插件直接使用结构化 Error，不需要解析显示字符串。普通 C++ 异常在接口边界转为 Internal；原生崩溃、越界访问等不受此保护。

## 线程、回调和宿主服务

所有 SDK 方法、Host 调用及 QWidget 操作均在宿主 GUI 线程执行。耗时计算与 I/O 放到有界后台任务；完成时使用 QMetaObject::invokeMethod(pluginQObject, functor, Qt::QueuedConnection) 投递回插件 QObject，再调用 Host。跨线程直接调用 Host 会被拒绝。

允许在 query、execute 或 prepareToLeave 尚未返回时立即调用对应完成方法，但此时必须返回 Pending。宿主排队处理完成通知；过期、错误令牌和重复最终通知均被丢弃。不要通过 processEvents 或嵌套事件循环等待任务完成。

Host 从 initialize 起有效，shutdown 返回后失效；插件不得删除它、删除 Qt 插件根对象或改变根对象线程。shutdown 返回前停止并等待后台任务、撤销后续投递并清除 Host 引用。正常退出的保存等待放在 prepareToLeave，shutdown 负责最后的同步资源收尾，不能再启动异步保存。

| Host 方法 | 行为 |
| --- | --- |
| directory(Package) | 当前插件安装目录，只用于读取包资源 |
| directory(Data) | 持久数据目录，按需创建；不可用时返回空字符串 |
| directory(Cache / Temporary) | 插件专属可清理目录，按需创建；插件自行清理临时资源 |
| configValue / setConfigValue | 插件专属 config.ini，使用 ini 后端；写入同步并返回错误，标量值规则同 parameters |
| log | 带插件身份记录日志，单条消息截至 2048 字符 |
| publishQuery / finishExecution / finishLeave | 带原始请求令牌完成对应操作 |
| closeView | 请求宿主走正常离开检查，不能代替保存或自行销毁窗口 |

## 视图与保存

createView(invocation, parent) 必须返回 GUI 线程中、以传入 parent 为父对象的 QWidget，宿主持有并销毁。activate 在显示前调用；首次激活可能早于创建界面，已缓存界面重新进入时也会再次激活。插件只保留 QPointer 等观察引用，不删除宿主视图。createSettingsView(nullptr) 返回新的无父对象设置视图，设置对话框随后接管并在关闭时销毁。

每插件当前只缓存一个主视图；多文档可在内部使用标签页。同一插件同时显示多种承载方式、任意多窗口、窗口几何持久化和空闲定时回收尚不支持。Overlay 提供全屏承载，屏幕捕获、多屏 DPI 和焦点恢复由截图业务实现。

prepareToLeave(reason, token) 的原因区分 Navigate、Hide、Disable 和 Shutdown。返回 Ready 表示内容和任务已能安全离开；Refused 保留现状，插件应通过自己的视图或日志说明保存失败原因；Pending 等待 Host::finishLeave(token, accepted)。同原因请求合并，其他原因等待当前离开结束后重试。

保存失败不得报告 Ready；禁用或正常退出被拒绝时，宿主保留插件和编辑内容。示例 sdk-editor 使用 QSaveFile 保存后才允许关闭，sdk-job 展示后台操作期间等待离开。强制终止与系统结束不能依靠异步退出协议保证保存，需要业务草稿恢复。客户端更新安装的退出协调、既有待办业务服务全部落盘错误尚未统一接入，不能据此承诺所有内部模块的数据保护已完成。

## 示例与验证

示例提供 sdk-demo（同步查询，输入 async 验证异步）、sdk-job（异步执行）和 sdk-editor（独立文本编辑器，离开保存）。选中查询结果会将文本写入插件配置，示例不会操作用户现有笔记。

仓库统一使用 test.bat 和 build.bat。测试会先安装 SDK，再以独立 CMake 工程编译示例，仅依赖已安装的 SDK 与 Qt，不包含宿主头文件或链接 typego_core。测试在 QTemporaryDir 与独立 ini 中发现并加载真实 DLL，覆盖元信息兼容、冲突、禁用、懒加载、参数限制、非法结果、过期回调、立即完成、重复操作、保存拒绝和退出等待。

界面不在自动化覆盖内，需在本机通过 run.bat 检查 sdk-editor 的编辑、关闭、保存失败后保留内容、禁用与托盘退出，以及设置页、独立窗口和覆盖层交互。截图和笔记应用本身尚未加入。
