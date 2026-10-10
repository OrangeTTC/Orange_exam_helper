// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/chapter_catalog.hpp"
#include "orange/main_window.hpp"
#include "orange/math_browser.hpp"
#include "orange/question_dialog.hpp"
#include "orange/reading_preferences.hpp"
#include <QAbstractItemModel>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

using namespace orange;
class GuiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory_;
  private slots:
    void initTestCase() {
        QVERIFY(settingsDirectory_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
        QCoreApplication::setOrganizationName("OrangeTests");
    }
    void chapterDirectoryAndEditor() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        Question q;
        q.subject = "mathematics";
        q.title = "目录检索测试";
        q.content = "求矩阵的秩";
        q.chapter = mathChapters().at(26).label();
        const auto saved = repo.add(q);
        q.chapter = "旧章节标签";
        repo.add(q);
        q.chapter = mathChapters().last().label();
        repo.add(q);
        q.chapter = mathChapters().at(29).label();
        q.source = "分组来源";
        q.difficulty = 4;
        repo.add(q);
        q.chapter = mathChapters().first().label();
        repo.add(q);
        q.subject = "professional";
        q.chapter = saved.chapter;
        repo.add(q); // 同名专业课章节不能混进数学板块。
        QuestionDialog editor(saved);
        auto* combo = editor.findChild<QComboBox*>("editChapter");
        QVERIFY(combo);
        QCOMPARE(combo->currentText(), saved.chapter);
        QVERIFY(combo->findData(mathChapters().first().label()) > 0);
        const int groupIndex = combo->findText("线性代数");
        QVERIFY(groupIndex > 0);
        QVERIFY(!(combo->model()->flags(combo->model()->index(groupIndex, 0)) &
                  Qt::ItemIsEnabled)); // 录入仍必须选择具体章节，不能保存板块标题。
        combo->setCurrentText("自定义知识点");
        auto* subject = editor.findChild<QComboBox*>("editSubject");
        subject->setCurrentIndex(subject->findData("professional"));
        QCOMPARE(editor.question().chapter, QString("自定义知识点"));
        QCOMPARE(combo->findData(mathChapters().first().label()), -1);
        subject->setCurrentIndex(subject->findData("mathematics"));
        combo->setCurrentIndex(combo->findData(mathChapters().last().label()));
        QCOMPARE(editor.question().chapter, mathChapters().last().label());

        MainWindow window(repo);
        window.show();
        QTimer::singleShot(50, &window, [&window, saved] {
            auto* dialog = window.findChild<QDialog*>("chapterCatalogDialog");
            QVERIFY(dialog);
            auto* tree = dialog->findChild<QTreeWidget*>("chapterTree");
            QCOMPARE(tree->topLevelItemCount(), 4);
            QCOMPARE(tree->topLevelItem(0)->childCount(), 24);
            QCOMPARE(tree->topLevelItem(1)->childCount(), 7);
            QCOMPARE(tree->topLevelItem(2)->childCount(), 6);
            auto* matrix = tree->topLevelItem(1)->child(2);
            QCOMPARE(matrix->data(0, Qt::UserRole).toString(), saved.chapter);
            QCOMPARE(matrix->text(2), QString("1"));
            auto* previewRows = dialog->findChild<QTableWidget*>("catalogQuestions");
            QVERIFY(previewRows);
            for (int group = 0; group < 4; ++group) {
                tree->setCurrentItem(tree->topLevelItem(group));
                QVERIFY(dialog->findChild<QPushButton*>("viewChapter")->isEnabled());
                QCOMPARE(previewRows->rowCount(), group == 1 ? 2 : 1);
                QCOMPARE(tree->topLevelItem(group)->text(2),
                         QString::number(previewRows->rowCount()));
            }
            tree->setCurrentItem(tree->topLevelItem(0)->child(1));
            QCOMPARE(previewRows->rowCount(), 0);
            QVERIFY(dialog->findChild<QTextBrowser*>("catalogPreview")
                        ->toPlainText().contains("暂无题目"));
            tree->setCurrentItem(matrix);
            QCOMPARE(previewRows->rowCount(), 1);
            QCOMPARE(previewRows->item(0, 0)->data(Qt::UserRole).toString(), saved.id);
            QVERIFY(dialog->findChild<QTextBrowser*>("catalogPreview")
                        ->toPlainText().contains(saved.title));
            tree->setCurrentItem(tree->topLevelItem(1));
            const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
            if (!screenshotDir.isEmpty())
                QVERIFY(dialog->grab().save(screenshotDir + "/chapters.png"));
            QTest::mouseClick(dialog->findChild<QPushButton*>("viewChapter"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton*>("chapterCatalog"), Qt::LeftButton);
        auto* filter = window.findChild<QComboBox*>("filterChapter");
        QCOMPARE(filter->currentData().toMap().value("math_group").toString(),
                 QString("线性代数"));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 2);
        for (const auto& group : QStringList{"高等数学", "线性代数", "概率论与数理统计", "other"}) {
            filter->setCurrentIndex(filter->findData(QVariantMap{{"math_group", group}}));
            QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(),
                     group == "线性代数" ? 2 : 1);
        }
        filter->setCurrentIndex(filter->findData(QVariantMap{{"math_group", "线性代数"}}));
        auto* source = window.findChild<QComboBox*>("filterSource");
        source->setCurrentIndex(source->findData("分组来源"));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 1);
        source->setCurrentIndex(0);
        filter->setCurrentIndex(filter->findData(saved.chapter));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 1);
        QCOMPARE(window.findChild<QLineEdit*>("search")->text(), QString());
        // 空章节可检索，目录保留零题章节；浏览不修改历史。
        filter->setCurrentIndex(filter->findData(mathChapters().at(1).label()));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 0);
        filter->setCurrentIndex(filter->findData("旧章节标签"));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 1);
        QCOMPARE(repo.history(saved.id).size(), 1);
        QCOMPARE(repo.find(saved.id)->updatedAt, saved.updatedAt);
        QCOMPARE(repo.list().size(), 6);
    }
    void quickReviewingStatus() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        MainWindow window(repo);
        window.show();
        auto* reviewing = window.findChild<QPushButton*>("markReviewing");
        QVERIFY(reviewing);
        QVERIFY(!reviewing->isEnabled());
        Question q;
        q.title = "快捷复习测试";
        q.content = "保留题干 $x^2$";
        q.answer = "保留解析";
        q.notes = "保留错因";
        q.conversation = "保留历史问答";
        const auto first = repo.add(q);
        q.title = "全屏复习测试";
        const auto second = repo.add(q);
        window.refresh();
        auto* rows = window.findChild<QTableWidget*>("questions");
        auto* status = window.findChild<QComboBox*>("filterStatus");
        QVERIFY(rows);
        QVERIFY(status);
        const auto select = [rows](const QString& id) {
            for (int row = 0; row < rows->rowCount(); ++row)
                if (rows->item(row, 0)->data(Qt::UserRole).toString() == id) {
                    rows->selectRow(row);
                    return true;
                }
            return false;
        };
        QVERIFY(select(first.id));
        QTest::mouseClick(reviewing, Qt::LeftButton);
        auto saved = repo.find(first.id);
        QVERIFY(saved);
        QCOMPARE(saved->status, QString("reviewing"));
        QCOMPARE(saved->content, first.content);
        QCOMPARE(saved->answer, first.answer);
        QCOMPARE(saved->notes, first.notes);
        QCOMPARE(saved->conversation, first.conversation);
        QCOMPARE(repo.history(first.id).size(), 2);
        QVERIFY(!reviewing->isEnabled());
        QTest::mouseClick(reviewing, Qt::LeftButton);
        QCOMPARE(repo.history(first.id).size(), 2);
        // 保留原有“已掌握”操作，已掌握的题目也可重新进入复习。
        QTest::mouseClick(window.findChild<QPushButton*>("markMastered"), Qt::LeftButton);
        QCOMPARE(repo.find(first.id)->status, QString("mastered"));
        QVERIFY(reviewing->isEnabled());
        QTest::mouseClick(reviewing, Qt::LeftButton);
        QCOMPARE(repo.find(first.id)->status, QString("reviewing"));
        status->setCurrentIndex(status->findData("pending"));
        QCOMPARE(rows->rowCount(), 1);
        QVERIFY(select(second.id));
        // 全屏操作后当前题目会移出“待复习”列表，仍应更新原题的阅读内容。
        QTimer::singleShot(50, &window, [&window, &repo, rows, second] {
            auto* dialog = window.findChild<QDialog*>("readingDialog");
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QVERIFY(dialog->isFullScreen());
            auto* action = dialog->findChild<QPushButton*>("readingMarkReviewing");
            QVERIFY(action);
            QVERIFY(action->isEnabled());
            QTest::mouseClick(action, Qt::LeftButton);
            QCOMPARE(repo.find(second.id)->status, QString("reviewing"));
            QCOMPARE(rows->rowCount(), 0);
            QVERIFY(!window.findChild<QPushButton*>("markReviewing")->isEnabled());
            QVERIFY(!action->isEnabled());
            const auto text = dialog->findChild<QTextBrowser*>("readingContent")->toPlainText();
            QVERIFY(text.contains(second.title));
            QVERIFY(text.contains("复习中"));
            QTest::mouseClick(action, Qt::LeftButton);
            QCOMPARE(repo.history(second.id).size(), 2);
            const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
            if (!screenshotDir.isEmpty())
                QVERIFY(dialog->grab().save(screenshotDir + "/reviewing.png"));
            auto* close = dialog->findChild<QPushButton*>("closeReading");
            QVERIFY(close);
            QVERIFY(close->isVisible());
            QTest::mouseClick(close, Qt::LeftButton);
            QVERIFY(!dialog->isVisible());
            QVERIFY(window.isVisible());
        });
        QTest::mouseClick(window.findChild<QPushButton*>("readQuestion"), Qt::LeftButton);
        status->setCurrentIndex(status->findData("reviewing"));
        QCOMPARE(rows->rowCount(), 2);
        QVERIFY(select(second.id));
        QVERIFY(!reviewing->isEnabled());
        QTimer::singleShot(50, &window, [&window] {
            auto* dialog = window.findChild<QDialog*>("readingDialog");
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QVERIFY(!dialog->findChild<QPushButton*>("readingMarkReviewing")->isEnabled());
            auto* close = dialog->findChild<QPushButton*>("readingCloseBottom");
            QVERIFY(close);
            QVERIFY(close->isVisible());
            QCOMPARE(close->text(), QString("关闭"));
            QTest::mouseClick(close, Qt::LeftButton);
            QVERIFY(!dialog->isVisible());
        });
        QTest::mouseClick(window.findChild<QPushButton*>("readQuestion"), Qt::LeftButton);
        QCOMPARE(repo.history(second.id).size(), 2);
        QCOMPARE(repo.find(second.id)->conversation, second.conversation);
    }
    void addFilterEditAndTimer() {
        QTemporaryDir dir;
        Repository repository(dir.path());
        MainWindow window(repository);
        window.show();
        auto* rows = window.findChild<QTableWidget*>("questions");
        QVERIFY(rows);
        QCOMPARE(rows->rowCount(), 0);
        const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
        if (!screenshotDir.isEmpty())
            QVERIFY(window.grab().save(screenshotDir + "/empty.png"));
        auto* add = window.findChild<QPushButton*>("addQuestion");
        QVERIFY(add);
        // 进入真实模态录入界面，通过控件填写并触发保存。
        QTimer::singleShot(50, &window, [&window] {
            auto* dialog = window.findChild<QDialog*>("questionDialog");
            QVERIFY(dialog);
            dialog->findChild<QLineEdit*>("editTitle")->setText("GUI 新增测试");
            dialog->findChild<QTextEdit*>("editContent")
                ->setPlainText("<script>不会执行</script>\n中文题干 $\\frac{1}{2}$");
            dialog->findChild<QTextEdit*>("editConversation")->setPlainText("历史追问：需要复习");
            dialog->findChild<QComboBox*>("editChapter")->setCurrentText("自定义章节");
            dialog->findChild<QComboBox*>("editDifficulty")->setCurrentIndex(3);
            QTest::qWait(350);
            auto* preview =
                dynamic_cast<MathBrowser*>(dialog->findChild<QTextBrowser*>("mathPreview"));
            QVERIFY(preview);
            QCOMPARE(preview->formulaCount(), 1);
            QCOMPARE(preview->errorCount(), 0);
            dialog->findChild<QSpinBox*>("readingFontSize")->setValue(24);
            QCOMPARE(preview->document()->defaultFont().pixelSize(), 24);
            QCOMPARE(
                dialog->findChild<QTextEdit*>("editContent")->document()->defaultFont().pixelSize(),
                24);
            const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
            if (!screenshotDir.isEmpty())
                QVERIFY(dialog->grab().save(screenshotDir + "/editor.png"));
            QTest::mouseClick(dialog->findChild<QPushButton*>("saveQuestion"), Qt::LeftButton);
        });
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(repository.list().size(), 1);
        QCOMPARE(rows->rowCount(), 1);
        QCOMPARE(window.findChild<QSpinBox*>("readingFontSize")->value(), 24);
        QVERIFY(window.findChild<QTextBrowser*>("questionDetail")
                    ->toPlainText()
                    .contains("<script>不会执行</script>"));
        QTimer::singleShot(50, &window, [&window] {
            auto* dialog = window.findChild<QDialog*>("readingDialog");
            QVERIFY(dialog);
            auto* browser =
                dynamic_cast<MathBrowser*>(dialog->findChild<QTextBrowser*>("readingContent"));
            QVERIFY(browser);
            QCOMPARE(browser->formulaCount(), 1);
            QCOMPARE(browser->errorCount(), 0);
            auto* size = dialog->findChild<QSpinBox*>("readingFontSize");
            QCOMPARE(size->value(), 24);
            size->setValue(20);
            QCOMPARE(browser->document()->defaultFont().pixelSize(), 20);
            QCOMPARE(window.findChild<QSpinBox*>("readingFontSize")->value(), 20);
            const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
            if (!screenshotDir.isEmpty())
                QVERIFY(dialog->grab().save(screenshotDir + "/reading.png"));
            dialog->reject();
        });
        QTest::mouseClick(window.findChild<QPushButton*>("readQuestion"), Qt::LeftButton);
        QCOMPARE(repository.history(repository.list().first().id).size(), 1);
        auto* difficulty = window.findChild<QComboBox*>("filterDifficulty");
        difficulty->setCurrentIndex(3);
        QCOMPARE(rows->rowCount(), 0);
        difficulty->setCurrentIndex(4);
        QCOMPARE(rows->rowCount(), 1);
        auto* search = window.findChild<QLineEdit*>("search");
        search->setText("历史追问");
        QTest::qWait(250);
        QCOMPARE(rows->rowCount(), 1);
        auto* navigation = window.findChild<QListWidget*>("navigation");
        navigation->setCurrentRow(1);
        QCOMPARE(rows->rowCount(), 0);
        navigation->setCurrentRow(0);
        QCOMPARE(rows->rowCount(), 1);
        QTimer::singleShot(50, &window, [&window] {
            auto* dialog = window.findChild<QDialog*>("questionDialog");
            QVERIFY(dialog);
            dialog->findChild<QTextEdit*>("editAnswer")->setPlainText("图形界面编辑的解析");
            QTest::mouseClick(dialog->findChild<QPushButton*>("saveQuestion"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton*>("editQuestion"), Qt::LeftButton);
        QCOMPARE(repository.list().first().answer, QString("图形界面编辑的解析"));
        QCOMPARE(repository.history(repository.list().first().id).size(), 2);
        if (!screenshotDir.isEmpty())
            QVERIFY(window.grab().save(screenshotDir + "/library.png"));
        window.findChild<QTabWidget*>("mainTabs")->setCurrentIndex(1);
        auto* start = window.findChild<QPushButton*>("timerStart");
        auto* save = window.findChild<QPushButton*>("timerSave");
        QTest::mouseClick(start, Qt::LeftButton);
        QTest::qWait(1200);
        QTest::mouseClick(start, Qt::LeftButton);
        QVERIFY(save->isEnabled());
        QVERIFY(!window.findChild<QComboBox*>("timerSubject")->isEnabled());
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(repository.sessions().size(), 1);
        if (!screenshotDir.isEmpty())
            QVERIFY(window.grab().save(screenshotDir + "/timer.png"));
        QVERIFY(repository.sessions().first().seconds >= 1);
        QCOMPARE(window.findChild<QTableWidget*>("sessions")->rowCount(), 1);
        QVERIFY(window.findChild<QComboBox*>("timerSubject")->isEnabled());
        window.close();
    }
};
QTEST_MAIN(GuiTests)
#include "gui_tests.moc"
