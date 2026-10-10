// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/model.hpp"
#include <QSqlDatabase>
#include <optional>

namespace orange {
// 一个 Repository 对应当前线程的一条 SQLite 连接，不允许跨线程共享。
// 所有外部值均通过绑定参数传入 SQL；批量导入与编辑采用事务。
class Repository {
  public:
    explicit Repository(const QString& dataDir);
    ~Repository();
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    QString databasePath() const;
    Question add(Question question);
    void update(Question question);
    std::optional<Question> find(const QString& id) const;
    QVector<Question> list(const Filter& filter = {}) const;
    QStringList tags(const QString& field, const QString& subject = {}) const;
    QVector<QJsonObject> history(const QString& id) const;
    void addSession(Session session);
    QVector<Session> sessions() const;
    QJsonObject exportJson() const;
    void exportFile(const QString& path) const;
    // 已存在且内容完全相同的 ID 会跳过；冲突 ID 拒绝导入，防止静默覆盖。
    int importJson(const QJsonObject& json);
    int importFile(const QString& path);

  private:
    void initialize();
    void insertQuestion(const Question& question);
    void insertSession(const Session& session);
    QSqlDatabase db_;
    QString connectionName_;
    QString path_;
};
} // namespace orange
