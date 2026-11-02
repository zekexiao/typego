# 项目开发约定

## 基本原则

- 性能和内存效率是首要原则。实现功能时优先降低运行时开销、内存占用和不必要的分配、复制。
- 所有代码和文本文件统一使用 UTF-8 编码。
- 代码注释使用中文，注释应简洁，并重点说明设计意图、约束和不明显的逻辑。
- 保持改动范围聚焦，遵循仓库现有代码风格，避免与任务无关的重构。

## 依赖管理

- 不使用 vcpkg。
- 第三方依赖使用 git submodule 管理，不要手工复制依赖到仓库。
- spdlog 使用 `third_party/spdlog` git submodule，并通过根目录 `CMakeLists.txt` 的 `add_subdirectory` 接入。
- 全局快捷键使用 `third_party/QHotkey` git submodule，不要手写平台原生热键监听。
- 新增或更新 submodule 时，同步维护 `.gitmodules` 和仓库记录的 submodule 提交。

## 构建与验证

- 编译项目统一使用仓库根目录的 `build.bat`。
- 运行项目统一使用仓库根目录的 `run.bat`，由该脚本注入 Qt 运行环境变量。
- 不要新增重复的构建脚本。
- 单元测试统一使用仓库根目录的 `test.bat`（配置 + 构建 `check` 目标 + 跑 ctest）。
  改动 `typego_core` 里的纯逻辑后必须运行它并报告结果。
- 界面使用下述全量截图流程检查；真实交互、系统磨砂、多屏和原生文件选择器仍需在本机验证。
- 若无法执行 `build.bat`，应明确说明原因和未验证的内容。

## 自动化测试

### 界面截图与交付

- 每次完成界面改动，都必须生成并检查整套界面截图，不能只截本次改动的页面。
- `build.bat` 编译成功后自动调用 `run.bat --ui-snapshots`；只需重新截图时直接运行后者。
- 默认输出到 `build/ui-snapshots/<UTC时间>`；`build/ui-snapshots/LATEST.txt` 指向最近成功批次。
  也可用 `run.bat --ui-snapshots <新目录>` 指定输出目录，禁止覆盖已有目录。
- 截图按分组开关：`--pages=main,settings,json,tools,update,capture`，缺省只跑 `main,settings`（主窗口输入态与设置页）。
  改动其它功能时用 `--pages=all` 或指定对应分组；`core` 是 `main,settings` 的别名。
- 每个批次包含浅色和深色主题的原尺寸 PNG、`manifest.json`（含 `groups` 字段）和内嵌图片的单文件 `index.html` 图册。
  全局部覆盖：主窗口及输入结果态、全部设置页（长页分段）、内置工具设置、JSON 窗口与弹窗、更新弹窗、截图选区/工具栏/贴图/长截图面板。
- 截图使用生产 Qt Widgets 和临时示例数据；入口必须先于日志、单实例、自启、热键、网络请求和后台服务初始化。
  禁止为截图读取个人剪贴板、用户配置或扫描个人文件；所有默认 QSettings 必须重定向到临时 INI。
  示例夹具目录固定为 `build/ui-snapshots/_fixture`，保证 PNG 字节可复现，跨批次才能用哈希当变更信号。
- 新增页面、弹窗、设置分组或重要状态时，同步扩展 `src/UiSnapshots.cpp` 并归入合适分组。浅深色必须覆盖同一套内容。
- 生成后用 `tools/ui-snapshot.ps1` 产出审阅包（默认读 `LATEST.txt` 指向的批次）：
  把同一页面的浅色与深色拼成对照总览图 `review/sheet-NN.png`，并与上一批次逐页哈希对比，
  输出 `review/summary.json`（pages/changed/added/removed/sheets）与 `review/index.html` 图册。
  常用参数：`-Generate` 先重跑截图（`-Scope all` 会自动传 `--pages=all`）、`-Scope all`、`-Ids 'json-*,update'`、
  `-Batch <目录>`、`-Baseline none`、`-TileWidth`、`-RowsPerSheet`。
- 生成后检查文字、底色、布局及长页面是否完整；失败必须修复或明确说明，不能交付上次的截图冒充本次结果。
- 完成回复须一次性交付整套审阅图（每个分组都覆盖，浅深对照），不逐个导入几十张原尺寸 PNG；
  用户要看细节时再导入对应原图。不要只给本机路径或让用户自行运行。
  在 Cindy 中使用媒体导入工具导入真实 PNG 后展示返回地址，查看图片的工具不等于向用户交付图片。
- 截图是离屏视觉检查，不替代交互测试；系统合成的磨砂效果、真实桌面捕获、DPI/多屏、原生对话框需另行实机验证。

### 逻辑测试

- 测试框架用 Qt Test（Qt6::Test），不引入 GoogleTest。每个用例文件是一个独立可执行程序，
  入口用 `QTEST_GUILESS_MAIN`，因此测试不依赖窗口和平台插件。
- 测试目录 `tests/`，新增用例文件时在 `tests/CMakeLists.txt` 里调用 `typego_add_test(名字 文件)` 即可。
- 无界面依赖的业务逻辑放进 `typego_core` 静态库（表达式计算、动作分类、拼音、
  版本比较与更新检查解析、终端参数拼装、配置读写），`TypeGo` 可执行目标和测试都链接它；
  新增这类模块时同时更新该库的源码清单。
- 需要把散落在界面或网络代码里的规则纳入测试时，抽成独立函数或模块，不要为了测试去 mock 整个类。
- 测试一律不能读写用户数据目录，也不能碰注册表：涉及落盘时用 `QTemporaryDir` 构造隔离路径，
  配置类用 ini 后端构造；为此类服务注入路径的构造函数是允许的开洞方式。
  构造函数里的副作用（如自启项同步）要改成显式调用，由 `main.cpp` 触发。
- 断言只锁行为不锁实现：数值比较用容差，字符串比较用界面实际拿到的字段。
  发现代码行为与预期不符时先确认是不是缺陷，不要直接把缺陷写成"预期结果"。
- `test.bat` 等批处理文件内只写 ASCII，cmd 按 GBK 解析带中文的行会打乱引号；中文注释只放在 CMake 和源码里。

## 国际化（i18n）

- 面向用户的文案一律用 `tr()`（QObject 子类内）或 `QCoreApplication::translate("上下文", "文案")` 包裹，
  源码里只保留英文原文，不要把中文硬编码进界面。
- `tr()` / `translate()` 的参数必须是**纯字符串字面量**：`tr("Save")` 可以，`tr(QStringLiteral("Save"))` 不行。
  `lupdate` 解析的是字面量 token，用 `QStringLiteral()` 包一层它就提取不到，文案会永远漏翻译。
- 非 QObject 的类（存储层、平台辅助代码等）没有 `tr()`，统一用
  `QCoreApplication::translate("上下文", "文案")`，上下文取类名。
- 主程序翻译文件是 `i18n/TypeGo_zh_CN.ts`，`build.bat` 会自动跑 `lupdate` 更新它。
  插件翻译文件在 `plugins/<插件>/translations/<插件>_zh_CN.ts`，插件用的是 `qt_add_translation`，
  **不会自动跑 lupdate**：新增或修改插件文案后必须手动执行
  `"C:\Qt\6.10.3\msvc2022_64\bin\lupdate.exe" plugins/<插件> -ts plugins/<插件>/translations/<插件>_zh_CN.ts -no-obsolete`，
  否则新文案在插件里根本不会出现。
- 每条 `<message>` 的 `<translation>` 必须有内容，不允许留空。留空时 Qt 直接回退成英文原文，
  界面上就会露出没翻译的英文。专有名词、命令行、JSON 字面量（如 `PowerShell`、`cmd.exe`、`true`、`null`）
  确实不需要翻译时，把原文照抄进 `<translation>`，同样不要留空。
- 新增界面文案要在同一次改动里补齐译文，不能只加原文等以后再翻。
- 改完文案要重新构建，让 `.qm` 重新生成并嵌入资源（`qt_add_translations` 会把 qm 放进 `:/i18n/`）。
- 插件描述（`plugin.json` 的 `description`）会显示在命令栏和分离窗口标题上，同样需要本地化：
  在 `plugin.json` 里加可选的 `descriptionTranslations`（键是 `zh_CN` 这类语言名），宿主按界面语言取，
  取不到时回退英文原文。插件 SDK 的虚表固定，不能靠加接口返回译文。
- Qt 标准控件文案通过启动时加载的 Qt 翻译文件本地化，包括 `QKeySequenceEdit` 的快捷键提示。
- `tst_translations` 用例会校验所有 `.ts` 的译文非空，并反向核对源码里每个 `tr()`/`translate()`
  文案都已登记到对应 `.ts`（插件没有自动 lupdate，这一步是主要的兜底），提交前必须让它通过。

## 发版工作流

发布新版本时按顺序执行以下步骤，不要跳步：

1. **提交功能代码**：完成开发并通过 `build.bat` 编译检查，提交全部改动。
2. **更新版本号**：同时修改三处，保持一致（`CMakeLists.txt` 配置阶段会强制校验）：
   - 根目录 `VERSION`（唯一版本来源）
   - `inno.iss` 的 `#define MyAppVersion`
   - `CHANGELOG.md` 顶部新增版本条目（中英双语，日期格式 `YYYY-MM-DD`）
3. **提交并推送**：`git add` + `git commit` + `git push origin master`。
4. **编译并发布 Release**：
   - 运行 `build.bat` 生成 Release 构建
   - 用 `"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" inno.iss` 打包，产物为 `dist\TypeGo_<版本>_x64.exe`
   - 创建并推送标签 `git tag -a v<版本> -m "TypeGo <版本>" && git push origin v<版本>`
   - 在 GitHub `zekexiao/typego` 创建同名 Release（正文取 CHANGELOG 该版本条目），上传安装包 asset，文件名必须是 `TypeGo_<版本>_x64.exe`（客户端更新器按 `.exe` + `x64` 命名规则匹配下载地址）
5. **更新网页信息**：官网版本与下载链接由 `website/src/data/releases.ts` 自动从 `CHANGELOG.md` + `VERSION` 派生，无需手改；确认 CHANGELOG 条目内容即可，网站构建时会校验两者一致。
6. **发布网页**：根目录执行 `npm run site:deploy`（wrangler pages，项目名 `typego`）。

注意：客户端更新检测走 `releases.atom` 页面接口（匿名 API 有 60 次/小时/IP 限流，共享出口 IP 会 403），安装包地址按 tag + 命名约定推导。

## 工具

- 优先使用 Microsoft Coreutils for Windows 处理文件、搜索文本和组合命令管道。
- 在 PowerShell 中显式使用 `.exe` 后缀，避免命中同名别名或内置命令，例如 `cat.exe`、`grep.exe`、`find.exe`、`cp.exe`、`mv.exe`、`sort.exe`、`uniq.exe` 和 `tee.exe`。
- 使用命令前可通过 `<命令>.exe --help` 查看完整语法；某个工具不可用时再使用 PowerShell 等替代方案。
- 注意 Windows 差异：文本可能使用 CRLF，空设备使用 `NUL`，路径可使用 `/` 或 `\`。
- 编辑代码仍应保持 UTF-8 编码；复制、移动或删除文件前确认目标路径，避免误操作。
- Coreutils for Windows 官方仓库：`https://github.com/microsoft/coreutils`。
