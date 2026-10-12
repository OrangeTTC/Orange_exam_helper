// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/chapter_catalog.hpp"
#include "orange/main_window.hpp"
#include "orange/math_browser.hpp"
#include "orange/question_dialog.hpp"
#include "orange/reading_preferences.hpp"
#include <QAbstractItemModel>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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
    void startupAndAnalysis() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        Question q;
        q.title = "分析统计测试";
        q.content = "仅用于临时题库";
        QString firstProfessional;
        for (int chapter = 1; chapter <= 40; ++chapter) {
            q.chapter = QString("第%1章 专业课测试章节").arg(chapter);
            const auto saved = repo.add(q);
            if (chapter == 1)
                firstProfessional = saved.id;
        }
        q.subject = "mathematics";
        q.chapter = mathChapters().first().label();
        repo.add(q);
        q.status = "reviewing";
        repo.add(q);
        q.status = "mastered";
        repo.add(q);
        q.status = "pending";
        q.chapter = "待标注";
        repo.add(q);
        MainWindow window(repo);
        window.show();
        auto* navigation = window.findChild<QListWidget*>("navigation");
        QCOMPARE(navigation->currentItem()->text(), QString("全部错题"));
        QCOMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 44);
        auto* tabs = window.findChild<QTabWidget*>("mainTabs");
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(2), QString("错题分析"));
        tabs->setCurrentIndex(2);
        QTest::qWait(50); // 等待收起概览卡后的布局，验证实际可见的坐标轴区域。
        auto* ranking = window.findChild<QTableWidget*>("analysisRanking");
        auto* dimension = window.findChild<QComboBox*>("analysisDimension");
        auto* subject = window.findChild<QComboBox*>("analysisSubject");
        auto* group = window.findChild<QComboBox*>("analysisMathGroup");
        auto* scroll = window.findChild<QScrollArea*>("analysisScroll");
        QVERIFY(ranking && dimension && subject && group && scroll);
        QCOMPARE(ranking->rowCount(), 4);
        QCOMPARE(ranking->item(0, 0)->data(Qt::UserRole).toString(), QString("professional"));
        QCOMPARE(ranking->item(0, 1)->text(), QString("40"));
        QCOMPARE(ranking->item(1, 1)->text(), QString("4"));
        QCOMPARE(ranking->item(1, 2)->text(), QString("3"));
        const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
        if (!screenshotDir.isEmpty())
            QVERIFY(window.grab().save(screenshotDir + "/analysis-subjects.png"));
        dimension->setCurrentIndex(dimension->findData("chapters"));
        QCOMPARE(ranking->rowCount(), 38);
        QCOMPARE(ranking->item(0, 0)->text(), mathChapters().first().label());
        QCOMPARE(ranking->item(0, 1)->text(), QString("3"));
        QCOMPARE(ranking->item(0, 2)->text(), QString("2"));
        QVERIFY(scroll->widget()->minimumWidth() >= 37 * 148);
        QTest::qWait(30);
        QVERIFY(scroll->horizontalScrollBar()->maximum() > 0);
        if (!screenshotDir.isEmpty()) {
            QVERIFY(window.grab().save(screenshotDir + "/analysis-math.png"));
            QVERIFY(scroll->widget()->grab().save(screenshotDir + "/analysis-math-full.png"));
        }
        QTimer::singleShot(50, &window, [&window, screenshotDir] {
            auto* dialog = window.findChild<QDialog*>("analysisChartDialog");
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QVERIFY(dialog->isFullScreen());
            auto* enlarged = dialog->findChild<QWidget*>("analysisChart");
            QVERIFY(enlarged);
            QVERIFY(enlarged->minimumWidth() >= 37 * 148);
            if (!screenshotDir.isEmpty())
                QVERIFY(dialog->grab().save(screenshotDir + "/analysis-enlarged.png"));
            QTest::mouseClick(dialog->findChild<QPushButton*>("closeAnalysisChart"), Qt::LeftButton);
            QVERIFY(!dialog->isVisible());
        });
        QTest::mouseClick(window.findChild<QPushButton*>("enlargeAnalysis"), Qt::LeftButton);
        group->setCurrentIndex(group->findData("线性代数"));
        QCOMPARE(ranking->rowCount(), 7);
        group->setCurrentIndex(group->findData("other"));
        QCOMPARE(ranking->rowCount(), 1);
        QCOMPARE(ranking->item(0, 0)->text(), QString("待标注"));
        subject->setCurrentIndex(subject->findData("professional"));
        QCOMPARE(ranking->rowCount(), 40);
        QVERIFY(scroll->widget()->minimumWidth() >= 40 * 148);
        QCOMPARE(ranking->item(1, 0)->text(), QString("第2章 专业课测试章节"));
        QTest::qWait(30);
        if (!screenshotDir.isEmpty())
            QVERIFY(window.grab().save(screenshotDir + "/analysis-professional.png"));
        // 关键词筛选不能截断分析；刷新应读取独立 CLI/其他窗口保存的状态。
        window.findChild<QLineEdit*>("search")->setText("不存在的关键词");
        QTRY_COMPARE(window.findChild<QTableWidget*>("questions")->rowCount(), 0);
        QCOMPARE(ranking->rowCount(), 40);
        auto changed = *repo.find(firstProfessional);
        changed.status = "mastered";
        repo.update(changed);
        QTest::mouseClick(window.findChild<QPushButton*>("refreshAnalysis"), Qt::LeftButton);
        QCOMPARE(ranking->item(39, 0)->data(Qt::UserRole).toString(), changed.chapter);
        QCOMPARE(ranking->item(39, 2)->text(), QString("0"));
        QCOMPARE(repo.history(firstProfessional).size(), 2);
        QCOMPARE(repo.list().size(), 44);
        subject->setCurrentIndex(subject->findData("english"));
        QCOMPARE(ranking->rowCount(), 0);
        QVERIFY(window.findChild<QLabel*>("analysisSummary")->text().contains("暂无"));
        // 左侧选择数学时同步章节科目，返回全部范围不丢失用户选择。
        navigation->setCurrentRow(1);
        QCOMPARE(subject->currentData().toString(), QString("mathematics"));
        navigation->setCurrentRow(4);
        QCOMPARE(subject->currentData().toString(), QString("mathematics"));
        QCOMPARE(repo.history(firstProfessional).size(), 2);
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
    void revealAnswerInReading() {
        QTemporaryDir dir;
        Repository repo(dir.path());
        Question q;
        q.title = "隐藏答案阅读测试";
        q.content = "请先思考：$x^2$";
        q.answer = "解析标记 $\\frac{1}{2}$";
        q.notes = "笔记标记";
        q.conversation = "问答标记";
        const auto saved = repo.add(q);
        MainWindow window(repo);
        window.show();
        // 每次重开都隐藏：覆盖按钮、双击题目、回车与字号控件焦点。
        for (int method = 0; method < 4; ++method) {
            QTimer::singleShot(50, &window, [&window, method] {
                auto* dialog = window.findChild<QDialog*>("readingDialog");
                QVERIFY(dialog);
                QVERIFY(dialog->isFullScreen());
                QTimer::singleShot(4000, dialog, &QDialog::reject);
                auto* browser = dynamic_cast<MathBrowser*>(
                    dialog->findChild<QTextBrowser*>("readingContent"));
                auto* reveal = dialog->findChild<QPushButton*>("revealAnswer");
                auto* close = dialog->findChild<QPushButton*>("closeReading");
                auto* size = dialog->findChild<QSpinBox*>("readingFontSize");
                QVERIFY(browser && reveal && close && size);
                QVERIFY(!browser->answerVisible());
                QVERIFY(reveal->isEnabled());
                QVERIFY(!browser->toPlainText().contains("解析标记"));
                QVERIFY(!browser->sourceText().contains("解析标记"));
                size->setValue(21);
                QTest::qWait(160);
                QVERIFY(!browser->answerVisible());
                if (method == 0) {
                    QTest::mouseClick(dialog->findChild<QPushButton*>("readingMarkReviewing"), Qt::LeftButton);
                    QVERIFY(!browser->answerVisible());
                    QVERIFY(!browser->toPlainText().contains("解析标记"));
                    const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
                    if (!screenshotDir.isEmpty())
                        QVERIFY(dialog->grab().save(screenshotDir + "/answer-hidden.png"));
                    QTest::mouseClick(reveal, Qt::LeftButton);
                } else if (method == 1) {
                    browser->setFocus();
                    QTest::keyClick(browser, Qt::Key_Return);
                } else if (method == 2) {
                    close->setFocus();
                    QTest::keyClick(close, Qt::Key_Enter);
                } else {
                    size->setFocus();
                    QTest::keyClick(size, Qt::Key_Return);
                }
                QTRY_VERIFY(browser->answerVisible());
                QVERIFY(dialog->isVisible());
                QVERIFY(!reveal->isEnabled());
                for (const auto& text : {"解析标记", "笔记标记", "问答标记"})
                    QVERIFY(browser->toPlainText().contains(text));
                QVERIFY(browser->sourceText().contains("解析标记"));
                QCOMPARE(browser->formulaCount(), 2);
                QCOMPARE(browser->errorCount(), 0);
                // 展开后回车也不触发默认关闭按钮。
                close->setFocus();
                QTest::keyClick(close, Qt::Key_Return);
                QVERIFY(dialog->isVisible());
                const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
                if (method == 0 && !screenshotDir.isEmpty())
                    QVERIFY(dialog->grab().save(screenshotDir + "/answer-revealed.png"));
                QTest::mouseClick(close, Qt::LeftButton);
            });
            if (method == 3) {
                auto* rows = window.findChild<QTableWidget*>("questions");
                QVERIFY(rows && rows->item(0, 0));
                const auto point = rows->visualItemRect(rows->item(0, 0)).center();
                QTest::mouseClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, point);
                QTest::mouseDClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, point);
            } else {
                QTest::mouseClick(window.findChild<QPushButton*>("readQuestion"), Qt::LeftButton);
            }
        }
        QCOMPARE(repo.find(saved.id)->answer, saved.answer);
        QCOMPARE(repo.find(saved.id)->conversation, saved.conversation);
        QCOMPARE(repo.history(saved.id).size(), 2); // 仅明确标记复习中产生一次编辑快照。
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
