# TypeGo 更新日志

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
