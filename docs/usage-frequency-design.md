# 应用使用频次排序设计

状态：已实现
日期：2026-10-21
相关代码：`src/utils/AppMatcher.*`、`src/AppLauncher.*`、`src/MainWindow.cpp`、`src/plugins/AppPlugin.cpp`

## 1. 背景

`AppMatcher` 已经给应用搜索提供了十档文本匹配分，同档内按「名称更短 → 名称字典序 → 路径字典序」决胜。这套规则可解释、可测试，但它对使用习惯完全无感：每天打开十次的 `Visual Studio Code` 和一年没碰过的 `Visual Studio Installer`，在同样的匹配档位下只由名称长度决定先后。

本次设计只增加一件事：**记录应用被成功启动的次数，在不破坏文本匹配优先级的前提下，让常用应用在同档内靠前**。

## 2. 目标与非目标

### 目标

1. 同一匹配档位内，按使用频次降序排列；频次相同看最近使用时间。
2. 频次**永远不能跨越匹配档位**：精确名称匹配（1000 分）无论多冷门，都排在名称前缀匹配（700 分）之前。
3. 启动路径不受影响：记录失败、文件损坏、目录只读都必须静默降级，绝不弹窗、不阻塞启动。
4. 搜索按键不产生磁盘 I/O；只有真正启动应用时才更新内存计数，并延迟落盘。
5. 频次数据只在本地，不采集、不上传；纯逻辑可测，测试不碰用户数据目录。

### 非目标

- 不做搜索历史、最近打开列表、使用时长统计。
- 不做时间衰减的加权分数（v1 只存原始次数 + 最近使用时间，衰减留作后续）。
- 不改变固定项面板（pins）的排列顺序，固定项仍然按用户添加顺序展示。
- 不让插件命令、文件搜索结果参与频次排序。
- 不引入新的配置项开关；频次是默认行为。

## 3. 现状约束

### 3.1 排序链路

- 非空查询：`AppLauncher::search()` 逐条调用 `AppMatcher::evaluate()`，命中项组成 `Hit { item, score }`，再用 `AppMatcher::ranksBefore()` 排序。
- 空查询：`search()` 按索引原序返回（内置项在前，其后是开始菜单和自定义目录），**当前不排序**。是否展示由设置 `showAllAppsWhenSearchEmpty` 决定。
- 结果列表首屏渲染 80 条，滚动时每批追加 120 条（`InitialVisibleResults` / `ResultsFetchBatchSize`），`AppLauncher::search()` 本身不截断。

当前档位：

| 档位 | 分值 | 含义 |
| --- | ---: | --- |
| ExactName | 1000 | 名称完全相同 |
| NamePrefix | 700 | 名称前缀 |
| TokenPrefix | 600 | 名称按非字母数字切词后，某个词的前缀 |
| PinyinPrefix | 550 | 拼音前缀 |
| AcronymPrefix | 500 | 拼音首字母前缀 |
| PinyinContains | 450 | 拼音包含 |
| AcronymContains | 380 | 首字母包含 |
| NameContains | 300 | 名称包含 |
| DescPrefix | 200 | 描述前缀 |
| DescContains | 120 | 描述包含 |

### 3.2 启动入口

`AppLauncher::launch(path)` 是唯一启动漏斗，以下几处都汇到这里：

- 搜索结果回车/双击（`MainWindow::launchCurrent` → `launchPath`）。
- 固定项按钮（`MainWindow` 中 pinned 按钮 → `launchPath`）。
- `app` 命令插件的执行动作（`AppPlugin::execute`）。

因此频次只需在一个地方记录，天然覆盖搜索、固定项和命令三种入口。

`launch()` 返回布尔值，语义是「系统接受了启动请求」：

| 路径类型 | 成功判定 |
| --- | --- |
| `http(s)://`、`ms-settings:`、目录、普通文件 | `QDesktopServices::openUrl()` |
| `.lnk` | `ShellExecuteW()` 返回值 > 32 |
| 其余（exe / 命令） | `QProcess::startDetached()` |

需要注意 `startDetached` 只能保证进程已排队，不能保证应用最终启动成功；这是现有语义，频次沿用，不额外探测。

### 3.3 现有持久化

`pins.json`、`todo.json` 都放在 `QStandardPaths::AppDataLocation`，都用 `QSaveFile` 原子写入。频次文件沿用同样的位置和写入方式。

## 4. 方案总览

新增一个纯逻辑的 `AppUsageStore`（放入 `typego_core`，可单测），由 `AppLauncher` 持有：

```text
launch(path) 成功
    └─ usage.record(path.toLower())        只改内存，标记脏
         └─ 2s 防抖 / 析构时 saveNow()      QSaveFile 原子写 app_usage.json

search(q)
    ├─ AppMatcher::evaluate(entry, q)      只算文本档位，语义不变
    ├─ usage.find(entry.lowerPath)         取得 count / lastUsed
    └─ ranksBefore(entry, {score,count,last}, ...)
```

排序规则从「单一分值比较」升级为**字典序比较**：

```text
匹配档位（降序） → 使用次数（降序） → 最近使用时间（降序） → 名称长度（升序） → 名称（升序） → 路径（升序）
```

前两项决定「谁更相关」，后三项沿用现有的确定性决胜，避免结果抖动。

## 5. 为什么不用加分制

早先讨论过一个简单做法：在匹配分上直接加 `min(count, 20) * 8`（最多 160 分）。这个方案会破坏档位语义，**不采用**：

- 名称前缀 700 + 160 = 860，会反超别名档 850（若后续按既定方案加入别名）。
- 拼音前缀 550 + 160 = 710，会反超完全没有用过的名称前缀 700。也就是说，一个只是拼音沾边但天天用的应用，会压过一个名称直接前缀命中但没怎么用过的应用，用户会觉得「搜索变笨了」。
- 若把加分压到不超过最小档位差（50 分），频次只值 49 分，区分度太弱，几乎看不出效果。

字典序比较等价于「同档内频次权重无限大，跨档权重为零」，既能让常用应用在同档内稳定靠前，又保证文本相关性优先，规则也更容易向用户解释和写测试。

若后续产品上确实希望「高频应用能适度越档」，那应当单独设计「学习排序」，而不是在这里偷偷改加分。

## 6. 数据模型

### 6.1 内存结构

```cpp
struct Usage {
    quint32 count = 0;    // 成功启动次数，已饱和
    qint64 lastUsed = 0;  // UTC 毫秒时间戳
};

QHash<QString, Usage> m_entries;  // key = 小写化的应用路径
```

- **key 使用小写路径**。Windows 路径大小写不敏感，扫描索引、固定项、插件传入的路径统一 `toLower()` 后作为键，避免同一应用出现多个计数。
- 不在 key 上做 `QDir::cleanPath()`：内置项里有 `https://www.bing.com`、`ms-settings:` 这类非文件路径，`cleanPath` 会把 `//` 折叠掉，导致键和搜索条目对不上。路径一致性由「扫描和启动使用同一个字符串」保证，只做小写化。
- 搜索时直接复用 `AppMatcher::Entry::lowerPath` 作为查询键，**不要**在每次按键时对路径做 `toLower()` 分配。

### 6.2 磁盘格式

文件：`<AppDataLocation>/app_usage.json`，与 `pins.json`、`todo.json` 同目录。

```json
{
  "version": 1,
  "entries": {
    "c:/users/me/appdata/local/programs/vscode/code.exe": { "count": 37, "last": 1760000000000 },
    "calc": { "count": 12, "last": 1759990000000 }
  }
}
```

- 顶层留 `version`，为将来结构变化留出判断依据；v1 只认 1，其他值按空数据处理并告警。
- 使用紧凑 JSON（`QJsonDocument::Compact`），千条记录约几十 KB。
- 只记录聚合结果，不记录每一次启动的时间序列，控制体积和隐私面。

### 6.3 上限与清理

| 限制 | 取值 | 处理方式 |
| --- | ---: | --- |
| 单条计数 | 9999 | 饱和，不再递增 |
| 条目总数 | 1024 | 超出时按 `(count, lastUsed)` 最小者淘汰 |
| 过期条目 | 180 天未使用 | 加载时丢弃 |
| 文件大小 | 1 MiB | 超过视为损坏，不解析 |

清理只影响排序加成，不影响应用能否被搜索到，因此可以放心地静默执行。淘汰时若 `(count, lastUsed)` 完全相同，再按路径字典序决定先后，保证同一份数据每次清理结果一致。已卸载应用留下的陈旧路径也会被 TTL 自然淘汰。

## 7. 记录与落盘时机

### 7.1 记录

- 只在 `launch()` 返回 `true` 后调用 `record(path)`；失败不计数。
- 空路径直接返回，不记录。
- `record()` 内部：`lastUsed = max(现有值, now)`。用户手动回拨系统时间时，最近使用时间不会倒退，排序不会突然乱掉。
- 计数变化只影响下一次 `search()`，当前已经展示的结果列表不重排，避免「启动前脚刚点完，列表在背后跳动」。

实现上把 `launch()` 的各个分支收敛成一个 `bool ok`，在函数末尾统一 `if (ok) record`，避免每个分支重复调用。

### 7.2 落盘

- 构造时加载一次。文件不存在、解析失败、版本不认识、超过大小上限，都按空数据处理；只写 `qWarning`/`qDebug`，界面无感知。
- `record()` 只改内存并置脏，启动一个 2 秒单次定时器；定时器触发或 `AppLauncher` 析构时 `saveNow()`。
  - 用户连续启动多个应用只写一次文件。
  - 键盘输入完全不会触发写盘。
  - 进程被强杀最多丢失最后 2 秒的记录，可接受。
- 写入用 `QSaveFile` + `commit()`，失败只记录日志；内存数据仍然有效，下一次启动会重试保存。
- 保存前先清理过期条目并执行条目上限淘汰。

### 7.3 线程与性能

- `AppUsageStore` 只在 GUI 线程读写；后台扫描线程不接触它，因此不需要锁。
- 非空查询只对**已经命中**的条目做一次哈希查找，命中集通常远小于索引总量。
- 空查询需要对全部条目查找，千级应用量级下开销可忽略；相比每次按键重新拼排序键，这点查找成本更低。
- 排序时 `Hit` 结构体直接携带 `{score, count, lastUsed}`，不要在比较器里反复查哈希表（比较器会被调用 O(n log n) 次）。

## 8. 排序集成

### 8.1 接口

保持 `AppMatcher::evaluate()` 不变，只扩展比较：

```cpp
struct RankKey {
    int score = 0;         // 文本匹配档位
    quint32 count = 0;     // 使用次数
    qint64 lastUsed = 0;   // 最近使用时间
};

bool ranksBefore(const Entry &a, const RankKey &ka,
                 const Entry &b, const RankKey &kb);
```

原有 `ranksBefore(Entry, int, Entry, int)` 保留为 `RankKey{score, 0, 0}` 的转发，现有测试不用改写。

比较顺序：

1. `score` 降序；
2. `count` 降序；
3. `lastUsed` 降序；
4. `lowerName.size()` 升序；
5. `lowerName` 升序；
6. `lowerPath` 升序。

### 8.2 非空查询示例

| 查询 | 应用 | 档位 | 次数 | 结果顺序 |
| --- | --- | ---: | ---: | --- |
| `ca` | Calculator | 700 | 30 | 1 |
| `ca` | Camera | 700 | 2 | 2 |
| `code` | Code | 1000 | 0 | 1 |
| `code` | Visual Studio Code | 600（token） | 50 | 2 |
| `co` | Color Picker | 700 | 1 | 1 |
| `co` | Visual Studio Code | 600（token） | 50 | 2 |

最后两行是重点：`Visual Studio Code` 再常用，也不能越过「名称前缀」档位。只有同为 TokenPrefix / 同为 NamePrefix 时，次数才起作用。

### 8.3 空查询

空查询目前不排序。建议（待确认）：

- 用 `std::stable_sort` 按 `(count, lastUsed)` 降序排列；
- 从未使用过的条目保持索引原序（内置项在前，其后按扫描目录），不引入名称字典序，避免每次打开都把「所有应用」重排成字母表；
- 不做额外截断，首屏 80 条 + 滚动加载已经够用；按频次排完后的第一屏正好是「常用的那些」。

这样既能让空输入下的面板真正体现使用习惯，又保留了未被使用条目的既有顺序。

### 8.4 与别名、高亮的关系

- 别名档（既定方案中的 850 分）位于 ExactName 与 NamePrefix 之间。频次只在同档内生效，因此别名命中永远不会被高频的普通前缀命中挤下去；若多个应用同时命中别名，再由频次决胜。
- `Match.score` 仍然只表示文本档位，不掺入频次。若后续要做搜索高亮或档位调试，分数语义保持干净；频次只存在于排序键中。
- 固定项面板不参与频次排序；用户手动排的顺序优先。

## 9. 可测性

### 9.1 模块归属

新增 `src/utils/AppUsageStore.h/.cpp`，加入 `typego_core` 源码清单。构造函数注入文件路径与时钟：

```cpp
explicit AppUsageStore(QString filePath,
                       std::function<qint64()> now = &QDateTime::currentMSecsSinceEpoch);
```

- 测试用 `QTemporaryDir` 生成隔离路径，完全不触碰 AppData。
- 时钟注入让「180 天过期」「最近使用时间」可确定性验证，不依赖真实时间。
- `AppLauncher` 构造时传入真实的 AppData 路径；测试不实例化 `AppLauncher`。

### 9.2 用例清单（`tests/tst_app_usage.cpp`）

1. 文件不存在时加载为空，且不报错。
2. `record` 后计数递增，`lastUsed` 取注入时钟。
3. 同一路径不同大小写合并为一条。
4. 计数在 9999 饱和。
5. 损坏 JSON / 未知版本 / 超大文件 → 返回空数据，后续保存能重写为合法文件。
6. 保存后重新加载，内容与内存一致（round-trip）。
7. 180 天未使用的条目在加载时被清理。
8. 条目超过 1024 时按 `(count, lastUsed)` 淘汰。
9. 系统时间回拨时 `lastUsed` 不倒退。

### 9.3 排序用例（扩展 `tests/tst_app_matcher.cpp`）

1. 同档内次数多的排前面。
2. 同档同次数时最近使用的排前面。
3. 次数和最近使用都相同，回落到名称长度/名称/路径决胜。
4. 频次不能跨档：`NamePrefix + 9999 次` 仍排在 `ExactName + 0 次` 之后；`TokenPrefix + 9999 次` 仍排在 `NamePrefix + 0 次` 之后。
5. 空查询的 `usageRanksBefore`：常用在前，未使用项保持稳定顺序。

按仓库约定，运行 `test.bat` 验证，不新增测试框架或脚本。

## 10. 失败与边界

| 场景 | 行为 |
| --- | --- |
| AppData 只读 / 磁盘满 | 内存计数照常生效，保存失败只写日志，不弹窗 |
| 文件被用户删除 | 内存数据继续工作，下次保存重新创建 |
| 文件损坏 | 按空数据启动，下一次保存覆盖为合法内容 |
| 应用升级导致路径变化 | 旧键成为陈旧条目，180 天后清理；新路径从 0 开始计数 |
| 通过固定项启动 | 与搜索启动共用同一计数 |
| 通过 `app` 命令启动 | 同上（`AppPlugin` 最终调用 `launch`） |
| 启动失败 | 不计数 |
| 时钟回拨 | `lastUsed` 取历史最大值，不回退 |
| 多用户共用电脑 | AppData 按用户隔离，互不影响 |

频次记录不校验路径是否仍在索引中：已卸载应用的计数不会出现在搜索结果里，过期后自然清掉；这样避免为了记录再去遍历索引。

## 11. 隐私与用户控制

- 数据仅包含本机路径、启动次数和最近使用时间，留在本地 AppData，无任何网络上报。
- 设置页「设置 → 隐私」提供「清空」入口（已实现），确认后调用 `AppLauncher::clearUsageHistory()`，清空内存并落盘为空文件，当前结果列表立即失去频次加成。
- 该文件不参与 `pins.json` / `todo.json` 的导入导出（当前也没有这类功能）。

## 12. 实施拆分（批准后执行）

1. **纯逻辑**：新增 `AppUsageStore` + `tst_app_usage`，接入 `typego_core` 与 `tests/CMakeLists.txt`，跑 `test.bat`。此步不改变现有行为。
2. **排序键**：`AppMatcher::RankKey` 与新的 `ranksBefore`，旧接口转发；扩展 `tst_app_matcher` 覆盖跨档与决胜。此步仍不改变界面行为。
3. **接线**：`AppLauncher` 加载/记录/防抖保存，`search()` 填充 `RankKey`，空查询改为按频次稳定排序。跑 `test.bat` + `build.bat`，由用户本机验证搜索与启动。
4. **可选**：设置页清空入口。

改动集中在 `AppMatcher`、`AppUsageStore`、`AppLauncher` 三处，`MainWindow` 与插件层不需要改动。

## 13. 待决策（已拍板，按推荐执行）

1. **空输入是否按频次排**：按第 8.3 节稳定排序，未使用项保持原序。
2. **时间衰减**：v1 不做，靠 180 天 TTL 控制陈旧数据。
3. **清空入口**：设置页「设置 → 隐私」已提供「清空」按钮。
4. **固定项是否也按频次排**：否，固定项是用户显式排序。

## 14. 验收标准

- `test.bat` 全部通过，新增的存储与排序用例覆盖第 9 节清单。
- `build.bat` 编译通过，安装包不受影响（测试目录不进包）。
- 本机手动验证：
  - 连续启动同一个应用后，同档搜索结果中它排到前面；重启 TypeGo 后顺序保持。
  - 精确名称匹配仍然压过任何高频的弱匹配。
  - 空输入（开启「显示所有应用」）时常用应用出现在首屏，未使用应用顺序不变。
  - 启动失败（例如路径已失效）不会增加计数。
  - 键盘输入过程中 `app_usage.json` 的修改时间不变；启动应用后约 2 秒更新。
  - 手工写坏 `app_usage.json`，启动与搜索不受影响，下一次启动后文件恢复合法。
