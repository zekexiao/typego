# TypeGo JSON Viewer

TypeGo 内嵌插件，支持分离窗口、JSON 编辑与 JSON 兼容 MessagePack 数据浏览。
使用仓库根目录的 `build.bat` 构建，`test.bat` 运行模型回归测试。

yyjson 由 `third_party/yyjson` git submodule 管理；首次克隆后运行 `git submodule update --init --recursive`。
MessagePack 保留 64 位整数，仅接受字符串键；不支持的二进制、扩展类型和非有限浮点数会拒绝加载。
MessagePack 修改后通过“另存为”导出 JSON，不覆盖原始二进制文件。
设置保存在宿主分配的插件数据目录。关闭、打开其他文件和退出前会确认未保存编辑。
