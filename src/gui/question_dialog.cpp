// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/question_dialog.hpp"
#include "orange/chapter_picker.hpp"
#include "orange/math_browser.hpp"
#include "orange/reading_preferences.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace orange {
QuestionDialog::QuestionDialog(const Question& q, QWidget* parent) : QDialog(parent), original_(q) {
    setObjectName("questionDialog");
    setWindowTitle(q.id.isEmpty() ? "加入错题" : "编辑错题");
    resize(1150, 780);
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel("把错题变成下一次的得分点。* 为必填项；标签可以随时调整。");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* form = new QFormLayout;
    subject_ = new QComboBox;
    subject_->setObjectName("editSubject");
    for (const auto& s : subjects())
        subject_->addItem(subjectLabel(s), s);
    subject_->setCurrentIndex(subject_->findData(q.subject));
    difficulty_ = new QComboBox;
    difficulty_->setObjectName("editDifficulty");
    const QStringList labels{"1 · 入门", "2 · 基础", "3 · 中等", "4 · 较难", "5 · 挑战"};
    for (int i = 1; i <= 5; ++i)
        difficulty_->addItem(labels.at(i - 1), i);
    difficulty_->setCurrentIndex(q.difficulty - 1);
    status_ = new QComboBox;
    for (const auto& s : QStringList{"pending", "reviewing", "mastered"})
        status_->addItem(statusLabel(s), s);
    status_->setCurrentIndex(status_->findData(q.status));
    title_ = new QLineEdit(q.title);
    title_->setObjectName("editTitle");
    title_->setMaxLength(240);
    title_->setPlaceholderText("给题目起一个便于找回的标题");
    chapter_ = new QComboBox;
    chapter_->setObjectName("editChapter");
    chapter_->setEditable(true);
    chapter_->setInsertPolicy(QComboBox::NoInsert);
    chapter_->lineEdit()->setPlaceholderText("选择目录章节，或填写自定义章节；未知时填待标注");
    const auto reloadChapters = [this](const QString& selected) {
        populateChapters(chapter_, subject_->currentData() == "mathematics", {selected}, "待标注",
                         "待标注");
        chapter_->setCurrentText(selected);
    };
    reloadChapters(q.chapter);
    connect(subject_, &QComboBox::currentIndexChanged, this, [this, reloadChapters] {
        // 学科切换保留用户已输入的标签，不暗中重分类。
        reloadChapters(chapter_->currentText());
    });
    source_ = new QLineEdit(q.source);
    source_->setObjectName("editSource");
    source_->setPlaceholderText("例如：个人提问 / 教材页码 / 某年真题");
    form->addRow("学科 *", subject_);
    form->addRow("标题 *", title_);
    form->addRow("难度 *", difficulty_);
    form->addRow("章节 *", chapter_);
    form->addRow("来源 *", source_);
    form->addRow("复习状态", status_);
    layout->addLayout(form);
    auto* tabs = new QTabWidget;
    const auto editor = [tabs](const QString& label, const QString& text, const QString& name,
                               const QString& placeholder) {
        auto* edit = new QTextEdit;
        edit->setAcceptRichText(false);
        edit->setPlainText(text);
        edit->setObjectName(name);
        edit->setPlaceholderText(placeholder);
        tabs->addTab(edit, label);
        return edit;
    };
    content_ =
        editor("题目 *", q.content, "editContent",
               "输入完整题干。行内公式使用 $...$；独立公式使用 $$...$$。例如 $\\frac{1}{2}$。");
    answer_ = editor("答案与解析", q.answer, "editAnswer", "记录推导过程、标准答案和关键方法。");
    notes_ = editor("错因与笔记", q.notes, "editNotes", "当时为什么做错？下次需要注意什么？");
    conversation_ = editor("历史问答", q.conversation, "editConversation",
                           "粘贴提问、回答和追问；追加问答可保留完整上下文。");
    auto* split = new QSplitter;
    split->addWidget(tabs);
    auto* previewPanel = new QWidget;
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->addWidget(new QLabel("公式预览 · $...$ 行内公式，$$...$$ 独立公式"));
    previewLayout->addWidget(createReadingToolbar());
    preview_ = new MathBrowser;
    preview_->setObjectName("mathPreview");
    previewLayout->addWidget(preview_, 1);
    split->addWidget(previewPanel);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);
    auto* debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(250);
    const auto updatePreview = [this, tabs] {
        if (const auto* edit = qobject_cast<QTextEdit*>(tabs->currentWidget()))
            preview_->showText(edit->toPlainText());
    };
    for (auto* edit : {content_, answer_, notes_, conversation_})
        connect(edit, &QTextEdit::textChanged, debounce, [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, updatePreview);
    connect(tabs, &QTabWidget::currentChanged, this, updatePreview);
    updatePreview();
    // 输入源码与预览同步字号；不改变题目字符串或生成编辑快照。
    const auto resizeEditors = [this](int pixels) {
        for (auto* edit : {content_, answer_, notes_, conversation_}) {
            edit->setStyleSheet(QString("QTextEdit { font-size: %1px; }").arg(pixels));
            auto readingFont = edit->font();
            readingFont.setPixelSize(pixels);
            edit->document()->setDefaultFont(readingFont);
        }
    };
    auto& preferences = ReadingPreferences::instance();
    resizeEditors(preferences.fontSize());
    connect(&preferences, &ReadingPreferences::fontSizeChanged, this, resizeEditors);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setObjectName("saveQuestion");
    buttons->button(QDialogButtonBox::Save)->setText("保存错题");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        auto q = question();
        // 表单尚未持久化，用临时 ID/时间戳校验，实际写入由 Repository 处理。
        if (q.id.isEmpty())
            q.id = newId();
        if (q.createdAt.isEmpty())
            q.createdAt = nowIso();
        if (q.updatedAt.isEmpty())
            q.updatedAt = q.createdAt;
        try {
            validate(q);
            accept();
        } catch (const std::exception& e) {
            QMessageBox::warning(this, "请检查输入", QString::fromUtf8(e.what()));
        }
    });
}
Question QuestionDialog::question() const {
    auto q = original_;
    q.subject = subject_->currentData().toString();
    q.title = title_->text().trimmed();
    q.difficulty = difficulty_->currentData().toInt();
    q.chapter = chapter_->currentText().trimmed();
    q.source = source_->text().trimmed();
    q.status = status_->currentData().toString();
    q.content = content_->toPlainText();
    q.answer = answer_->toPlainText();
    q.notes = notes_->toPlainText();
    q.conversation = conversation_->toPlainText();
    return q;
}
} // namespace orange
