# 本地数据与录入格式

数据库文件：`orange-study.sqlite3`。SQLite WAL/SHM 是运行时辅助文件；备份优先使用 GUI/CLI JSON 导出。不要在运行时只复制 `.sqlite3` 文件。JSON 备份包含题目、全部编辑快照与计时记录，`schema_version` 为 1。

## 题目字段

| 字段 | 含义 |
| --- | --- |
| id | 小写 UUID；导入时缺省则新生成，推荐显式提供以防重复 |
| subject | professional / mathematics / english / politics；默认 professional |
| title / content | 必填标题和题干；标题最多 240 字符 |
| answer / notes / conversation | 解析、错因、历史问答，默认空字符串 |
| difficulty | 整数 1–5，默认 3 |
| chapter / source | 非空的单值标签；默认待标注 / 个人提问 |
| status | pending / reviewing / mastered，默认 pending |
| created_at / updated_at | ISO 8601 时间戳；默认录入时的 UTC 时间 |

GUI 和 CLI 检索均使用同一仓储和 SQLite 索引；多个条件用 AND 组合。章节、来源精确匹配，关键词对标题、题干、解析、错因和历史问答进行子串匹配。文字输入的百分号和下划线不被当作通配符。

数学章节标准标签见 [数学教材章节目录](math-chapters.md)。例如 `线性代数 · 第2讲 矩阵`，保存完整板块和讲次，不把细分知识点直接作为目录章节。跨章关联及具体知识点可放在 notes，其他学科仍可自定义章节。更改已有题目的章节须通过 update 保存编辑快照，不修改旧快照；目录与界面筛选本身不会自动重写题库。

content、answer、notes、conversation 按 UTF-8 文本保存。GUI 识别 `$...$` / `\(...\)` 行内公式，以及 `$$...$$` / `\[...\]` 独立公式并离线排版，存储仍保留 LaTeX 源码。JSON 中反斜杠须转义，例如 `{"content":"求 $\\frac{1}{2}$。"}`；用标准 JSON 序列化函数可自动转义。正文不要包在公式定界符内；不支持完整 TeX 文档、文件引用及自定义宏。该显示功能不改变 schema_version，也不修改历史快照。

## 备份结构

```json
{
  "schema_version": 1,
  "questions": [],
  "sessions": [],
  "history": []
}
```

导入必须有 questions 数组（可为空），sessions 和 history 可省略。sessions 包含 id、subject、question_id、label、seconds、started_at、finished_at。question_id 为空字符串表示未关联题目，否则必须引用存在的题目；seconds 是正整数。计时学科保存开始时的学科，之后编辑题目所属学科不会改变过去的学习记录。

history 每项是 `{"question_id":"UUID","snapshot":{完整题目字段}}`，快照 ID 必须与 question_id 一致。导出的 history 按题目内保存顺序排列，导入按内容哈希去重。新导入题目也会保存初始快照。

导入文件有类型错误、无效难度、重复 ID、引用缺失或与本地数据冲突时，整次操作失败并回滚。已有 ID 内容相同会跳过；省略 ID 的文件重复导入会生成不同题目，自动录入应显式提供 UUID。标签建议可以写在 notes 中，之后按用户反馈调整。
