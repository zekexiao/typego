import { latestRelease } from "./data/releases";

export type Language = "zh" | "en";

export const site = {
  name: "TypeGo",
  // 版本与更新日志由 data/releases.ts 从根目录 VERSION 和 CHANGELOG.md 解析。
  version: latestRelease.version,
  downloadUrl: `https://github.com/zekexiao/typego/releases/download/v${latestRelease.version}/TypeGo_${latestRelease.version}_x64.exe`,
};

export const copy = {
  zh: {
    nav: [
      ["首页", "/"],
      ["关于", "/about"],
      ["下载", "/download"],
      ["更新日志", "/download#changelog"],
    ],
    language: "EN",
    languageUrl: "/en/",
    download: "下载 Windows 版",
    requirements: "Windows 10 / 11 · 64 位",
  },
  en: {
    nav: [
      ["Home", "/en/"],
      ["About", "/en/about"],
      ["Download", "/en/download"],
      ["Changelog", "/en/download#changelog"],
    ],
    language: "中文",
    languageUrl: "/",
    download: "Download for Windows",
    requirements: "Windows 10 / 11 · 64-bit",
  },
} as const;
