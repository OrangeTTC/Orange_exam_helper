// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/model.hpp"
#include <QTextBrowser>
#include <optional>

namespace orange {
// 将正文中的 LaTeX 公式在本地排版为高清图像；数据库始终保存原始文本。
// 支持 $...$、$$...$$、\(...\)、\[...\]，其余文字按纯文本转义。
class MathBrowser : public QTextBrowser {
  public:
    explicit MathBrowser(QWidget* parent = nullptr);
    void showQuestion(const Question& question);
    // 仅控制当前阅读器的显示与复制内容，字号及状态刷新不得重置此状态。
    void setAnswerVisible(bool visible);
    bool answerVisible() const {
        return answerVisible_;
    }
    void showText(const QString& text);
    QString sourceText() const;
    int formulaCount() const {
        return formulaCount_;
    }
    int errorCount() const {
        return errorCount_;
    }

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void applyFontSize(int pixels);
    void render();
    QString mathHtml(const QString& text);
    QString formulaHtml(const QString& source, bool display);
    std::optional<Question> question_;
    QString text_;
    int formulaCount_ = 0;
    int errorCount_ = 0;
    bool renderPending_ = false;
    bool answerVisible_ = true;
    int fontSize_ = 15;
};
} // namespace orange
