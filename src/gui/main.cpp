// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/main_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Orange");
    QCoreApplication::setApplicationName("OrangeStudy");
    QCoreApplication::setApplicationVersion("1.2.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Orange 考研助手 · 本地错题本与学习计时");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-dir", "指定本地题库目录", "directory", orange::defaultDataDir()});
    parser.addOption({"screenshot", "将界面保存为 PNG 后退出（用于界面验证）", "file"});
    parser.process(app);
    try {
        orange::Repository repository(parser.value("data-dir"));
        orange::MainWindow window(repository);
        window.show();
        if (parser.isSet("screenshot"))
            QTimer::singleShot(500, &app, [&] {
                const bool saved = window.grab().save(parser.value("screenshot"));
                app.exit(saved ? 0 : 1);
            });
        return app.exec();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "启动失败", QString::fromUtf8(e.what()));
        return 1;
    }
}
