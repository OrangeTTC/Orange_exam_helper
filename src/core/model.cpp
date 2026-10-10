// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/model.hpp"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>
#include <stdexcept>

namespace orange {
namespace {
[[noreturn]] void invalid(const QString& message) {
    throw std::runtime_error(message.toStdString());
}
QString stringField(const QJsonObject& json, const QString& key,
                    const QString& fallback = QStringLiteral("")) {
    if (!json.contains(key))
        return fallback;
    if (!json.value(key).isString())
        invalid("字段必须是字符串：" + key);
    return json.value(key).toString();
}
bool validDate(const QString& value) {
    return QDateTime::fromString(value, Qt::ISODateWithMs).isValid();
}
void checkId(const QString& id) {
    if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id)
        invalid("ID 必须是标准小写 UUID：" + id);
}
} // namespace
QStringList subjects() {
    return {"professional", "mathematics", "english", "politics"};
}
QString subjectLabel(const QString& subject) {
    if (subject == "professional")
        return "专业课 · 电子科技大学 839";
    if (subject == "mathematics")
        return "数学（一）";
    if (subject == "english")
        return "英语";
    if (subject == "politics")
        return "政治";
    return subject;
}
QString statusLabel(const QString& status) {
    if (status == "pending")
        return "待复习";
    if (status == "reviewing")
        return "复习中";
    if (status == "mastered")
        return "已掌握";
    return status;
}
QString nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QString defaultDataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
void validate(const Question& q) {
    checkId(q.id);
    if (!subjects().contains(q.subject))
        invalid("无效学科：" + q.subject);
    if (q.title.trimmed().isEmpty() || q.content.trimmed().isEmpty())
        invalid("标题和题目内容不能为空。");
    if (q.title.size() > 240)
        invalid("标题不能超过 240 个字符。");
    if (q.difficulty < 1 || q.difficulty > 5)
        invalid("难度必须为 1–5。");
    if (q.chapter.trimmed().isEmpty() || q.source.trimmed().isEmpty())
        invalid("章节和来源不能为空；未知时请填“待标注”。");
    if (!QStringList{"pending", "reviewing", "mastered"}.contains(q.status))
        invalid("无效复习状态。");
    if (!validDate(q.createdAt) || !validDate(q.updatedAt))
        invalid("题目时间戳必须为 ISO 8601 格式。");
}
void validate(const Session& s) {
    checkId(s.id);
    if (!s.questionId.isEmpty())
        checkId(s.questionId);
    if (!subjects().contains(s.subject))
        invalid("计时学科无效。");
    if (s.seconds <= 0 || s.seconds > 315360000)
        invalid("计时时长必须为正整数且不超过十年。");
    const auto start = QDateTime::fromString(s.startedAt, Qt::ISODateWithMs);
    const auto end = QDateTime::fromString(s.finishedAt, Qt::ISODateWithMs);
    if (!start.isValid() || !end.isValid())
        invalid("计时时间戳无效。");
}
QJsonObject toJson(const Question& q) {
    return {{"id", q.id},
            {"subject", q.subject},
            {"title", q.title},
            {"content", q.content},
            {"answer", q.answer},
            {"notes", q.notes},
            {"conversation", q.conversation},
            {"chapter", q.chapter},
            {"source", q.source},
            {"difficulty", q.difficulty},
            {"status", q.status},
            {"created_at", q.createdAt},
            {"updated_at", q.updatedAt}};
}
Question questionFromJson(const QJsonObject& j) {
    Question q;
    q.id = stringField(j, "id", newId());
    q.subject = stringField(j, "subject", "professional");
    q.title = stringField(j, "title");
    q.content = stringField(j, "content");
    q.answer = stringField(j, "answer");
    q.notes = stringField(j, "notes");
    q.conversation = stringField(j, "conversation");
    q.chapter = stringField(j, "chapter", "待标注");
    q.source = stringField(j, "source", "个人提问");
    q.status = stringField(j, "status", "pending");
    if (j.contains("difficulty")) {
        const double d = j.value("difficulty").toDouble(-1);
        if (!j.value("difficulty").isDouble() || d != std::floor(d) || d < 1 || d > 5)
            invalid("difficulty 必须为 1–5 的整数。");
        q.difficulty = static_cast<int>(d);
    }
    q.createdAt = stringField(j, "created_at", nowIso());
    q.updatedAt = stringField(j, "updated_at", q.createdAt);
    validate(q);
    return q;
}
QJsonObject toJson(const Session& s) {
    return {{"id", s.id},
            {"subject", s.subject},
            {"question_id", s.questionId},
            {"label", s.label},
            {"seconds", s.seconds},
            {"started_at", s.startedAt},
            {"finished_at", s.finishedAt}};
}
Session sessionFromJson(const QJsonObject& j) {
    Session s;
    s.id = stringField(j, "id", newId());
    s.subject = stringField(j, "subject", "professional");
    s.questionId = stringField(j, "question_id");
    s.label = stringField(j, "label");
    const double duration = j.value("seconds").toDouble(-1);
    if (!j.value("seconds").isDouble() || duration != std::floor(duration) || duration <= 0 ||
        duration > 315360000)
        invalid("seconds 必须为正整数且不超过十年。");
    s.seconds = static_cast<qint64>(duration);
    s.startedAt = stringField(j, "started_at");
    s.finishedAt = stringField(j, "finished_at");
    validate(s);
    return s;
}
} // namespace orange
