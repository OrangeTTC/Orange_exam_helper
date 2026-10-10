# Orange 集成说明

上游：<https://github.com/NanoMichael/MicroTeX>

固定提交：`0e3707f6dafebb121d98b53c64364d16fefe481d`。

引擎源代码使用 MIT 许可证，见 LICENSE。随库数学字体保留 `res/fonts/licences` 中的 Knuth、dsrom、SIL Open Font License 等原有许可证。

本项目仅收录引擎源代码及基础数学字体；不包含上游 Greek/Cyrillic 扩展资源。希腊数学字母使用引擎内置的 LaTeX 命令，例如 `\alpha`、`\beta`，中文正文由 Qt 绘制。

Orange 的修改：提供仅构建库的 C++17/Qt6 CMake 配置；Qt 字体加载改用 Qt6 的 QFontDatabase 静态接口；资源查询支持 Qt `:/` 路径。字体通过 Qt Resource 嵌入，避免运行时安装 TeX 或读取网络。
