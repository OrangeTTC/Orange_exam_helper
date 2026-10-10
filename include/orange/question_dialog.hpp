// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/model.hpp"
#include <QDialog>
class QComboBox;
class QLineEdit;
class QTextEdit;
namespace orange {
class MathBrowser;
class QuestionDialog : public QDialog {
  public:
    explicit QuestionDialog(const Question& question, QWidget* parent = nullptr);
    Question question() const;

  private:
    Question original_;
    QComboBox *subject_, *difficulty_, *status_;
    QLineEdit *title_, *chapter_, *source_;
    QTextEdit *content_, *answer_, *notes_, *conversation_;
    MathBrowser* preview_;
};
} // namespace orange
