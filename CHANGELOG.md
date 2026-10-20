# TypeGo 更新日志

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
