// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace orange {
// 数据层使用稳定英文枚举值；界面展示中文，避免修改文案影响已有题库。
QStringList subjects();
QString subjectLabel(const QString& subject);
QString statusLabel(const QString& status);
QString nowIso();
QString newId();
QString defaultDataDir();

struct Question {
    QString id;
    QString subject = "professional";
    QString title;
    QString content;
    QString answer;
    QString notes;
    QString conversation; // 保存历史问答原文或摘要，可包含多次追问。
    QString chapter = "待标注";
    QString source = "个人提问";
    int difficulty = 3; // 1 入门、2 基础、3 中等、4 较难、5 挑战。
    QString status = "pending";
    QString createdAt;
    QString updatedAt;
};
struct Filter {
    QString subject;
    int difficulty = 0; // 0 表示不限。
    QString chapter;
    QString source;
    QString status;
    QString keyword;
    QStringList chapters; // 目录汇总使用精确章节集合；空集合表示不限，不保存到题库。
};
struct Session {
    QString id;
    QString subject = "professional";
    QString questionId; // 可选；不关联具体题目时为空。
    QString label;
    qint64 seconds = 0;
    QString startedAt;
    QString finishedAt;
};
QJsonObject toJson(const Question& question);
Question questionFromJson(const QJsonObject& json);
QJsonObject toJson(const Session& session);
Session sessionFromJson(const QJsonObject& json);
void validate(const Question& question);
void validate(const Session& session);
} // namespace orange
