# Orange 考研助手

作者 **Orange**。C++17 + Qt 6 Widgets + SQLite，本地运行的跨平台错题本与学习计时器。项目原创代码使用 **Apache License 2.0**。

Git 仓库提供源码、测试与构建脚本。个人题库、本机工具链和编译产物不提交；首次克隆后请按“编译与测试”准备 Qt 并构建，生成 Windows 便携版后即可使用 Start.cmd。

放大阅读默认全屏打开；右上角固定显示“关闭阅读”按钮，底部保留“关闭”按钮，均可用鼠标点击返回题库，也支持 Esc。

每次进入放大阅读时默认隐藏答案与解析，同时隐藏可能含答案的错因、笔记和历史问答。点击上方“显示答案（Enter）”或按主键盘回车 / 小键盘 Enter 后展开，并定位到解析；重复按 Enter 不会关闭窗口。隐藏状态下复制原始文本仅包含题目，字号调整及标记复习中不会提前显示答案。此状态只影响当前放大阅读窗口，不写入题库；普通详情、编辑与历史窗口仍完整显示。

数学章节筛选可选择高等数学、线性代数、概率论与数理统计，汇总该板块的全部题目。“数学章节目录”窗口左侧列出目录和题数，右侧即时列出所选目录的题目，点击题目查看正文与解析。

## 使用

在本项目中双击根目录 **Start.cmd** 启动已编译的 Windows 版（1.4.1）；所有题目、问答和计时保存在项目根目录 `data/orange-study.sqlite3`。如果只复制 `release/OrangeStudy-1.4.1`，双击其中的 Start.cmd，便携版会在自己的 `data` 目录保存题库。两种启动方式数据目录不同，迁移时可使用 JSON 备份。

1. 启动默认进入**全部错题**；左侧可选择专业课、数学（一）、英语或政治。全部范围新增题目时默认科目仍为电子科技大学 839 专业课，可在编辑窗口调整。
2. 点击“加入错题”，填写题目、难度、章节、来源，可追加解析、错因和历史问答。
3. 通过难度、章节、来源及状态组合筛选，或搜索题干、解析、笔记与历史问答。双击题目编辑。
4. 选中题目后点击“标记复习中”或“标记已掌握”，无需进入编辑窗口；全屏放大阅读底部也可一键标记复习中。状态立即保存，已处于复习中的题目不会重复写入历史。点击“编辑历史”查看历次保存的完整快照。
5. “学习计时”支持正计时、1–600 分钟倒计时、暂停/继续及结束保存。可关联选中题目，关联关系和学科在开始时固定。
6. “导出备份”保存全部题目、编辑历史、计时；“导入”还原 JSON。相同 UUID 的相同记录跳过，冲突记录会导致整个导入回滚，不静默覆盖。

数学（一）按用户提供的教材目录分为高等数学、线性代数、概率论与数理统计，共 37 项。点击“数学章节目录”查看各章题数并进入章节；录入窗口可选择章节。完整目录与标准标签见 [数学教材章节目录](docs/math-chapters.md)。其他学科与旧章节保留自定义标签。839 的院校及编号按用户指定配置，软件未内置未经确认的考试大纲或教材章节。英语未假设为英语（一）或英语（二）。难度从 1 入门到 5 挑战，可在编辑中调整。当前版本不支持图片附件或 OCR。

### 错题分析

“错题分析”与“错题本”“学习计时”并列，可切换按科目或按章节查看折线图。橙色实线显示全部错题，绿色虚线显示尚未掌握（待复习与复习中）；下方按尚未掌握题数降序列出复习重点。这里统计已收录题数，不是作答错误率或重复做错次数。

数学按教材 37 项顺序展示，包含零题章节，并支持按高等数学、线性代数、概率论与数理统计筛选；自定义与待标注章节追加展示。其他科目仅展示已录入的章节，名称中的数字按自然顺序排列。章节图每章至少预留 148 像素横向空间，多章时横向滚动，长名称换行，悬停可查看完整标签和数量，下方表格也提供完整名称提示。“放大图表”可全屏阅读，右上角“关闭图表”或 Esc 返回。进入分析页或点击“刷新统计”会读取最新题库，统计不受错题本的关键词、难度、来源与状态筛选影响。

### 数学公式

题干、解析、笔记及历史问答支持离线 LaTeX 数学排版。行内公式用 `$...$` 或 `\(...\)`，独立公式用 `$$...$$` 或 `\[...\]`。例如：

```text
行内：当 $|x|<1$ 时，级数收敛。
分式：$\dfrac{f(x)-f(1)}{x-1}$
独立公式：$$\sum_{n=1}^{\infty}\frac{x^n}{n^2}$$
矩阵：$$\begin{pmatrix}1&0\\0&1\end{pmatrix}$$
```

编辑窗口左侧填写源码，右侧随当前标签页显示预览；详情页点击“放大阅读”可查看长公式。行内分式用 `\dfrac` 可使分子、分母更大，普通 `\frac` 遵循行内大小。右键“复制原始文本（含 LaTeX）”可复制完整可编辑内容。题库及 JSON 备份保存公式源码，CLI 输出同样保留源码。

渲染器为本地 C++ MicroTeX，数学字体随程序嵌入，无需联网、浏览器或安装 TeX。支持常用分式、根式、上下标、求和、积分、矩阵和分段函数；支持的是数学表达式子集，不接受完整 LaTeX 文档、自定义宏或文件引用。无效或不支持的公式显示源码并提示，中文正文按文本显示，Markdown 不作为富文本解析。原有普通符号文本需要转换为上述公式语法才会排版。

### 字号调节

题目详情上方、放大阅读、编辑预览和编辑历史中均有“字号”控件。点击 `A−` / `A+` 或输入数字，可在 **12–32 px** 间调整；“默认”恢复 **15 px**。正文、公式和编辑窗口的源码文字同步缩放，已打开的阅读窗口共用设置。长公式保持选定字号，可以横向滚动或使用“放大阅读”。菜单、标签与计时控件保持界面字号。

字号自动保存到当前系统用户的 Qt 本地设置，重启后恢复；它是显示偏好，不写入题库或编辑历史，也不包含在题库 JSON 备份中。

计时使用单调时钟，暂停不累计。倒计时结束发出提示音并停表，需点击结束保存。关闭窗口时会询问是否保存当前计时；进程被强制结束的未保存计时不能恢复。今日时长统计所有学科**已保存且在本地今日结束**的学习记录，跨日记录归入结束日。

## 后续让我录入错题

在这个项目中继续提问，并明确说“记录为错题”。我可整理题干、解析、问答和标签，通过配套 C++ CLI 写入同一题库；未明确要求记录时不录入。难度可以先给出建议，未知章节/来源保留“待标注”，不虚构来源。软件不会自动监听聊天，也没有内置在线 AI；其他聊天需要提供本项目位置和待录入内容。

录入 JSON 格式见 `examples/question.json`（仅格式示例，不会自动加入真实题库）。建议为同一题目生成并保留 UUID，使重试导入不会重复。保存 UTF-8 JSON 后：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/record.ps1 -JsonFile "D:\path\question.json"
```

GUI 点击“刷新”即可看到命令行新增记录。

```powershell
# 根目录 Start.cmd 与这些 CLI 命令共用 data。
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' list --subject professional --chapter '待标注' --difficulty 3
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' list --source '个人提问' --search '关键词'
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' show '<题目 UUID>'
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' history '<题目 UUID>'
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' export 'backup.json'
& '.\release\OrangeStudy-1.4.1\orange-cli.exe' --data-dir '.\data' stats
```

`update 文件.json` 接受含 `id` 的部分字段对象，例如 `{"id":"…","notes":"新的错因"}`。追加问答时先 show 读取原 conversation，再合并追加。可携带原 `updated_at` 防止覆盖并发编辑。CLI 所有输出为 UTF-8，错误时返回非零退出码。

## 编译与测试

依赖 CMake 3.21+、C++17 编译器、Qt 6.5+ 的 Widgets/Sql，测试还需要 Qt Test。Windows 本机已准备 Qt 6.8.3 与匹配的 MinGW 13.1（位于被 Git 忽略的 `.tools`）。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Package
# 使用其他已安装 Qt kit：
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtPrefix 'C:\Qt\6.8.3\mingw_64' -Package
```

Linux（例如安装 Qt 6 开发包的环境）和 macOS（例如使用 Qt 安装器）：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/OrangeStudy --data-dir ./data
./build/orange-cli --data-dir ./data list
```

Windows 打包默认输出到 `release/OrangeStudy-1.4.1`，也可用 `-PackageDir` 指定路径；根目录启动器使用上述默认路径。Windows 用 windeployqt 打包运行依赖；macOS 可用 macdeployqt，Linux 需安装对应 Qt 运行库及 SQLite 驱动。源代码面向 Windows/macOS/Linux；当前交付构建与运行验证在 Windows，其他平台尚未实机验证。

没有传入 `--data-dir` 时，GUI 与 CLI 使用同一个 `QStandardPaths::AppDataLocation`，可在“关于 / 数据位置”确认路径。测试全部使用临时目录，不向真实题库写示例。

## 项目结构与实现

```text
include/orange/    数据模型、存储、计时及界面头文件
src/core/         模型校验、SQLite 仓储、单调时钟计时
src/gui/          Qt 界面与题目录入对话框
src/cli/          供后续问答录入与维护的命令行工具
tests/            持久化、索引、备份、计时、GUI 流程与公式渲染测试
third_party/      固定版本的本地数学渲染器、XML 解析库及许可文本
scripts/          Windows 编译打包与录入脚本
docs/             数据格式与数学教材章节目录
data/             根目录启动器使用的本地题库（不纳入版本控制）
```

界面与仓储分离，资源通过 RAII 管理，SQL 使用参数绑定。SQLite 对难度、章节、来源分别建立索引，并建立学科组合索引。编辑保存完整快照，时间戳用于并发更新校验。JSON 备份读取一致快照，并通过 QSaveFile 原子提交。数据库版本由 `PRAGMA user_version` 管理；较新格式会拒绝打开。关键词是字面子串检索，当前未建立全文索引。

完整源码注释使用中文。项目许可见 LICENSE，署名见 NOTICE，Qt 等组件许可见 THIRD_PARTY_NOTICES.md。
