// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/main_window.hpp"
#include "orange/analysis_page.hpp"
#include "orange/chapter_catalog.hpp"
#include "orange/chapter_picker.hpp"
#include "orange/math_browser.hpp"
#include "orange/question_dialog.hpp"
#include "orange/reading_preferences.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace orange {
namespace {
QString duration(qint64 seconds) {
    return QString("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds / 60) % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
QString localDate(const QString& iso) {
    return QDateTime::fromString(iso, Qt::ISODateWithMs).toLocalTime().toString("yyyy-MM-dd HH:mm");
}
QPushButton* button(const QString& text, const QString& name = {}) {
    auto* result = new QPushButton(text);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}
QLabel* title(const QString& text) {
    auto* result = new QLabel(text);
    result->setObjectName("sectionTitle");
    return result;
}
QTableWidget* table(const QStringList& columns) {
    auto* result = new QTableWidget(0, columns.size());
    result->setHorizontalHeaderLabels(columns);
    result->setSelectionBehavior(QAbstractItemView::SelectRows);
    result->setSelectionMode(QAbstractItemView::SingleSelection);
    result->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result->setAlternatingRowColors(true);
    result->verticalHeader()->hide();
    result->verticalHeader()->setDefaultSectionSize(48);
    result->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    result->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    result->setWordWrap(false);
    return result;
}
} // namespace
MainWindow::MainWindow(Repository& repository, QWidget* parent)
    : QMainWindow(parent), repository_(repository) {
    setWindowTitle("Orange 考研助手 · 839 / 数学（一）");
    resize(1360, 880);
    setMinimumSize(1100, 740);
    setStyleSheet(R"(
        QMainWindow, QDialog { background: #f7f5f0; color: #282d2a; }
        QWidget { font-family: 'Microsoft YaHei', 'Noto Sans CJK SC', 'PingFang SC'; font-size: 13px; color: #282d2a; }
        QLabel#brand { font-size: 28px; font-weight: bold; color: #c65a24; }
        QLabel#sectionTitle { font-size: 22px; font-weight: bold; }
        QLabel#muted { color: #777e76; }
        QFrame#card { background: #ffffff; border: 1px solid #e5e6de; border-radius: 12px; }
        QLabel#metric { font-size: 25px; font-weight: bold; color: #355548; }
        QListWidget { background: #eeeee5; border: none; border-radius: 12px; padding: 8px; outline: none; }
        QListWidget::item { padding: 18px 10px; border-radius: 8px; }
        QListWidget::item:selected { background: #355548; color: white; }
        QPushButton { background: #ffffff; border: 1px solid #dcded6; border-radius: 7px; padding: 9px 14px; }
        QPushButton:hover { background: #f6ebe2; border-color: #d68c57; }
        QPushButton:disabled { color: #a8aaa3; background: #f1f1ed; }
        QPushButton#addQuestion, QPushButton#timerStart, QPushButton#saveQuestion { background: #cb652f; color: white; border: none; }
        QLineEdit, QComboBox, QSpinBox { background: white; border: 1px solid #dcded6; border-radius: 6px; padding: 7px; min-height: 22px; }
        QTextEdit, QTextBrowser { background: white; border: 1px solid #e2e4dc; border-radius: 9px; padding: 12px; }
        QTableWidget { background: white; alternate-background-color: #fafaf6; border: 1px solid #e2e4dc; border-radius: 8px; gridline-color: #f0f0e8; }
        QTableWidget::item:selected { background: #e5eee5; color: #264338; }
        QHeaderView::section { background: #f0f1ea; border: none; padding: 12px 8px; font-weight: bold; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { padding: 13px 24px; margin-bottom: 10px; background: #eeeee5; }
        QTabBar::tab:selected { background: #355548; color: white; }
        QLabel#clock { font-size: 64px; font-weight: bold; color: #355548; }
        QStatusBar { background: #eeeee5; }
    )");
    auto* root = new QWidget;
    auto* outer = new QHBoxLayout(root);
    outer->setContentsMargins(24, 24, 24, 16);
    outer->setSpacing(24);
    auto* sidebar = new QVBoxLayout;
    auto* brand = new QLabel("Orange");
    brand->setObjectName("brand");
    sidebar->addWidget(brand);
    auto* caption = new QLabel("考研助手 / STUDY WITH FOCUS");
    caption->setObjectName("muted");
    sidebar->addWidget(caption);
    navigation_ = new QListWidget;
    navigation_->setObjectName("navigation");
    navigation_->setFixedWidth(230);
    navigation_->addItems({"专业课 · 839", "数学（一）", "英语", "政治", "全部错题"});
    navigation_->setCurrentRow(4);
    sidebar->addWidget(navigation_, 1);
    auto* target = new QLabel(
        "目标院校\n电子科技大学\n\n数学（一） · 专业课 839\n\n作者 Orange\nApache License 2.0");
    target->setWordWrap(true);
    target->setObjectName("muted");
    sidebar->addWidget(target);
    auto* about = button("关于 / 数据位置");
    sidebar->addWidget(about);
    outer->addLayout(sidebar);
    auto* main = new QVBoxLayout;
    main->setSpacing(18);
    auto* overview = new QWidget;
    auto* overviewLayout = new QVBoxLayout(overview);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->setSpacing(18);
    overviewLayout->addWidget(title("每一道错题，都是进步的入口"));
    auto* subtitle = new QLabel("本地题库 · 标签索引 · 历史问答 · 专注计时");
    subtitle->setObjectName("muted");
    overviewLayout->addWidget(subtitle);
    auto* metrics = new QHBoxLayout;
    const auto metric = [metrics](const QString& name) {
        auto* card = new QFrame;
        card->setObjectName("card");
        auto* layout = new QVBoxLayout(card);
        auto* label = new QLabel(name);
        label->setObjectName("muted");
        layout->addWidget(label);
        auto* value = new QLabel;
        value->setObjectName("metric");
        layout->addWidget(value);
        metrics->addWidget(card);
        return value;
    };
    count_ = metric("当前范围 · 错题数");
    pending_ = metric("当前范围 · 尚未掌握");
    today_ = metric("全部学科 · 今日已保存专注");
    overviewLayout->addLayout(metrics);
    main->addWidget(overview);
    auto* tabs = new QTabWidget;
    tabs->setObjectName("mainTabs");
    tabs->addTab(libraryPage(), "错题本");
    tabs->addTab(timerPage(), "学习计时");
    analysis_ = new AnalysisPage;
    tabs->addTab(analysis_, "错题分析");
    connect(tabs, &QTabWidget::currentChanged, this, [this, overview](int index) {
        // 分析页已有统计摘要，收起重复信息卡，为完整坐标轴留出高度。
        overview->setVisible(index != 2);
        if (index == 2)
            refresh(); // 进入分析页时同步 CLI 录入及其他窗口保存的数据。
    });
    connect(analysis_->findChild<QPushButton*>("refreshAnalysis"), &QPushButton::clicked,
            this, &MainWindow::refresh);
    main->addWidget(tabs, 1);
    outer->addLayout(main, 1);
    setCentralWidget(root);
    statusBar()->showMessage("题库：" + repository_.databasePath());
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
        currentSubject_ = row < 4 ? subjects().at(row) : QString();
        reloadTags();
        refresh();
    });
    connect(about, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this, "Orange 考研助手",
            "作者：Orange\n版本：1.4.1\n项目许可：Apache License 2.0\n\n"
            "考试配置：数学（一）、电子科技大学 "
            "839。\n数学目录按用户教材划分，其他学科章节自由填写。\n\n题库位置：\n" +
                repository_.databasePath() +
                "\n\n使用“导出备份”保存题目、编辑历史和学习计时。\n未来在本项目中明确要求记录错题，"
                "可通过配套 CLI 录入。\n"
                "本软件不自动读取其他聊天，也不内置在线 AI。\nQt 保留其原有许可证，详见 "
                "THIRD_PARTY_NOTICES.md。");
    });
    ticker_ = new QTimer(this);
    ticker_->setInterval(200);
    connect(ticker_, &QTimer::timeout, this, &MainWindow::updateTimer);
    ticker_->start();
    reloadTags();
    refresh();
    updateTimer();
}
void MainWindow::guarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "操作未完成", QString::fromUtf8(e.what()));
    }
}
QWidget* MainWindow::libraryPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tools = new QHBoxLayout;
    search_ = new QLineEdit;
    search_->setObjectName("search");
    search_->setClearButtonEnabled(true);
    search_->setPlaceholderText("搜索题目、解析、错因与历史问答…");
    tools->addWidget(search_, 1);
    auto* add = button("+ 加入错题", "addQuestion");
    tools->addWidget(add);
    auto* import = button("导入");
    tools->addWidget(import);
    auto* exportButton = button("导出备份");
    tools->addWidget(exportButton);
    auto* reload = button("刷新", "refresh");
    tools->addWidget(reload);
    layout->addLayout(tools);
    auto* filters = new QHBoxLayout;
    difficulty_ = new QComboBox;
    difficulty_->setObjectName("filterDifficulty");
    difficulty_->addItem("所有难度", 0);
    for (int i = 1; i <= 5; ++i)
        difficulty_->addItem("难度 " + QString::number(i), i);
    chapter_ = new QComboBox;
    chapter_->setObjectName("filterChapter");
    chapter_->setMinimumWidth(160);
    source_ = new QComboBox;
    source_->setObjectName("filterSource");
    source_->setMinimumWidth(160);
    status_ = new QComboBox;
    status_->setObjectName("filterStatus");
    status_->addItem("所有状态", "");
    for (const auto& s : QStringList{"pending", "reviewing", "mastered"})
        status_->addItem(statusLabel(s), s);
    filters->addWidget(difficulty_);
    filters->addWidget(chapter_, 1);
    filters->addWidget(source_, 1);
    filters->addWidget(status_);
    auto* catalog = button("数学章节目录", "chapterCatalog");
    filters->addWidget(catalog);
    connect(catalog, &QPushButton::clicked, this, &MainWindow::showChapterCatalog);
    auto* reset = button("清除筛选");
    filters->addWidget(reset);
    layout->addLayout(filters);
    resultCount_ = new QLabel;
    resultCount_->setObjectName("muted");
    layout->addWidget(resultCount_);
    auto* split = new QSplitter;
    questions_ = table({"题目", "难度", "章节", "来源", "状态"});
    // 长章节名称在提示中完整显示，避免目录名挤占题目阅读区域。
    questions_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    questions_->setColumnWidth(2, 220);
    questions_->setObjectName("questions");
    questions_->setMinimumWidth(470);
    split->addWidget(questions_);
    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->addWidget(createReadingToolbar());
    detail_ = new MathBrowser;
    detail_->setObjectName("questionDetail");
    detail_->setOpenExternalLinks(false);
    rightLayout->addWidget(detail_, 1);
    auto* actions = new QHBoxLayout;
    edit_ = button("编辑", "editQuestion");
    read_ = button("放大阅读", "readQuestion");
    reviewing_ = button("标记复习中", "markReviewing");
    mastered_ = button("标记已掌握", "markMastered");
    history_ = button("编辑历史");
    actions->addWidget(edit_);
    actions->addWidget(read_);
    actions->addWidget(reviewing_);
    actions->addWidget(mastered_);
    actions->addWidget(history_);
    rightLayout->addLayout(actions);
    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    layout->addWidget(split, 1);
    connect(add, &QPushButton::clicked, this, [this] { editQuestion(true); });
    connect(edit_, &QPushButton::clicked, this, [this] { editQuestion(false); });
    connect(read_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            const auto q = repository_.find(selectedId());
            if (!q)
                return;
            QDialog dialog(this);
            dialog.setObjectName("readingDialog");
            dialog.setWindowTitle(q->title);
            dialog.resize(1000, 800);
            auto* layout = new QVBoxLayout(&dialog);
            auto* readingHeader = new QHBoxLayout;
            readingHeader->addWidget(createReadingToolbar(), 1);
            auto* revealAnswer = button("显示答案（Enter）", "revealAnswer");
            readingHeader->addWidget(revealAnswer);
            // 全屏没有系统标题栏，关闭入口固定在正文滚动区域之外。
            auto* closeReading = button("关闭阅读", "closeReading");
            closeReading->setToolTip("返回题库（Esc）");
            readingHeader->addWidget(closeReading);
            connect(closeReading, &QPushButton::clicked, &dialog, &QDialog::reject);
            layout->addLayout(readingHeader);
            auto* browser = new MathBrowser;
            browser->setObjectName("readingContent");
            layout->addWidget(browser);
            browser->setAnswerVisible(false);
            browser->showQuestion(*q);
            const auto reveal = [browser, revealAnswer] {
                if (browser->answerVisible())
                    return;
                browser->setAnswerVisible(true);
                revealAnswer->setText("答案已显示");
                revealAnswer->setEnabled(false);
                browser->scrollToAnchor("answer");
                browser->setFocus();
            };
            connect(revealAnswer, &QPushButton::clicked, &dialog, reveal);
            // 主键盘回车与小键盘 Enter 均可展开；重复按键不关闭阅读窗口。
            for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
                auto* shortcut = new QShortcut(QKeySequence(key), &dialog);
                shortcut->setContext(Qt::WidgetWithChildrenShortcut);
                shortcut->setAutoRepeat(false);
                connect(shortcut, &QShortcut::activated, &dialog, reveal);
            }
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
            buttons->button(QDialogButtonBox::Close)->setText("关闭");
            buttons->button(QDialogButtonBox::Close)->setObjectName("readingCloseBottom");
            auto* reviewing = buttons->addButton("标记复习中", QDialogButtonBox::ActionRole);
            reviewing->setObjectName("readingMarkReviewing");
            reviewing->setEnabled(q->status != "reviewing");
            // 固定阅读题目的 ID：状态筛选刷新后，主界面的选中题目可能已变化。
            connect(reviewing, &QPushButton::clicked, &dialog,
                    [this, id = q->id, browser, reviewing] {
                        guarded([this, id, browser, reviewing] {
                            const auto updated = setQuestionStatus(id, "reviewing");
                            if (updated) {
                                browser->showQuestion(*updated);
                                reviewing->setEnabled(false);
                            }
                        });
                    });
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            for (auto* action : dialog.findChildren<QPushButton*>()) {
                action->setAutoDefault(false);
                action->setDefault(false);
            }
            // 默认全屏阅读；关闭按钮和 Esc 均可返回题库界面。
            dialog.showFullScreen();
            browser->setFocus();
            dialog.exec();
        });
    });
    connect(history_, &QPushButton::clicked, this, &MainWindow::showHistory);
    connect(reviewing_, &QPushButton::clicked, this, [this] {
        guarded([this] { setQuestionStatus(selectedId(), "reviewing"); });
    });
    connect(mastered_, &QPushButton::clicked, this, [this] {
        guarded([this] { setQuestionStatus(selectedId(), "mastered"); });
    });
    connect(questions_, &QTableWidget::itemSelectionChanged, this, &MainWindow::showSelected);
    connect(questions_, &QTableWidget::cellDoubleClicked, this, [this] { editQuestion(false); });
    for (auto* combo : {difficulty_, chapter_, source_, status_})
        connect(combo, &QComboBox::currentIndexChanged, this, &MainWindow::refresh);
    // 防抖输入，避免每个按键都刷新整个表格。
    auto* debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(180);
    connect(search_, &QLineEdit::textChanged, debounce, [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, &MainWindow::refresh);
    connect(reset, &QPushButton::clicked, this, [this] {
        QSignalBlocker a(difficulty_), b(chapter_), c(source_), d(status_), e(search_);
        difficulty_->setCurrentIndex(0);
        chapter_->setCurrentIndex(0);
        source_->setCurrentIndex(0);
        status_->setCurrentIndex(0);
        search_->clear();
        refresh();
    });
    connect(reload, &QPushButton::clicked, this, [this] {
        reloadTags();
        refresh();
    });
    connect(import, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, "导入错题或备份", {}, "JSON (*.json)");
        if (file.isEmpty())
            return;
        guarded([this, file] {
            const int n = repository_.importFile(file);
            reloadTags();
            refresh();
            QMessageBox::information(
                this, "导入完成", QString("新增 %1 道题。相同 ID 的相同记录已自动跳过。").arg(n));
        });
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getSaveFileName(
            this, "导出完整备份",
            "orange-backup-" + QDate::currentDate().toString("yyyyMMdd") + ".json",
            "JSON (*.json)");
        if (!file.isEmpty())
            guarded([this, file] {
                repository_.exportFile(file);
                statusBar()->showMessage("备份已保存：" + file, 10000);
            });
    });
    return page;
}
QWidget* MainWindow::timerPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* focus = new QVBoxLayout(card);
    focus->addWidget(title("留一段时间，只做一件事"));
    auto* settings = new QHBoxLayout;
    timerSubject_ = new QComboBox;
    timerSubject_->setObjectName("timerSubject");
    for (const auto& s : subjects())
        timerSubject_->addItem(subjectLabel(s), s);
    mode_ = new QComboBox;
    mode_->addItems({"正计时", "倒计时"});
    mode_->setObjectName("timerMode");
    minutes_ = new QSpinBox;
    minutes_->setRange(1, 600);
    minutes_->setValue(25);
    minutes_->setSuffix(" 分钟");
    minutes_->setEnabled(false);
    timerLabel_ = new QLineEdit;
    timerLabel_->setPlaceholderText("本次学习内容，例如：839 错题复习");
    settings->addWidget(timerSubject_);
    settings->addWidget(mode_);
    settings->addWidget(minutes_);
    settings->addWidget(timerLabel_, 1);
    focus->addLayout(settings);
    linkQuestion_ = new QCheckBox("关联错题本中当前选中的题目（开始时固定题目和学科）");
    focus->addWidget(linkQuestion_);
    clock_ = new QLabel("00:00:00");
    clock_->setObjectName("clock");
    clock_->setAlignment(Qt::AlignCenter);
    focus->addWidget(clock_);
    timerHint_ = new QLabel("准备好后开始。暂停时间不计入学习时长。");
    timerHint_->setAlignment(Qt::AlignCenter);
    timerHint_->setObjectName("muted");
    focus->addWidget(timerHint_);
    auto* actions = new QHBoxLayout;
    actions->addStretch();
    start_ = button("开始专注", "timerStart");
    save_ = button("结束并保存", "timerSave");
    auto* reset = button("重置", "timerReset");
    actions->addWidget(start_);
    actions->addWidget(save_);
    actions->addWidget(reset);
    actions->addStretch();
    focus->addLayout(actions);
    layout->addWidget(card);
    layout->addWidget(title("学习记录"));
    sessions_ = table({"学习内容", "学科", "时长", "完成时间"});
    sessions_->setObjectName("sessions");
    layout->addWidget(sessions_, 1);
    connect(start_, &QPushButton::clicked, this, &MainWindow::toggleTimer);
    connect(save_, &QPushButton::clicked, this, [this] { saveTimer(); });
    connect(mode_, &QComboBox::currentIndexChanged, this, [this] {
        minutes_->setEnabled(mode_->currentIndex() == 1);
        updateTimer();
    });
    connect(minutes_, &QSpinBox::valueChanged, this, &MainWindow::updateTimer);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (timer_.elapsed().count() > 0 &&
            QMessageBox::question(this, "重置计时", "丢弃本次尚未保存的计时？") != QMessageBox::Yes)
            return;
        clearTimer();
    });
    return page;
}
Filter MainWindow::filter() const {
    Filter f;
    f.subject = currentSubject_;
    f.difficulty = difficulty_->currentData().toInt();
    f.chapter = chapter_->currentData().toString();
    const auto group = chapter_->currentData().toMap().value("math_group").toString();
    if (!group.isEmpty()) {
        // 板块只汇总数学题的标准章节；旧标签单独放在“其他”目录。
        f.subject = "mathematics";
        f.chapter.clear();
        QStringList known;
        for (const auto& entry : mathChapters()) {
            known.append(entry.label());
            if (entry.group == group)
                f.chapters.append(entry.label());
        }
        if (group == "other")
            for (const auto& tag : repository_.tags("chapter", "mathematics"))
                if (!known.contains(tag))
                    f.chapters.append(tag);
        if (f.chapters.isEmpty())
            f.chapters.append(QStringLiteral("")); // 空目录不能退化为全部数学题。
    }
    f.source = source_->currentData().toString();
    f.status = status_->currentData().toString();
    f.keyword = search_->text();
    return f;
}
void MainWindow::reloadTags() {
    guarded([this] {
        for (const auto& item : {std::make_pair(chapter_, QString("chapter")),
                                 std::make_pair(source_, QString("source"))}) {
            const QSignalBlocker blocker(item.first);
            const auto selected = item.first->currentData();
            const auto tags = repository_.tags(item.second, currentSubject_);
            if (item.second == "chapter")
                populateChapters(item.first,
                                 currentSubject_.isEmpty() || currentSubject_ == "mathematics",
                                 tags, "所有章节", "", true);
            else {
                item.first->clear();
                item.first->addItem("所有来源", "");
                for (const auto& tag : tags)
                    item.first->addItem(tag, tag);
            }
            const int index = item.first->findData(selected);
            item.first->setCurrentIndex(index < 0 ? 0 : index);
        }
    });
}
void MainWindow::showChapterCatalog() {
    guarded([this] {
        QDialog dialog(this);
        dialog.setObjectName("chapterCatalogDialog");
        dialog.setWindowTitle("数学（一） · 教材章节目录");
        dialog.resize(1300, 850);
        auto* layout = new QVBoxLayout(&dialog);
        auto* hint = new QLabel("选择板块或章节，右侧即时列出其中全部数学题目；点击题目查看详情。"
                                "点击“查看题目”或双击目录，可返回题库并应用该目录筛选。");
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto* tree = new QTreeWidget;
        tree->setObjectName("chapterTree");
        tree->setHeaderLabels({"板块 / 讲次 / 章节", "教材页码", "错题数"});
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree->setColumnWidth(1, 65);
        tree->setColumnWidth(2, 60);
        Filter math;
        math.subject = "mathematics";
        QMap<QString, int> counts;
        const auto mathQuestions = repository_.list(math);
        for (const auto& q : mathQuestions)
            ++counts[q.chapter];
        QTreeWidgetItem* group = nullptr;
        QString previous;
        for (const auto& entry : mathChapters()) {
            if (entry.group != previous) {
                previous = entry.group;
                group = new QTreeWidgetItem(tree, {entry.group, "", "0"});
                group->setData(0, Qt::UserRole, QVariantMap{{"math_group", entry.group}});
                auto font = group->font(0);
                font.setBold(true);
                group->setFont(0, font);
            }
            const int count = counts.take(entry.label());
            auto* item = new QTreeWidgetItem(
                group, {entry.heading, QString::number(entry.page), QString::number(count)});
            item->setData(0, Qt::UserRole, entry.label());
            item->setData(0, Qt::UserRole + 1, QStringList{entry.label()});
            item->setToolTip(0, entry.label());
            group->setText(2, QString::number(group->text(2).toInt() + count));
            auto labels = group->data(0, Qt::UserRole + 1).toStringList();
            labels.append(entry.label());
            group->setData(0, Qt::UserRole + 1, labels);
        }
        if (!counts.isEmpty()) {
            auto* custom = new QTreeWidgetItem(tree, {"其他 / 待标注 / 自定义章节", "", "0"});
            custom->setData(0, Qt::UserRole, QVariantMap{{"math_group", "other"}});
            custom->setData(0, Qt::UserRole + 1, counts.keys());
            int total = 0;
            for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
                auto* item =
                    new QTreeWidgetItem(custom, {it.key(), "—", QString::number(it.value())});
                item->setData(0, Qt::UserRole, it.key());
                item->setData(0, Qt::UserRole + 1, QStringList{it.key()});
                total += it.value();
            }
            custom->setText(2, QString::number(total));
        }
        tree->expandAll();
        auto* split = new QSplitter;
        split->addWidget(tree);
        auto* right = new QWidget;
        auto* rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(8, 0, 0, 0);
        auto* summary = new QLabel;
        summary->setObjectName("catalogSummary");
        summary->setWordWrap(true);
        rightLayout->addWidget(summary);
        auto* readingSplit = new QSplitter(Qt::Vertical);
        auto* rows = table({"题目", "难度", "来源"});
        rows->setObjectName("catalogQuestions");
        rows->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        rows->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        rows->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        readingSplit->addWidget(rows);
        auto* preview = new MathBrowser;
        preview->setObjectName("catalogPreview");
        readingSplit->addWidget(preview);
        readingSplit->setStretchFactor(0, 1);
        readingSplit->setStretchFactor(1, 2);
        rightLayout->addWidget(readingSplit, 1);
        split->addWidget(right);
        split->setStretchFactor(0, 5);
        split->setStretchFactor(1, 6);
        split->setSizes({570, 690});
        layout->addWidget(split, 1);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto* view = buttons->addButton("查看题目", QDialogButtonBox::AcceptRole);
        view->setObjectName("viewChapter");
        view->setEnabled(false);
        connect(rows, &QTableWidget::itemSelectionChanged, &dialog,
                [rows, preview, mathQuestions] {
                    const auto* selected = rows->item(rows->currentRow(), 0);
                    if (!selected)
                        return;
                    for (const auto& q : mathQuestions)
                        if (q.id == selected->data(Qt::UserRole).toString()) {
                            preview->showQuestion(q);
                            return;
                        }
                });
        connect(tree, &QTreeWidget::itemSelectionChanged, &dialog,
                [tree, view, rows, preview, summary, mathQuestions] {
                    const auto* item = tree->currentItem();
                    view->setEnabled(item && item->data(0, Qt::UserRole).isValid());
                    const auto labels = item ? item->data(0, Qt::UserRole + 1).toStringList()
                                             : QStringList{};
                    rows->setRowCount(0);
                    preview->showText("当前目录暂无题目。");
                    // 使用同一份只读题目快照，确保列表、目录数量和预览一致。
                    for (const auto& q : mathQuestions) {
                        if (!labels.contains(q.chapter))
                            continue;
                        const int row = rows->rowCount();
                        rows->insertRow(row);
                        const QStringList cells{q.title, QString::number(q.difficulty), q.source};
                        for (int col = 0; col < cells.size(); ++col) {
                            auto* cell = new QTableWidgetItem(cells.at(col));
                            cell->setToolTip(cells.at(col));
                            if (col == 0)
                                cell->setData(Qt::UserRole, q.id);
                            rows->setItem(row, col, cell);
                        }
                    }
                    summary->setText(QString("%1 · %2 道题")
                                         .arg(item ? item->text(0) : QString("请选择目录"))
                                         .arg(rows->rowCount()));
                    if (rows->rowCount() > 0)
                        rows->selectRow(0);
        });
        const auto select = [this, tree, &dialog] {
            const auto* item = tree->currentItem();
            if (!item || !item->data(0, Qt::UserRole).isValid())
                return;
            const auto chapter = item->data(0, Qt::UserRole);
            // 打开目录章节时清除其他筛选，确保数量与目录一致；此操作不写入题库。
            navigation_->setCurrentRow(subjects().indexOf("mathematics"));
            reloadTags();
            QSignalBlocker a(difficulty_), b(chapter_), c(source_), d(status_), e(search_);
            difficulty_->setCurrentIndex(0);
            source_->setCurrentIndex(0);
            status_->setCurrentIndex(0);
            search_->clear();
            chapter_->setCurrentIndex(chapter_->findData(chapter));
            refresh();
            dialog.accept();
        };
        connect(view, &QPushButton::clicked, &dialog, select);
        connect(tree, &QTreeWidget::itemDoubleClicked, &dialog, [select] { select(); });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        // 重新打开时优先定位当前筛选目录，首次打开默认展示高等数学。
        tree->setCurrentItem(tree->topLevelItem(0));
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole) == chapter_->currentData()) {
                tree->setCurrentItem(*it);
                break;
            }
        dialog.exec();
    });
}
QString MainWindow::selectedId() const {
    const auto* item = questions_->item(questions_->currentRow(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}
void MainWindow::refresh() {
    guarded([this] {
        const auto selected = selectedId();
        const auto rows = repository_.list(filter());
        const QSignalBlocker blocker(questions_);
        questions_->setRowCount(rows.size());
        int selection = -1;
        for (int row = 0; row < rows.size(); ++row) {
            const auto& q = rows.at(row);
            const QStringList cells{q.title, QString::number(q.difficulty), q.chapter, q.source,
                                    statusLabel(q.status)};
            for (int col = 0; col < cells.size(); ++col) {
                auto* item = new QTableWidgetItem(cells.at(col));
                item->setToolTip(cells.at(col));
                if (col == 0)
                    item->setData(Qt::UserRole, q.id);
                questions_->setItem(row, col, item);
            }
            if (q.id == selected)
                selection = row;
        }
        if (selection < 0 && !rows.isEmpty())
            selection = 0;
        if (selection >= 0)
            questions_->selectRow(selection);
        else
            questions_->setCurrentCell(-1, -1);
        resultCount_->setText(
            rows.isEmpty() ? "暂无匹配题目。加入第一道错题，或清除筛选查看其他记录。"
                           : QString("找到 %1 道题 · 点击查看详情，双击编辑").arg(rows.size()));
        showSelected();
        Filter subject;
        subject.subject = currentSubject_;
        const auto all = repository_.list(subject);
        int pending = 0;
        for (const auto& q : all)
            if (q.status != "mastered")
                ++pending;
        count_->setText(QString::number(all.size()));
        pending_->setText(QString::number(pending));
        const auto records = repository_.sessions();
        qint64 today = 0;
        sessions_->setRowCount(records.size());
        for (int row = 0; row < records.size(); ++row) {
            const auto& s = records.at(row);
            if (QDateTime::fromString(s.finishedAt, Qt::ISODateWithMs).toLocalTime().date() ==
                QDate::currentDate())
                today += s.seconds;
            const QStringList cells{s.label.isEmpty() ? "专注学习" : s.label,
                                    subjectLabel(s.subject), duration(s.seconds),
                                    localDate(s.finishedAt)};
            for (int col = 0; col < cells.size(); ++col) {
                auto* item = new QTableWidgetItem(cells.at(col));
                item->setToolTip("记录 ID：" + s.id +
                                 (s.questionId.isEmpty() ? "" : "\n关联题目：" + s.questionId));
                sessions_->setItem(row, col, item);
            }
        }
        today_->setText(duration(today));
        analysis_->setQuestions(repository_.list(), currentSubject_);
    });
}
void MainWindow::showSelected() {
    guarded([this] {
        const auto q = repository_.find(selectedId());
        edit_->setEnabled(q.has_value());
        read_->setEnabled(q.has_value());
        reviewing_->setEnabled(q && q->status != "reviewing");
        mastered_->setEnabled(q && q->status != "mastered");
        history_->setEnabled(q.has_value());
        if (q)
            detail_->showQuestion(*q);
        else
            detail_->showText(
                "让错题有迹可循\n\n从“加入错题”开始，保存题目与解析。\n\n使用难度、章节和来源索引找"
                "回历史提问。\n\n启动显示全部错题；新增题目的默认科目为专业课839，章节按您的教材填写。");
    });
}
std::optional<Question> MainWindow::setQuestionStatus(const QString& id, const QString& status) {
    // 每次读取最新记录，保留题干、解析、笔记与历史问答；相同状态不重复写历史。
    auto q = repository_.find(id);
    if (!q)
        return std::nullopt;
    if (q->status != status) {
        q->status = status;
        repository_.update(*q);
    }
    refresh();
    return repository_.find(id);
}
void MainWindow::editQuestion(bool create) {
    guarded([this, create] {
        Question q;
        if (create)
            q.subject = currentSubject_.isEmpty() ? "professional" : currentSubject_;
        else {
            const auto found = repository_.find(selectedId());
            if (!found)
                return;
            q = *found;
        }
        QuestionDialog dialog(q, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (create)
            repository_.add(dialog.question());
        else
            repository_.update(dialog.question());
        reloadTags();
        refresh();
        statusBar()->showMessage("错题已保存到本地题库。", 5000);
    });
}
void MainWindow::showHistory() {
    guarded([this] {
        const auto records = repository_.history(selectedId());
        QDialog dialog(this);
        dialog.setWindowTitle("题目编辑历史");
        dialog.resize(820, 650);
        auto* layout = new QVBoxLayout(&dialog);
        auto* split = new QSplitter;
        auto* list = new QListWidget;
        auto* detail = new MathBrowser;
        auto* readingPanel = new QWidget;
        auto* readingLayout = new QVBoxLayout(readingPanel);
        readingLayout->setContentsMargins(0, 0, 0, 0);
        readingLayout->addWidget(createReadingToolbar());
        readingLayout->addWidget(detail, 1);
        split->addWidget(list);
        split->addWidget(readingPanel);
        split->setStretchFactor(1, 2);
        for (const auto& record : records)
            list->addItem(localDate(record.value("updated_at").toString()));
        connect(list, &QListWidget::currentRowChanged, &dialog, [records, detail](int row) {
            if (row >= 0 && row < records.size())
                detail->showQuestion(questionFromJson(records.at(row)));
        });
        layout->addWidget(split);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        list->setCurrentRow(0);
        dialog.exec();
    });
}
void MainWindow::toggleTimer() {
    guarded([this] {
        if (timer_.running()) {
            timer_.pause();
            updateTimer();
            return;
        }
        if (countdownFinished_)
            return;
        if (activeSession_.id.isEmpty()) {
            Session s;
            s.id = newId();
            s.subject = timerSubject_->currentData().toString();
            s.label = timerLabel_->text().trimmed();
            s.startedAt = nowIso();
            if (linkQuestion_->isChecked()) {
                const auto q = repository_.find(selectedId());
                if (!q)
                    throw std::runtime_error("请先在错题本中选中一道题目。");
                s.questionId = q->id;
                s.subject = q->subject;
                if (s.label.isEmpty())
                    s.label = q->title;
                timerSubject_->setCurrentIndex(timerSubject_->findData(s.subject));
            }
            activeSession_ = s;
            countdownSeconds_ = mode_->currentIndex() == 1 ? minutes_->value() * 60 : 0;
        }
        timer_.start();
        updateTimer();
    });
}
void MainWindow::updateTimer() {
    const bool active = !activeSession_.id.isEmpty();
    if (active && countdownSeconds_ > 0 && timer_.seconds() >= countdownSeconds_ &&
        !countdownFinished_) {
        timer_.pause();
        countdownFinished_ = true;
        QApplication::beep();
    }
    const qint64 seconds = active ? timer_.seconds() : 0;
    const qint64 target =
        active ? countdownSeconds_ : (mode_->currentIndex() == 1 ? minutes_->value() * 60 : 0);
    clock_->setText(duration(target > 0 ? qMax<qint64>(0, target - seconds) : seconds));
    start_->setText(timer_.running() ? "暂停" : (active ? "继续专注" : "开始专注"));
    start_->setEnabled(!countdownFinished_);
    save_->setEnabled(seconds >= 1);
    timerSubject_->setEnabled(!active);
    mode_->setEnabled(!active);
    minutes_->setEnabled(!active && mode_->currentIndex() == 1);
    timerLabel_->setEnabled(!active);
    linkQuestion_->setEnabled(!active);
    if (countdownFinished_)
        timerHint_->setText("本轮倒计时结束，请点击“结束并保存”记录学习时长。");
    else if (active)
        timerHint_->setText((timer_.running() ? "正在专注 · " : "已暂停 · ") +
                            subjectLabel(activeSession_.subject));
    else
        timerHint_->setText("准备好后开始。暂停时间不计入学习时长。");
}
bool MainWindow::saveTimer() {
    bool saved = false;
    guarded([this, &saved] {
        timer_.pause();
        if (timer_.seconds() < 1) {
            updateTimer();
            return;
        }
        auto s = activeSession_;
        s.seconds =
            countdownSeconds_ > 0 ? qMin(timer_.seconds(), countdownSeconds_) : timer_.seconds();
        s.finishedAt = nowIso();
        repository_.addSession(s);
        clearTimer();
        refresh();
        saved = true;
        statusBar()->showMessage("学习记录已保存。", 5000);
    });
    return saved;
}
void MainWindow::clearTimer() {
    timer_.reset();
    activeSession_ = {};
    countdownSeconds_ = 0;
    countdownFinished_ = false;
    updateTimer();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (activeSession_.id.isEmpty()) {
        event->accept();
        return;
    }
    timer_.pause();
    updateTimer();
    const auto choice = QMessageBox::question(
        this, "未保存的计时", "本次计时尚未保存。保存后退出？（不足一秒的计时无法保存）",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel || (choice == QMessageBox::Save && !saveTimer()))
        event->ignore();
    else
        event->accept();
}
} // namespace orange
