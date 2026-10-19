# TypeGo 更新服务器

轻量 Go 后端，用于管理 Windows、macOS、Linux 三个平台的版本、下载链接、更新日志和灰度发布。

## 启动

```powershell
$env:TYPEGO_ADMIN_TOKEN="replace-with-a-long-random-token"
go run ./cmd/server
```

环境变量：

- `TYPEGO_ADDR`：监听地址，默认 `:8080`
- `TYPEGO_DATA_FILE`：版本数据文件，默认 `data/releases.json`
- `TYPEGO_ADMIN_TOKEN`：管理 API Token，必须设置

## 公开接口

- `GET /api/v1/releases/latest?platform=windows&arch=amd64&client_id=device-id`
- `GET /api/v1/update/latest?platform=windows&arch=amd64&current_version=1.0.0&client_id=device-id`
- `GET /download/windows?arch=amd64&client_id=device-id`
- `GET /download/macos`
- `GET /download/linux`

`/api/v1/update/latest` 与桌面端当前更新器字段兼容。下载接口使用 `307 Temporary Redirect`
跳转到符合平台、架构和灰度策略的最新版本下载链接。

灰度版本通过 `rollout_percent` 设置百分比。相同 `client_id` 对同一版本始终得到相同结果；
未提供 `client_id` 时不会命中低于 100% 的灰度版本。

## 管理接口

浏览器访问 `http://127.0.0.1:8080/admin/` 可以使用内置的简单管理后台。输入
`TYPEGO_ADMIN_TOKEN` 后即可新增、编辑、发布、设置灰度比例和删除版本。Token 仅保存在当前
浏览器会话中。

后台支持直接上传安装包到 RainS3。S3 API 端点固定为 `https://cn-nb1.rains3.com`，公开下载
端点固定为 `https://typego.cn-nb1.rains3.com`。Access Key、Secret Key 和 Bucket 在后台
页面填写，仅保存在当前浏览器会话并随上传请求提交，不写入服务端版本数据。
上传后会检查公开链接是否可访问；需要在 RainS3 控制台将 Bucket 配置为公开读取，否则公开
下载地址会返回 `403 AccessDenied`。

后台默认使用预签名 URL 让浏览器直接上传到 RainS3，安装包不会经过 Go 服务或反向代理。
RainS3 Bucket 需要允许后台页面来源执行跨域 `PUT` 请求。

管理接口需要请求头：

```text
Authorization: Bearer replace-with-a-long-random-token
```

列出版本：

```text
GET /admin/api/v1/releases
```

新增或修改版本：

```text
PUT /admin/api/v1/releases
Content-Type: application/json
```

```json
{
  "platform": "windows",
  "arch": "amd64",
  "version": "1.1.0",
  "download_url": "https://downloads.example.com/TypeGo_1.1.0_windows_amd64.exe",
  "changelog": "新增功能与问题修复。",
  "mandatory": false,
  "published": true,
  "rollout_percent": 20
}
```

删除版本：

```text
DELETE /admin/api/v1/releases/windows-amd64-1.1.0
```

上传接口：

```text
POST /admin/api/v1/uploads
Content-Type: multipart/form-data
```

## 后续扩展

`internal/future` 已预留用户和订阅服务接口，`/api/v1/users/` 与
`/api/v1/subscriptions/` 当前返回 `501 Not Implemented`。单实例 JSON 存储后续可以在不改变
HTTP API 的情况下替换为数据库实现。
