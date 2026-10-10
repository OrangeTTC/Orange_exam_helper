# Orange 考研助手协作约定

- 作者 Orange；原创代码使用 Apache-2.0，在新增源文件注明版权与 SPDX，中文注释说明用途与关键约束。
- 保持 C++17 + Qt6，数据/计时核心与 GUI 分离，不引入联网 AI 服务或私自上传题库。
- 用户学习目标为数学（一）及电子科技大学 839 专业课；专业课默认优先。未知大纲、章节及题目来源不能编造。
- 数学章节按用户提供的三张教材目录照片划分，标准标签见 docs/math-chapters.md，目录定义见 src/core/chapter_catalog.cpp。使用完整“板块 · 第N讲 名称”标签，知识点细分写入 notes；跨章题按主要解题内容选择一章，关联章写入 notes。不确定时仍为“待标注”。
- 仅在用户明确表示需要记录错题时写入；普通解题讨论不自动收录。题目、解析、错因和历史问答都应按已知内容整理。
- 后续自动录入：在 `data/inbox/` 保存 UTF-8 JSON（schema_version=1），显式生成稳定小写 UUID，按 docs/data-format.md 填写字段，再用 `release/OrangeStudy-1.3.0/orange-cli.exe --data-dir data import <文件>`。根目录 Start.cmd 与此共用 data 目录。成功后报告题目 ID 与难度/章节/来源标签，提醒界面刷新。
- 不确定的章节/来源标为“待标注”。仅来源确为个人提问时填“个人提问”；难度是建议值，可在笔记中说明依据，用户意见优先。
- 同一题目的追问用 CLI show 获取原记录，再追加 conversation，通过 update 保留 ID 和旧 updated_at 防止冲突；不覆盖旧问答。
- 测试使用临时目录，不往真实 data 写入样例。存储修改后运行 ctest；Windows 打包用 scripts/build.ps1 -Package。
