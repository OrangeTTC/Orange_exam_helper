# 第三方组件

Orange 原创源码版权归 Orange，使用 Apache License 2.0；完整文本见 LICENSE。

本项目动态链接 Qt 6 的 Core、Gui、Widgets、Sql 模块。Qt 保留其 LGPLv3 / GPL / 商业许可证等原有许可，Qt 的测试模块只用于开发测试，不属于应用功能。项目的 Apache 2.0 声明不改变 Qt 或 Qt 内含第三方代码的许可证。Qt SQL 的 SQLite 驱动及其依赖保留各自的许可。

- Qt 官方许可说明：<https://www.qt.io/licensing/open-source-lgpl-obligations>
- Qt 6.8.3 qtbase 源码与许可文件：<https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz>
- SQLite 官方许可：<https://www.sqlite.org/copyright.html>
- GCC 运行库例外：<https://www.gnu.org/licenses/gcc-exception-3.1.html>

Windows 包使用 Qt 动态库与 MinGW 运行库。用户可自行替换兼容动态库、重新链接、为调试库修改而进行逆向工程，也可用公开项目源码重新编译。Windows 交付包中的 `third-party-sources/qtbase-everywhere-src-6.8.3.tar.xz` 提供本次 Qt 库及内含第三方组件的未修改源码，含完整版权声明与许可证；`third-party-licenses` 提供许可文本。重新打包其他版本时应提供对应源码与许可文本。

公式引擎 [MicroTeX](https://github.com/NanoMichael/MicroTeX) 固定于提交 `0e3707f6dafebb121d98b53c64364d16fefe481d`，以 MIT 许可证静态链接；源码及许可在 `third_party/MicroTeX`，Orange 的集成修改见其中的 `ORANGE-INTEGRATION.md`。基础数学字体保留 Knuth、dsrom、SIL Open Font License 等原许可，完整文件在 `res/fonts/licences`；程序嵌入字体副本，Windows 包附带同一组许可。本项目未包含上游 Greek/Cyrillic 扩展资源。

[tinyxml2 10.0.0](https://github.com/leethomason/tinyxml2/tree/10.0.0) 以 zlib 许可证静态链接，原作者 Lee Thomason 等；完整源码及许可在 `third_party/tinyxml2`。上述第三方组件及字体不受本项目 Apache-2.0 声明重新授权。
