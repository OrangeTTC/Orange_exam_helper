// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/repository.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <stdexcept>

namespace orange {
namespace {
[[noreturn]] void fail(const QString& message) {
    throw std::runtime_error(message.toStdString());
}
QSqlQuery query(const QSqlDatabase& db, const QString& sql, const QVariantList& values = {}) {
    QSqlQuery result(db);
    if (!result.prepare(sql))
        fail("SQL 准备失败：" + result.lastError().text());
    for (auto value : values) {
        // Qt 的 null QString 会绑定成 SQL NULL；可选文本字段应储存空字符串。
        if (value.metaType().id() == QMetaType::QString && value.toString().isNull())
            value = QStringLiteral("");
        result.addBindValue(value);
    }
    if (!result.exec())
        fail("数据库操作失败：" + result.lastError().text());
    return result;
}
class Transaction {
  public:
    explicit Transaction(QSqlDatabase& db) : db_(db) {
        if (!db_.transaction())
            fail("无法开始事务：" + db_.lastError().text());
    }
    ~Transaction() {
        if (!committed_)
            db_.rollback();
    }
    void commit() {
        if (!db_.commit())
            fail("无法提交事务：" + db_.lastError().text());
        committed_ = true;
    }

  private:
    QSqlDatabase& db_;
    bool committed_ = false;
};
const QString columns = "id,subject,title,content,answer,notes,conversation,chapter,source,"
                        "difficulty,status,created_at,updated_at";
Question readQuestion(const QSqlQuery& q) {
    Question r;
    r.id = q.value(0).toString();
    r.subject = q.value(1).toString();
    r.title = q.value(2).toString();
    r.content = q.value(3).toString();
    r.answer = q.value(4).toString();
    r.notes = q.value(5).toString();
    r.conversation = q.value(6).toString();
    r.chapter = q.value(7).toString();
    r.source = q.value(8).toString();
    r.difficulty = q.value(9).toInt();
    r.status = q.value(10).toString();
    r.createdAt = q.value(11).toString();
    r.updatedAt = q.value(12).toString();
    return r;
}
QVariantList values(const Question& q) {
    return {q.id,      q.subject, q.title,      q.content, q.answer,    q.notes,    q.conversation,
            q.chapter, q.source,  q.difficulty, q.status,  q.createdAt, q.updatedAt};
}
void snapshot(const QSqlDatabase& db, const Question& question) {
    const auto text = QJsonDocument(toJson(question)).toJson(QJsonDocument::Compact);
    const auto hash = QCryptographicHash::hash(text, QCryptographicHash::Sha256).toHex();
    query(db, "INSERT OR IGNORE INTO question_history(question_id,hash,snapshot) VALUES(?,?,?)",
          {question.id, QString::fromLatin1(hash), QString::fromUtf8(text)});
}
QJsonArray arrayField(const QJsonObject& j, const QString& key, bool required = false) {
    if (!j.contains(key) && !required)
        return {};
    if (!j.value(key).isArray())
        fail("字段必须是数组：" + key);
    return j.value(key).toArray();
}
QJsonObject objectValue(const QJsonValue& v) {
    if (!v.isObject())
        fail("数组元素必须是 JSON 对象。");
    return v.toObject();
}
} // namespace
Repository::Repository(const QString& dataDir) : connectionName_(newId()) {
    if (dataDir.isEmpty() || !QDir().mkpath(dataDir))
        fail("无法创建数据目录：" + dataDir);
    path_ = QDir(dataDir).absoluteFilePath("orange-study.sqlite3");
    db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(path_);
    db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    try {
        if (!db_.open())
            fail("无法打开题库：" + db_.lastError().text());
        initialize();
    } catch (...) {
        db_.close();
        db_ = {};
        QSqlDatabase::removeDatabase(connectionName_);
        throw;
    }
}
Repository::~Repository() {
    db_.close();
    db_ = {};
    QSqlDatabase::removeDatabase(connectionName_);
}
QString Repository::databasePath() const {
    return path_;
}
void Repository::initialize() {
    query(db_, "PRAGMA foreign_keys=ON");
    query(db_, "PRAGMA journal_mode=WAL");
    auto version = query(db_, "PRAGMA user_version");
    version.next();
    const int schema = version.value(0).toInt();
    version.finish();
    if (schema > 1)
        fail("题库来自更新版本的软件，请升级后再打开。");
    Transaction transaction(db_);
    query(db_,
          "CREATE TABLE IF NOT EXISTS questions("
          "id TEXT PRIMARY KEY,subject TEXT NOT NULL CHECK(subject IN "
          "('professional','mathematics','english','politics')),"
          "title TEXT NOT NULL,content TEXT NOT NULL,answer TEXT NOT NULL,notes TEXT NOT "
          "NULL,conversation TEXT NOT NULL,"
          "chapter TEXT NOT NULL,source TEXT NOT NULL,difficulty INTEGER NOT NULL CHECK(difficulty "
          "BETWEEN 1 AND 5),"
          "status TEXT NOT NULL CHECK(status IN ('pending','reviewing','mastered')),created_at "
          "TEXT NOT NULL,updated_at TEXT NOT NULL)");
    // 单字段索引支持全学科标签检索；组合索引支持默认的专业课筛选。
    for (const auto& field : QStringList{"subject", "difficulty", "chapter", "source", "status"})
        query(db_,
              "CREATE INDEX IF NOT EXISTS idx_questions_" + field + " ON questions(" + field + ")");
    for (const auto& field : QStringList{"difficulty", "chapter", "source"})
        query(db_, "CREATE INDEX IF NOT EXISTS idx_subject_" + field + " ON questions(subject," +
                       field + ")");
    query(db_, "CREATE TABLE IF NOT EXISTS question_history("
               "sequence INTEGER PRIMARY KEY AUTOINCREMENT,question_id TEXT NOT NULL REFERENCES "
               "questions(id),"
               "hash TEXT NOT NULL,snapshot TEXT NOT NULL,UNIQUE(question_id,hash))");
    query(db_, "CREATE TABLE IF NOT EXISTS sessions(id TEXT PRIMARY KEY,subject TEXT NOT NULL,"
               "question_id TEXT REFERENCES questions(id),label TEXT NOT NULL,seconds INTEGER NOT "
               "NULL CHECK(seconds>0),"
               "started_at TEXT NOT NULL,finished_at TEXT NOT NULL)");
    query(db_, "CREATE INDEX IF NOT EXISTS idx_sessions_subject ON sessions(subject)");
    query(db_, "PRAGMA user_version=1");
    transaction.commit();
}
void Repository::insertQuestion(const Question& q) {
    query(db_, "INSERT INTO questions(" + columns + ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
          values(q));
}
Question Repository::add(Question q) {
    if (q.id.isEmpty())
        q.id = newId();
    q.title = q.title.trimmed();
    q.chapter = q.chapter.trimmed();
    q.source = q.source.trimmed();
    if (q.createdAt.isEmpty())
        q.createdAt = nowIso();
    if (q.updatedAt.isEmpty())
        q.updatedAt = q.createdAt;
    validate(q);
    Transaction transaction(db_);
    insertQuestion(q);
    snapshot(db_, q);
    transaction.commit();
    return q;
}
void Repository::update(Question q) {
    const auto previous = find(q.id);
    if (!previous)
        fail("题目不存在，可能已在其他窗口更改。");
    // 乐观锁：编辑表单携带原时间戳，防止 GUI 与命令行互相覆盖。
    if (q.updatedAt != previous->updatedAt)
        fail("题目已被其他窗口更新，请刷新后重试。");
    q.createdAt = previous->createdAt;
    const auto previousTime = QDateTime::fromString(previous->updatedAt, Qt::ISODateWithMs);
    q.updatedAt = qMax(QDateTime::currentDateTimeUtc(), previousTime.addMSecs(1))
                      .toUTC()
                      .toString(Qt::ISODateWithMs);
    q.title = q.title.trimmed();
    q.chapter = q.chapter.trimmed();
    q.source = q.source.trimmed();
    validate(q);
    Transaction transaction(db_);
    auto bindings = values(q);
    bindings.removeFirst();
    bindings.append(q.id);
    bindings.append(previous->updatedAt);
    const auto result =
        query(db_,
              "UPDATE questions SET subject=?,title=?,content=?,answer=?,notes=?,conversation=?,"
              "chapter=?,source=?,difficulty=?,status=?,created_at=?,updated_at=? WHERE id=? AND "
              "updated_at=?",
              bindings);
    if (result.numRowsAffected() != 1)
        fail("题目已被其他窗口更新，请刷新后重试。");
    snapshot(db_, q);
    transaction.commit();
}
std::optional<Question> Repository::find(const QString& id) const {
    auto result = query(db_, "SELECT " + columns + " FROM questions WHERE id=?", {id});
    if (!result.next())
        return std::nullopt;
    return readQuestion(result);
}
QVector<Question> Repository::list(const Filter& f) const {
    QString sql = "SELECT " + columns + " FROM questions WHERE 1=1";
    QVariantList bindings;
    const auto equal = [&](const QString& field, const QVariant& value) {
        sql += " AND " + field + "=?";
        bindings.append(value);
    };
    if (!f.subject.isEmpty())
        equal("subject", f.subject);
    if (f.difficulty != 0)
        equal("difficulty", f.difficulty);
    if (!f.chapter.isEmpty())
        equal("chapter", f.chapter);
    if (!f.source.isEmpty())
        equal("source", f.source);
    if (!f.status.isEmpty())
        equal("status", f.status);
    if (!f.keyword.trimmed().isEmpty()) {
        // instr 使用字面子串；用户输入 %、_、引号都不会变成 SQL 通配符。
        sql += " AND "
               "instr(lower(title||char(10)||content||char(10)||answer||char(10)||notes||char(10)||"
               "conversation),lower(?))>0";
        bindings.append(f.keyword.trimmed());
    }
    sql += " ORDER BY created_at DESC,id";
    auto result = query(db_, sql, bindings);
    QVector<Question> rows;
    while (result.next())
        rows.append(readQuestion(result));
    return rows;
}
QStringList Repository::tags(const QString& field, const QString& subject) const {
    if (!QStringList{"chapter", "source"}.contains(field))
        fail("不支持的标签索引。");
    QString sql = "SELECT DISTINCT " + field + " FROM questions";
    QVariantList bindings;
    if (!subject.isEmpty()) {
        sql += " WHERE subject=?";
        bindings.append(subject);
    }
    sql += " ORDER BY " + field;
    auto result = query(db_, sql, bindings);
    QStringList rows;
    while (result.next())
        rows.append(result.value(0).toString());
    return rows;
}
QVector<QJsonObject> Repository::history(const QString& id) const {
    auto result = query(
        db_, "SELECT snapshot FROM question_history WHERE question_id=? ORDER BY sequence DESC",
        {id});
    QVector<QJsonObject> rows;
    while (result.next())
        rows.append(QJsonDocument::fromJson(result.value(0).toByteArray()).object());
    return rows;
}
void Repository::insertSession(const Session& s) {
    if (!s.questionId.isEmpty()) {
        const auto q = find(s.questionId);
        if (!q)
            fail("计时关联题目不存在。");
    }
    query(db_,
          "INSERT INTO sessions(id,subject,question_id,label,seconds,started_at,finished_at) "
          "VALUES(?,?,?,?,?,?,?)",
          {s.id, s.subject, s.questionId.isEmpty() ? QVariant() : QVariant(s.questionId), s.label,
           s.seconds, s.startedAt, s.finishedAt});
}
void Repository::addSession(Session s) {
    if (s.id.isEmpty())
        s.id = newId();
    validate(s);
    insertSession(s);
}
QVector<Session> Repository::sessions() const {
    auto result = query(db_, "SELECT id,subject,question_id,label,seconds,started_at,finished_at "
                             "FROM sessions ORDER BY finished_at DESC,id");
    QVector<Session> rows;
    while (result.next()) {
        Session s;
        s.id = result.value(0).toString();
        s.subject = result.value(1).toString();
        s.questionId = result.value(2).toString();
        s.label = result.value(3).toString();
        s.seconds = result.value(4).toLongLong();
        s.startedAt = result.value(5).toString();
        s.finishedAt = result.value(6).toString();
        rows.append(s);
    }
    return rows;
}
QJsonObject Repository::exportJson() const {
    // 同一只读事务中的题目、历史和计时来自一致的数据库快照。
    auto connection = db_;
    Transaction transaction(connection);
    QJsonArray questions, sessionRows, histories;
    for (const auto& q : list()) {
        questions.append(toJson(q));
        auto snapshots = history(q.id);
        for (auto it = snapshots.crbegin(); it != snapshots.crend(); ++it)
            histories.append(QJsonObject{{"question_id", q.id}, {"snapshot", *it}});
    }
    for (const auto& s : sessions())
        sessionRows.append(toJson(s));
    transaction.commit();
    return {{"schema_version", 1},
            {"exported_at", nowIso()},
            {"questions", questions},
            {"sessions", sessionRows},
            {"history", histories}};
}
void Repository::exportFile(const QString& path) const {
    const auto target = QFileInfo(path).absoluteFilePath();
    for (const auto& protectedPath : QStringList{path_, path_ + "-wal", path_ + "-shm"}) {
        if (target.compare(QFileInfo(protectedPath).absoluteFilePath(), Qt::CaseInsensitive) == 0)
            fail("备份文件不能覆盖正在使用的数据库或辅助文件。");
    }
    const auto bytes = QJsonDocument(exportJson()).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        fail("无法保存备份：" + file.errorString());
}
int Repository::importJson(const QJsonObject& j) {
    if (j.value("schema_version").toDouble(-1) != 1)
        fail("不支持的导入版本；schema_version 必须为 1。");
    QVector<Question> questions;
    QVector<Session> sessionRows;
    QVector<Question> snapshots;
    QSet<QString> questionIds, sessionIds;
    // 先检查所有输入，事务内再检查与现有数据的冲突。任何错误都会整体回滚。
    for (const auto& v : arrayField(j, "questions", true)) {
        auto object = objectValue(v);
        const auto existing = find(object.value("id").toString());
        // 简化录入文件可省略时间戳；重试同一 UUID 时保留本地时间，仍比较实际内容。
        if (existing) {
            if (!object.contains("created_at"))
                object.insert("created_at", existing->createdAt);
            if (!object.contains("updated_at"))
                object.insert("updated_at", existing->updatedAt);
        }
        const auto q = questionFromJson(object);
        if (questionIds.contains(q.id))
            fail("导入文件中题目 ID 重复。");
        questionIds.insert(q.id);
        questions.append(q);
    }
    for (const auto& v : arrayField(j, "sessions")) {
        const auto s = sessionFromJson(objectValue(v));
        if (sessionIds.contains(s.id))
            fail("导入文件中计时 ID 重复。");
        sessionIds.insert(s.id);
        sessionRows.append(s);
    }
    for (const auto& v : arrayField(j, "history")) {
        const auto h = objectValue(v);
        if (!h.value("snapshot").isObject())
            fail("历史快照必须为对象。");
        const auto q = questionFromJson(h.value("snapshot").toObject());
        if (!h.value("question_id").isString() || h.value("question_id").toString() != q.id)
            fail("历史快照的题目 ID 不一致。");
        snapshots.append(q);
    }
    Transaction transaction(db_);
    int inserted = 0;
    for (const auto& q : questions) {
        const auto old = find(q.id);
        if (old) {
            if (toJson(*old) != toJson(q))
                fail("题目 ID 冲突，已取消全部导入：" + q.id);
        } else {
            insertQuestion(q);
            ++inserted;
        }
    }
    // 先还原历史的时间顺序，然后补充当前版本，重复快照按哈希跳过。
    for (const auto& q : snapshots) {
        if (!find(q.id))
            fail("历史记录引用不存在的题目。");
        snapshot(db_, q);
    }
    for (const auto& q : questions)
        snapshot(db_, q);
    for (const auto& s : sessionRows) {
        auto old = query(db_, "SELECT id FROM sessions WHERE id=?", {s.id});
        const bool exists = old.next();
        old.finish();
        if (exists) {
            bool same = false;
            for (const auto& existing : sessions())
                if (existing.id == s.id)
                    same = toJson(existing) == toJson(s);
            if (!same)
                fail("计时 ID 冲突，已取消全部导入：" + s.id);
        } else
            insertSession(s);
    }
    transaction.commit();
    return inserted;
}
int Repository::importFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail("无法读取导入文件：" + file.errorString());
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail("JSON 格式错误：" + error.errorString());
    return importJson(document.object());
}
} // namespace orange
