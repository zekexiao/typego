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
- 新增或更新 submodule 时，同步维护 `.gitmodules` 和仓库记录的 submodule 提交。

## 构建与验证

- 编译项目统一使用仓库根目录的 `build.bat`。
- 运行项目统一使用仓库根目录的 `run.bat`，由该脚本注入 Qt 运行环境变量。
- 不要新增重复的构建脚本。
- 不要自行运行测试。完成修改和必要的编译检查后，通知用户进行测试。
- 若无法执行 `build.bat`，应明确说明原因和未验证的内容。

## 工具

- 优先使用 Microsoft Coreutils for Windows 处理文件、搜索文本和组合命令管道。
- 在 PowerShell 中显式使用 `.exe` 后缀，避免命中同名别名或内置命令，例如 `cat.exe`、`grep.exe`、`find.exe`、`cp.exe`、`mv.exe`、`sort.exe`、`uniq.exe` 和 `tee.exe`。
- 使用命令前可通过 `<命令>.exe --help` 查看完整语法；某个工具不可用时再使用 PowerShell 等替代方案。
- 注意 Windows 差异：文本可能使用 CRLF，空设备使用 `NUL`，路径可使用 `/` 或 `\`。
- 编辑代码仍应保持 UTF-8 编码；复制、移动或删除文件前确认目标路径，避免误操作。
- Coreutils for Windows 官方仓库：`https://github.com/microsoft/coreutils`。
