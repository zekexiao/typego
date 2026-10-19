# TypeGo Website

TypeGo 官方网站，使用 Astro 构建，部署到 Cloudflare Pages 项目 `typego`。

## 本地开发

```powershell
corepack pnpm install
corepack pnpm dev
corepack pnpm build
```

生产构建输出位于 `dist/`。部署前可设置 `PUBLIC_SITE_URL`，并将 `/download/*` 和
`/api/*` 请求反向代理到 `server/` 中的 Go 后端。

## 版本与更新日志

- 当前版本号统一维护在仓库根目录的 `VERSION`，不要在组件中硬编码。
- CMake 会从 `VERSION` 注入应用版本，并校验 `inno.iss` 的 `MyAppVersion` 是否一致。
- 更新日志维护在仓库根目录的 `CHANGELOG.md`。构建时 `src/data/releases.ts` 会自动
  解析 Markdown，不需要修改页面组件。
- 最新版本必须放在文件最前面，且版本号要与 `VERSION` 一致，否则构建会失败。

每个版本使用以下格式；摘要为普通段落，更新条目使用无序列表：

```markdown
## 1.1 - 2026-10-20

### 中文

可选的一句话摘要

- 第一条更新
- 第二条更新

### English

Optional one-line summary

- First change
- Second change
```

## 构建与部署

在仓库根目录执行：

```powershell
npm run site:install   # 首次或依赖变化时
npm run site:build     # 只构建并检查
npm run site:deploy    # 自动解析 CHANGELOG.md、重新构建并发布到生产分支 main
npm run site:list      # 查看部署历史
```

部署使用本机 Wrangler 登录态；首次使用先执行 `npx wrangler login`。
