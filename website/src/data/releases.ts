import changelogSource from "../../../CHANGELOG.md?raw";
import currentVersionSource from "../../../VERSION?raw";

export interface Release {
  /** 与安装包、后端 releases 表一致的版本字符串，不要带前缀 v。 */
  version: string;
  /** 发布日期，ISO 格式，用于页面展示。 */
  date: string;
  /** 每个版本的更新条目，按中英文分别给出。 */
  notes: Record<"zh" | "en", string[]>;
  /** 可选的一句话版本摘要，展示在版本号旁。 */
  tag?: Record<"zh" | "en", string>;
}

type LocalizedContent = { notes: string[]; tag?: string };

/** 解析一个语言区块：普通段落作为摘要，无序列表作为更新条目。 */
function parseLocalizedContent(source: string): LocalizedContent {
  const notes: string[] = [];
  const tagLines: string[] = [];

  for (const rawLine of source.split("\n")) {
    const line = rawLine.trim();
    if (!line) continue;

    const item = line.match(/^[-*]\s+(.+)$/);
    if (item) notes.push(item[1].trim());
    else tagLines.push(line);
  }

  if (notes.length === 0) {
    throw new Error("CHANGELOG.md 的每个语言区块都必须包含至少一个列表项");
  }

  return { notes, tag: tagLines.length > 0 ? tagLines.join(" ") : undefined };
}

/** 解析仓库根目录的 CHANGELOG.md，格式错误时直接终止构建。 */
function parseChangelog(markdown: string): Release[] {
  const source = markdown
    .replace(/\r\n?/g, "\n")
    .replace(/<!--[\s\S]*?-->/g, "");
  const blocks = source.split(/^##[ \t]+/m).slice(1);

  if (blocks.length === 0) {
    throw new Error("CHANGELOG.md 中没有任何版本记录");
  }

  return blocks.map((block) => {
    const headingEnd = block.indexOf("\n");
    if (headingEnd < 0) throw new Error("CHANGELOG.md 的版本记录缺少正文");

    const heading = block.slice(0, headingEnd).trim();
    const match = heading.match(/^(\d+\.\d+(?:\.\d+)?)\s+-\s+(\d{4}-\d{2}-\d{2})$/);
    if (!match) {
      throw new Error(`CHANGELOG.md 版本标题格式错误：${heading}；应为“版本号 - YYYY-MM-DD”`);
    }

    const localized: Partial<Record<"zh" | "en", LocalizedContent>> = {};
    const sections = block.slice(headingEnd + 1).split(/^###[ \t]+/m).slice(1);
    for (const section of sections) {
      const titleEnd = section.indexOf("\n");
      if (titleEnd < 0) throw new Error(`CHANGELOG.md ${match[1]} 的语言区块缺少正文`);

      const title = section.slice(0, titleEnd).trim();
      const lang = title === "中文" ? "zh" : title === "English" ? "en" : undefined;
      if (!lang) {
        throw new Error(`CHANGELOG.md ${match[1]} 包含未知语言区块：${title}`);
      }
      localized[lang] = parseLocalizedContent(section.slice(titleEnd + 1));
    }

    const zh = localized.zh;
    const en = localized.en;
    if (!zh || !en) {
      throw new Error(`CHANGELOG.md ${match[1]} 必须同时包含“中文”和“English”区块`);
    }

    return {
      version: match[1],
      date: match[2],
      notes: { zh: zh.notes, en: en.notes },
      ...(zh.tag && en.tag ? { tag: { zh: zh.tag, en: en.tag } } : {}),
    };
  });
}

const currentVersion = currentVersionSource.trim();
export const releases = parseChangelog(changelogSource);

if (releases[0].version !== currentVersion) {
  throw new Error(
    `CHANGELOG.md 最新版本 ${releases[0].version} 与 VERSION 文件 ${currentVersion} 不一致`,
  );
}

/** 当前发布版本，供下载页主标题和徽标使用。 */
export const latestRelease: Release = releases[0];
