# TypeGo 更新日志

## 1.9 - 2026-10-29

### 中文

- 新增剪贴板历史插件：记录复制的文字、图片和本地文件，输入 `clip`（`cb`、`paste`）搜索，回车重新复制；文字和文件显示 T / F 图标，图片显示缩略图
- 剪贴板监听随 TypeGo 启动运行，设置页可调整历史条数、分别开关文字/图片/文件记录并清空历史；禁用插件或退出后停止监听
- 新增 JSON / MessagePack 查看器插件，支持内嵌浏览、搜索、编辑与分离窗口
- 插件支持随安装包分发的内置包和内嵌视图分离窗口，分离后保留编辑状态
- 统一界面视觉基线：拆分强调色的描边、填充与文字角色以满足 WCAG AA，提高占位文字对比度，统一字号、间距与设置分组
- 标题栏关闭、返回、分离图标改为代码绘制并居中窗口标题，修复快捷键输入框内边距与聚焦时的行高跳动

### English

- Added the clipboard history plugin: records copied text, images and local files; search with `clip` (`cb`, `paste`) and press Enter to copy again. Text and files show T / F icons, images show thumbnails
- Clipboard monitoring starts with TypeGo; settings adjust the history size, toggle text/image/file recording and clear history, and disabling the plugin or exiting stops monitoring
- Added a JSON / MessagePack viewer plugin with embedded browsing, search, editing and a detachable window
- Plugins can now ship as bundled packages, and embedded views can detach into a separate window while keeping their state
- Aligned the interface with a shared visual baseline: split accent colors into stroke, fill and text roles for WCAG AA, raised placeholder contrast, and unified type sizes, spacing and settings groups
- Title bar close, back and detach glyphs are drawn in code with a centered window title; fixed shortcut field padding and row height shifts on focus

## 1.8 - 2026-10-28

### 中文

- 修复快捷键设置输入框仍显示英文提示的问题，改为显示“按下快捷键”
- 改进更新安装交接：等待当前程序退出后再启动安装包；安装未完成时重新打开 TypeGo

### English

- Fixed the shortcut settings field showing an untranslated English prompt
- Improved update handoff by waiting for TypeGo to exit before launching the installer and reopening TypeGo if installation does not complete

## 1.7 - 2026-10-28

### 中文

- 移除内置 Todo 管理、本地存储及相关测试，专注应用、文件与命令快速启动
- 移除遗留服务器代码与运行入口，精简构建依赖、界面资源和维护文档
- 更新官网介绍与开发清单，聚焦快速启动体验

### English

- Removed built-in todo management, local storage, and related tests to focus on launching apps, files, and commands quickly
- Removed legacy server code and its run command, simplifying build dependencies, UI resources, and project documentation
- Updated website copy and development priorities around the launcher experience

## 1.6 - 2026-10-24

### 中文

- 修复目录输错时悄悄跳回用户主目录的问题：无效或不可读路径明确提示，相对路径统一从用户主目录解析
- 优化大目录浏览：仅保留需要显示的排序结果，降低内存占用，支持取消过时查询，并提示结果截断
- 优化大量待办任务的完成、删除与撤销操作，子树遍历不再逐节点扫描全部任务
- 修复托盘重启的单实例竞争：新进程等待旧进程退出后启动；启动失败时保留当前实例并提示
- 修复多屏悬浮图标位置恢复：支持负坐标副屏，屏幕拔插或可用区域变化时自动校正位置
- 固定应用仅在保存成功后更新界面；失败保留原列表并提示，损坏的固定数据不再被默认项覆盖
- 补齐新增提示的中文翻译，并扩展目录、存储失败、多屏坐标、进程交接及大量待办的自动化测试

### English

- Fixed invalid directory input silently falling back to the home folder: invalid or unreadable paths now show an error, and relative paths consistently resolve from the user home directory
- Reduced memory use when browsing large directories by retaining only the displayed sorted entries, cancelling stale queries, and indicating truncated results
- Optimized completion, deletion, and undo for large todo subtrees without repeatedly scanning all tasks for each node
- Fixed the tray restart race: the new process waits for the previous process to exit; launch failures keep the current instance running with an error message
- Fixed floating icon placement across multiple monitors, including negative coordinates and automatic repositioning when screens or available geometry change
- Pinned apps now update only after a successful save; failures preserve the previous list and show feedback, while malformed data is protected from being overwritten by defaults
- Added Chinese translations for new feedback and expanded automated coverage for directories, storage failures, screen positions, process handoff, and large todo lists

## 1.5 - 2026-10-23

### 中文

- 修复共享网络环境下检查更新报 403 失败：更新检测改用 GitHub 页面接口，不再受匿名 API 限流影响

### English

- Fixed update checks failing with 403 on shared networks: update detection now uses GitHub's public page feed instead of the rate-limited anonymous API

## 1.4 - 2026-10-23

### 中文

- 开放原生插件 SDK：插件基座支持异步查询、执行回执与视图生命周期管理，第三方可独立开发插件
- 目录浏览（cd）改为后台异步列举，大目录与网络盘不再阻塞输入，可随时取消
- 文件搜索性能优化：结果不再逐条读取文件系统信息，出结果更快
- 修复文件搜索中驱动器根目录（如 C:\）标题为空、根目录下文件路径缺少反斜杠的问题
- shell 命令在退出插件视图时自动终止未完成的进程，避免残留
- 换算、shell 等插件视图在离开后释放内存

### English

- Opened the native plugin SDK: the plugin host now supports async queries, execution receipts, and view lifecycle management so third parties can build plugins independently
- Directory browsing (cd) now lists entries in a background thread with cancellation, so large folders and network drives no longer block typing
- Faster file search: results no longer stat the filesystem one by one
- Fixed file search showing an empty title for drive roots (e.g. C:\) and a missing backslash in paths directly under a root
- Shell commands are terminated automatically when leaving the plugin view, leaving no stray processes
- Convert, shell, and other plugin views now release memory after leaving

## 1.3 - 2026-10-22

### 中文

- 应用搜索升级为打分匹配：拼写与拼音匹配更准，同档结果按使用频次靠前
- 设置「关于」页新增手动检查更新按钮，随时确认是否有新版本
- 网络请求默认跟随系统代理设置，代理环境下也能正常检查与下载更新
- 设置页新增清空应用使用记录入口，滚动内容底部增加留白

### English

- App search now uses scored matching with more accurate fuzzy and pinyin results, ranking frequently used apps first among ties
- Added a manual "Check for Updates" button on the Settings > About page
- Network requests now follow the system proxy settings, so update checks and downloads work behind a proxy
- Added an option to clear app usage history in Settings, and bottom padding for scrolled settings content

## 1.2 - 2026-10-20

### 中文

- 优化更新安装交接，安装包接管提权与重启后才退出当前程序，升级更可靠
- 修复输入刷新后回车或点击结果跳回首项的问题，现在始终执行当前选中结果
- 应用后台搜索与图标加载性能优化，不可读取的路径缓存失败结果，避免重复排队
- 待办读取失败时保留原始文件，不再用空数据覆盖已有任务；全局快捷键全部注册成功后才释放旧键

### English

- Improved update handoff: the app now exits only after the installer takes over elevation, file replacement, and restart
- Fixed Enter/click resetting to the first result after the input refreshed; the selected result now runs as shown
- Optimized background app search and icon loading; unreadable paths are cached to avoid repeated work on redraws
- Todo data is preserved when a read fails instead of being overwritten with an empty store; global hotkeys are released only after all new bindings register

## 1.1 - 2026-06-15

### 中文

- 新增启动时自动检测 GitHub Release 更新，发现新版本时提示并可一键下载安装

### English

- Added automatic GitHub Release update check at startup, with one-click download and install when a new version is found

## 1.0 - 2026-06-13

### 中文

首个正式版本

- 新增应用搜索、固定应用和右键取消固定
- 新增命令插件与自定义插件快捷键
- 新增待办事项管理
- 优化动态结果窗口和插件界面尺寸
- 全新 TypeGo 品牌与界面样式

### English

First stable release

- Added app search, pinned apps, and unpin context menu
- Added command plugins and custom plugin hotkeys
- Added todo management
- Improved dynamic result and plugin window sizing
- Introduced the TypeGo brand and refreshed interface
