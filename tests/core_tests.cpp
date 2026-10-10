// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/chapter_catalog.hpp"
#include "orange/repository.hpp"
#include "orange/study_timer.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace orange;
class CoreTests : public QObject {
    Q_OBJECT
  private:
    static Question example(const QString& subject = "professional") {
        Question q;
        q.subject = subject;
        q.title = "中文标题 ' _ %";
        q.content = "题干\n包含公式 $x^2$ 和 UTF-8 🍊";
        q.chapter = "第一章";
        q.source = "个人提问";
        q.conversation = "问：为什么？\n答：历史问答索引测试";
        return q;
    }
  private slots:
    void textbookChapterCatalog() {
        const auto& chapters = mathChapters();
        QCOMPARE(chapters.size(), 37);
        QSet<QString> labels;
        QMap<QString, int> sizes;
        QMap<QString, int> lastPage;
        for (const auto& chapter : chapters) {
            QVERIFY(!labels.contains(chapter.label()));
            labels.insert(chapter.label());
            QVERIFY(chapter.page > lastPage.value(chapter.group));
            lastPage[chapter.group] = chapter.page;
            ++sizes[chapter.group];
        }
        QCOMPARE(sizes.value("高等数学"), 24);
        QCOMPARE(sizes.value("线性代数"), 7);
        QCOMPARE(sizes.value("概率论与数理统计"), 6);
        QVERIFY(chapters.at(9).heading.startsWith("第10讲 "));
        QVERIFY(chapters.at(24).heading.startsWith("第0讲 "));
        QCOMPARE(chapters.last().label(), QString("概率论与数理统计 · 第6讲 数理统计"));
    }
    void persistenceAndIndexes() {
        try {
            QTemporaryDir dir;
            QString id;
            {
                Repository repo(dir.path());
                id = repo.add(example()).id;
                repo.add(example("mathematics"));
                repo.add(example("english"));
                repo.add(example("politics"));
            }
            Repository repo(dir.path());
            QCOMPARE(repo.list().size(), 4);
            const auto q = repo.find(id);
            QVERIFY(q);
            QCOMPARE(q->content, example().content);
            Filter f;
            f.subject = "professional";
            f.difficulty = 3;
            f.chapter = "第一章";
            f.source = "个人提问";
            QCOMPARE(repo.list(f).size(), 1);
            f.difficulty = 4;
            QCOMPARE(repo.list(f).size(), 0);
            f.difficulty = 0;
            f.keyword = "历史问答索引";
            QCOMPARE(repo.list(f).size(), 1);
            f.keyword = "' _ %";
            QCOMPARE(repo.list(f).size(), 1);
            f.source = "' OR 1=1 --";
            QCOMPARE(repo.list(f).size(), 0);
            QCOMPARE(repo.tags("chapter", "professional"), QStringList{"第一章"});
            QVERIFY_EXCEPTION_THROWN(repo.tags("title; DROP TABLE questions"), std::runtime_error);
            // 检查真实 SQLite schema 中确有用户要求的三个索引。
            const auto connectionName = newId();
            {
                auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
                db.setDatabaseName(repo.databasePath());
                QVERIFY(db.open());
                QSqlQuery query(db);
                QVERIFY(query.exec("SELECT name FROM sqlite_master WHERE type='index'"));
                QStringList indexes;
                while (query.next())
                    indexes.append(query.value(0).toString());
                QVERIFY(indexes.contains("idx_questions_difficulty"));
                QVERIFY(indexes.contains("idx_questions_chapter"));
                QVERIFY(indexes.contains("idx_questions_source"));
            }
            QSqlDatabase::removeDatabase(connectionName);
        } catch (const std::exception& e) {
            QFAIL(e.what());
        }
    }
    void revisionAndConcurrentUpdate() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        auto q = repo.add(example());
        const auto stale = q;
        q.answer = "新的推导";
        q.status = "mastered";
        repo.update(q);
        QCOMPARE(repo.history(q.id).size(), 2);
        QCOMPARE(repo.history(q.id).last().value("answer").toString(), QString());
        QCOMPARE(repo.find(q.id)->answer, QString("新的推导"));
        QVERIFY_EXCEPTION_THROWN(repo.update(stale), std::runtime_error);
        QCOMPARE(repo.history(q.id).size(), 2);
    }
    void roundTripBackup() {
        QTemporaryDir original, restored;
        Repository repo(original.path());
        auto q = repo.add(example());
        q.notes = "易错点";
        repo.update(q);
        Session session;
        session.questionId = q.id;
        session.seconds = 75;
        session.startedAt = "2026-10-10T00:00:00.000Z";
        session.finishedAt = "2026-10-10T00:01:15.000Z";
        repo.addSession(session);
        const auto path = original.filePath("backup.json");
        repo.exportFile(path);
        Repository target(restored.path());
        QCOMPARE(target.importFile(path), 1);
        QCOMPARE(target.importFile(path), 0);
        QCOMPARE(target.list().size(), 1);
        QCOMPARE(target.history(q.id).size(), 2);
        QCOMPARE(target.sessions().size(), 1);
        QCOMPARE(target.sessions().first().seconds, 75);
        QCOMPARE(toJson(*target.find(q.id)), toJson(*repo.find(q.id)));
        QVERIFY_EXCEPTION_THROWN(repo.exportFile(repo.databasePath()), std::runtime_error);
    }
    void importRollbackAndValidation() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        auto q = repo.add(example());
        auto minimal = toJson(q);
        minimal.remove("created_at");
        minimal.remove("updated_at");
        QCOMPARE(
            repo.importJson(QJsonObject{{"schema_version", 1}, {"questions", QJsonArray{minimal}}}),
            0);
        auto fresh = q;
        fresh.id = newId();
        auto conflict = q;
        conflict.content = "冲突内容";
        QJsonObject bundle{{"schema_version", 1},
                           {"questions", QJsonArray{toJson(fresh), toJson(conflict)}}};
        QVERIFY_EXCEPTION_THROWN(repo.importJson(bundle), std::runtime_error);
        QCOMPARE(repo.list().size(), 1);
        auto invalid = toJson(fresh);
        invalid["difficulty"] = 1.5;
        bundle["questions"] = QJsonArray{toJson(fresh), invalid};
        QVERIFY_EXCEPTION_THROWN(repo.importJson(bundle), std::runtime_error);
        QCOMPARE(repo.list().size(), 1);
        invalid["difficulty"] = 3;
        invalid["content"] = QJsonValue::Null;
        QVERIFY_EXCEPTION_THROWN(questionFromJson(invalid), std::runtime_error);
        bundle["questions"] = QJsonArray{toJson(fresh)};
        Session broken;
        broken.id = newId();
        broken.questionId = newId();
        broken.seconds = 60;
        broken.startedAt = nowIso();
        broken.finishedAt = nowIso();
        bundle["sessions"] = QJsonArray{toJson(broken)};
        QVERIFY_EXCEPTION_THROWN(repo.importJson(bundle), std::runtime_error);
        QCOMPARE(repo.list().size(), 1);
        QVERIFY(repo.sessions().isEmpty());
        auto empty = example();
        empty.content.clear();
        QVERIFY_EXCEPTION_THROWN(repo.add(empty), std::runtime_error);
    }
    void sessionReferencesAndRetagging() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        auto q = repo.add(example("mathematics"));
        Session s;
        s.questionId = q.id;
        s.seconds = 10;
        s.startedAt = nowIso();
        s.finishedAt = nowIso();
        s.subject = "mathematics";
        repo.addSession(s);
        QCOMPARE(repo.sessions().size(), 1);
        q.subject = "professional";
        repo.update(q);
        QTemporaryDir restored;
        Repository target(restored.path());
        QCOMPARE(target.importJson(repo.exportJson()), 1);
        QCOMPARE(target.sessions().first().subject, QString("mathematics"));
        s.questionId = newId();
        QVERIFY_EXCEPTION_THROWN(repo.addSession(s), std::runtime_error);
    }
    void malformedFile() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        QFile file(dir.filePath("bad.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{broken}");
        file.close();
        QVERIFY_EXCEPTION_THROWN(repo.importFile(file.fileName()), std::runtime_error);
        QVERIFY(repo.list().isEmpty());
    }
    void timerPauseResumeAndReset() {
        StudyTimer::Clock::time_point time{};
        StudyTimer timer([&time] { return time; });
        timer.start();
        time += std::chrono::milliseconds(1250);
        QCOMPARE(timer.seconds(), 1LL);
        timer.start(); // 重复开始不清零。
        timer.pause();
        timer.pause();
        time += std::chrono::seconds(40);
        QCOMPARE(timer.elapsed().count(), 1250LL);
        timer.start();
        time += std::chrono::milliseconds(750);
        QCOMPARE(timer.seconds(), 2LL);
        timer.pause();
        timer.reset();
        QCOMPARE(timer.seconds(), 0LL);
        QVERIFY(!timer.running());
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
