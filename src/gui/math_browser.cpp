// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/math_browser.hpp"
#include "core/formula.h"
#include "latex.h"
#include "orange/reading_preferences.hpp"
#include "platform/qt/graphic_qt.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <cmath>
#include <memory>
#include <stdexcept>

// Q_INIT_RESOURCE 必须位于全局命名空间，确保静态库里的嵌入字体被链接。
static void initializeMathResources() {
    Q_INIT_RESOURCE(orange_math_fonts);
}

namespace orange {
namespace {
struct Engine {
    Engine() {
        initializeMathResources();
        tex::LaTeX::init(":/orange-tex");
    }
    ~Engine() {
        tex::LaTeX::release();
    }
};
QString escaped(const QString& text) {
    return text.toHtmlEscaped().replace("\n", "<br>");
}
bool isEscaped(const QString& text, qsizetype position) {
    int slashes = 0;
    while (position > 0 && text.at(--position) == '\\')
        ++slashes;
    return slashes % 2 == 1;
}
QString date(const QString& iso) {
    return QDateTime::fromString(iso, Qt::ISODateWithMs).toLocalTime().toString("yyyy-MM-dd HH:mm");
}
void validateFormula(const QString& source) {
    if (source.size() > 8000)
        throw std::runtime_error("公式过长，请拆分后显示。");
    int depth = 0;
    for (qsizetype i = 0; i < source.size(); ++i) {
        if (isEscaped(source, i))
            continue;
        if (source.at(i) == '{' && ++depth > 64)
            throw std::runtime_error("公式嵌套过深。");
        if (source.at(i) == '}' && --depth < 0)
            throw std::runtime_error("公式花括号不匹配。");
    }
    if (depth != 0)
        throw std::runtime_error("公式花括号不匹配。");
    // 渲染单个数学表达式，不接受文档、文件引用及跨公式的宏定义。
    for (const auto& command :
         QStringList{"\\input", "\\include", "\\def", "\\newcommand", "\\renewcommand", "\\csname"})
        if (source.contains(command))
            throw std::runtime_error("仅支持数学表达式，不支持文件引用或自定义宏。");
}
} // namespace
MathBrowser::MathBrowser(QWidget* parent) : QTextBrowser(parent) {
    document()->setDefaultStyleSheet("p { margin-top: 6px; margin-bottom: 6px; }");
    setOpenExternalLinks(false);
    setOpenLinks(false);
    auto& preferences = ReadingPreferences::instance();
    applyFontSize(preferences.fontSize());
    connect(&preferences, &ReadingPreferences::fontSizeChanged, this, [this](int pixels) {
        applyFontSize(pixels);
        render();
    });
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        std::unique_ptr<QMenu> menu(createStandardContextMenu());
        menu->addSeparator();
        auto* copy = menu->addAction("复制原始文本（含 LaTeX）");
        connect(copy, &QAction::triggered, this,
                [this] { QApplication::clipboard()->setText(sourceText()); });
        menu->exec(mapToGlobal(position));
    });
}
void MathBrowser::applyFontSize(int pixels) {
    fontSize_ = pixels;
    setStyleSheet(QString("QTextBrowser { font-size: %1px; background: white; padding: 12px; "
                          "border: 1px solid #e2e4dc; border-radius: 9px; }")
                      .arg(pixels));
    auto readingFont = font();
    readingFont.setPixelSize(pixels);
    document()->setDefaultFont(readingFont);
}
void MathBrowser::showQuestion(const Question& question) {
    question_ = question;
    text_.clear();
    render();
}
void MathBrowser::showText(const QString& text) {
    question_.reset();
    text_ = text;
    render();
}
void MathBrowser::setAnswerVisible(bool visible) {
    if (answerVisible_ == visible)
        return;
    answerVisible_ = visible;
    render();
}
QString MathBrowser::sourceText() const {
    if (!question_)
        return text_;
    const auto& q = *question_;
    if (!answerVisible_)
        return q.title + "\n\n题目\n" + q.content + "\n\n答案与解析已隐藏。";
    return q.title + "\n\n题目\n" + q.content + "\n\n答案与解析\n" + q.answer + "\n\n错因与笔记\n" +
           q.notes + "\n\n历史问答\n" + q.conversation;
}
QString MathBrowser::formulaHtml(const QString& source, bool display) {
    try {
        static Engine engine;
        validateFormula(source);
        tex::Formula formula(source.toStdWString());
        tex::TeXRenderBuilder builder;
        std::unique_ptr<tex::TeXRender> output(
            builder.setStyle(display ? tex::TexStyle::display : tex::TexStyle::text)
                .setTextSize((display ? 22.f : 18.f) * fontSize_ / 15.f)
                .setForeground(0xff282d2a)
                .build(formula));
        const int width = output->getWidth() + 8, height = output->getHeight() + 8;
        if (width <= 0 || height <= 0 || width > 16000 || height > 4000 ||
            static_cast<qint64>(width) * height > 8000000)
            throw std::runtime_error("公式尺寸超出显示范围，请拆分公式。");
        // 二倍像素密度保证分式、小下标在高 DPI 屏幕上也清晰。
        QImage image(width * 2, height * 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.scale(2, 2);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        tex::Graphics2D_qt graphics(&painter);
        output->draw(graphics, 4, 4);
        painter.end();
        const auto name = QUrl("orange-math:" + QString::number(formulaCount_++));
        document()->addResource(QTextDocument::ImageResource, name, image);
        // 保持实际字号；长公式通过横向滚动阅读，避免自适应缩小抵消字号调整。
        return QString("<img src='%1' width='%2' height='%3' align='middle' alt='%4' title='%4'>")
            .arg(name.toString())
            .arg(width)
            .arg(height)
            .arg(source.toHtmlEscaped());
    } catch (const std::exception& e) {
        ++errorCount_;
        return "<span style='color:#a73b28; font-family:monospace' title='" +
               QString::fromUtf8(e.what()).toHtmlEscaped() + "'>" +
               escaped((display ? "$$" : "$") + source + (display ? "$$" : "$")) + "</span>";
    }
}
QString MathBrowser::mathHtml(const QString& text) {
    QString html = "<p>";
    qsizetype plainStart = 0, position = 0;
    while (position < text.size()) {
        QString opening, closing;
        bool display = false;
        if (!isEscaped(text, position)) {
            if (text.mid(position, 2) == "$$") {
                opening = closing = "$$";
                display = true;
            } else if (text.at(position) == '$')
                opening = closing = "$";
            else if (text.mid(position, 2) == "\\[") {
                opening = "\\[";
                closing = "\\]";
                display = true;
            } else if (text.mid(position, 2) == "\\(") {
                opening = "\\(";
                closing = "\\)";
            }
        }
        if (opening.isEmpty()) {
            ++position;
            continue;
        }
        qsizetype end = position + opening.size();
        while ((end = text.indexOf(closing, end)) >= 0 && isEscaped(text, end))
            end += closing.size();
        // 未闭合的定界符保持原文，不吞掉后续内容。
        if (end < 0) {
            position += opening.size();
            continue;
        }
        auto plain = text.mid(plainStart, position - plainStart);
        // 独立公式自身会换段，去掉紧邻的一次换行，避免额外空白行。
        if (display && plain.endsWith('\n')) {
            plain.chop(1);
            if (plain.endsWith('\r'))
                plain.chop(1);
        }
        html += escaped(plain);
        const auto source = text.mid(position + opening.size(), end - position - opening.size());
        if (display)
            html += "</p><p align='center'>";
        html += formulaHtml(source, display);
        if (display)
            html += "</p><p>";
        position = end + closing.size();
        if (display && text.mid(position, 2) == "\r\n")
            position += 2;
        else if (display && position < text.size() && text.at(position) == '\n')
            ++position;
        plainStart = position;
    }
    return html + escaped(text.mid(plainStart)) + "</p>";
}
void MathBrowser::render() {
    const int scroll = verticalScrollBar()->value();
    formulaCount_ = 0;
    errorCount_ = 0;
    document()->clear();
    QString html;
    if (!question_)
        html = mathHtml(text_.isEmpty() ? "尚未填写" : text_);
    else {
        const auto& q = *question_;
        html = "<h2>" + q.title.toHtmlEscaped() + "</h2><p>" +
               subjectLabel(q.subject).toHtmlEscaped() + " · 难度 " +
               QString::number(q.difficulty) + " · " + statusLabel(q.status) + "</p><p>章节：" +
               q.chapter.toHtmlEscaped() + "<br>来源：" + q.source.toHtmlEscaped() + "</p>";
        html += "<h3>题目</h3>" + mathHtml(q.content);
        if (!answerVisible_)
            html += "<h3>答案与解析已隐藏</h3><p>请先尝试解题，再按 Enter 或点击“显示答案”查看。"
                    "<br>错因、笔记和历史问答也将在显示答案后展开。</p>";
        else {
            html += "<a name='answer'></a>";
            for (const auto& section : {std::make_pair(QString("答案与解析"), q.answer),
                                    std::make_pair(QString("错因与笔记"), q.notes),
                                    std::make_pair(QString("历史问答"), q.conversation)})
                html += "<h3>" + section.first + "</h3>" +
                        mathHtml(section.second.isEmpty() ? "尚未填写" : section.second);
        }
        html += "<hr><p>加入：" + date(q.createdAt) + "<br>更新：" + date(q.updatedAt) +
                "<br>ID：" + q.id + "</p>";
    }
    if (errorCount_)
        html.prepend(QString("<p style='color:#a73b28'>有 %1 "
                             "个公式未能渲染，已保留源码。请在编辑窗口检查公式。</p>")
                         .arg(errorCount_));
    setHtml(html);
    verticalScrollBar()->setValue(scroll);
}
void MathBrowser::resizeEvent(QResizeEvent* event) {
    QTextBrowser::resizeEvent(event);
    if (renderPending_)
        return;
    renderPending_ = true;
    QTimer::singleShot(120, this, [this] {
        renderPending_ = false;
        render();
    });
}
} // namespace orange
