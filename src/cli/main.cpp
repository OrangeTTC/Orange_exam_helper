// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/repository.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <stdexcept>

namespace {
void print(const QJsonObject& object) {
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
}
[[noreturn]] void invalid(const QString& text) {
    throw std::runtime_error(text.toStdString());
}
QJsonObject readObject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        invalid("无法读取文件：" + file.errorString());
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        invalid("无效 JSON 对象：" + error.errorString());
    return doc.object();
}
} // namespace
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Orange");
    app.setApplicationName("OrangeStudy");
    app.setApplicationVersion("1.3.0");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Orange 错题录入工具；与 GUI 共用 SQLite。输出为 UTF-8 JSON。\n"
        "命令：import 文件 | export 文件 | list | show ID | history ID | update 文件 | stats");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-dir", "本地题库目录", "directory", orange::defaultDataDir()});
    parser.addOption({"subject", "筛选学科 professional/mathematics/english/politics", "subject"});
    parser.addOption({"difficulty", "筛选难度 1–5", "number"});
    parser.addOption({"chapter", "按章节索引精确筛选", "chapter"});
    parser.addOption({"source", "按来源索引精确筛选", "source"});
    parser.addOption({"status", "筛选状态 pending/reviewing/mastered", "status"});
    parser.addOption({"search", "搜索题目、解析、笔记和历史问答", "keyword"});
    parser.addPositionalArgument("command", "操作命令");
    parser.addPositionalArgument("argument", "文件路径或题目 ID", "[argument]");
    parser.process(app);
    try {
        const auto args = parser.positionalArguments();
        if (args.isEmpty())
            parser.showHelp(0);
        const auto command = args.first();
        if (!QStringList{"import", "export", "list", "show", "history", "update", "stats"}.contains(
                command))
            invalid("未知命令，请使用 --help。");
        const bool needsArgument =
            QStringList{"import", "export", "show", "history", "update"}.contains(command);
        if (args.size() != (needsArgument ? 2 : 1))
            invalid("命令参数数量错误，请使用 --help。");
        orange::Repository repo(parser.value("data-dir"));
        if (command == "import")
            print({{"inserted_questions", repo.importFile(args.at(1))},
                   {"database", repo.databasePath()}});
        else if (command == "export") {
            repo.exportFile(args.at(1));
            print({{"exported", args.at(1)}});
        } else if (command == "list") {
            orange::Filter filter;
            filter.subject = parser.value("subject");
            filter.chapter = parser.value("chapter");
            filter.source = parser.value("source");
            filter.keyword = parser.value("search");
            filter.status = parser.value("status");
            if (!filter.subject.isEmpty() && !orange::subjects().contains(filter.subject))
                invalid("无效学科。");
            if (!filter.status.isEmpty() &&
                !QStringList{"pending", "reviewing", "mastered"}.contains(filter.status))
                invalid("无效状态。");
            if (parser.isSet("difficulty")) {
                bool ok;
                filter.difficulty = parser.value("difficulty").toInt(&ok);
                if (!ok || filter.difficulty < 1 || filter.difficulty > 5)
                    invalid("难度必须为 1–5。");
            }
            QJsonArray questions;
            for (const auto& q : repo.list(filter))
                questions.append(orange::toJson(q));
            print({{"questions", questions}, {"count", questions.size()}});
        } else if (command == "show" || command == "history") {
            const auto q = repo.find(args.at(1));
            if (!q)
                invalid("未找到题目 ID。");
            if (command == "show")
                print(orange::toJson(*q));
            else {
                QJsonArray history;
                for (const auto& h : repo.history(q->id))
                    history.append(h);
                print({{"history", history}});
            }
        } else if (command == "update") {
            const auto patch = readObject(args.at(1));
            if (!patch.value("id").isString())
                invalid("更新文件必须包含 id。");
            const auto q = repo.find(patch.value("id").toString());
            if (!q)
                invalid("题目不存在。");
            auto merged = orange::toJson(*q);
            for (auto it = patch.begin(); it != patch.end(); ++it) {
                if (!merged.contains(it.key()))
                    invalid("未知题目字段：" + it.key());
                merged.insert(it.key(), it.value());
            }
            repo.update(orange::questionFromJson(merged));
            print(orange::toJson(*repo.find(q->id)));
        } else if (command == "stats") {
            QJsonObject totals;
            QJsonArray sessions;
            for (const auto& s : orange::subjects())
                totals.insert(s, 0);
            for (const auto& s : repo.sessions()) {
                totals.insert(s.subject, totals.value(s.subject).toInteger() + s.seconds);
                sessions.append(orange::toJson(s));
            }
            print({{"question_count", repo.list().size()},
                   {"seconds_by_subject", totals},
                   {"sessions", sessions}});
        }
        return 0;
    } catch (const std::exception& e) {
        const auto bytes = QString("错误：%1\n").arg(QString::fromUtf8(e.what())).toUtf8();
        fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr);
        return 1;
    }
}
