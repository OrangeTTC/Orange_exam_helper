// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/repository.hpp"
#include "orange/study_timer.hpp"
#include <QMainWindow>
#include <functional>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextBrowser;
class QTimer;

namespace orange {
class MathBrowser;
class AnalysisPage;
class MainWindow : public QMainWindow {
  public:
    explicit MainWindow(Repository& repository, QWidget* parent = nullptr);
    void refresh();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    QWidget* libraryPage();
    QWidget* timerPage();
    void guarded(const std::function<void()>& action);
    void reloadTags();
    void showChapterCatalog();
    Filter filter() const;
    QString selectedId() const;
    void showSelected();
    std::optional<Question> setQuestionStatus(const QString& id, const QString& status);
    void editQuestion(bool create);
    void showHistory();
    void toggleTimer();
    bool saveTimer();
    void updateTimer();
    void clearTimer();
    Repository& repository_;
    QString currentSubject_; // 启动显示全部错题；新增题目的默认科目仍为专业课。
    QListWidget* navigation_;
    QTableWidget *questions_, *sessions_;
    MathBrowser* detail_;
    AnalysisPage* analysis_;
    QComboBox *difficulty_, *chapter_, *source_, *status_, *timerSubject_, *mode_;
    QLineEdit *search_, *timerLabel_;
    QLabel *count_, *pending_, *today_, *resultCount_, *clock_, *timerHint_;
    QPushButton *edit_, *read_, *reviewing_, *mastered_, *history_, *start_, *save_;
    QCheckBox* linkQuestion_;
    QSpinBox* minutes_;
    QTimer* ticker_;
    StudyTimer timer_;
    Session activeSession_;
    qint64 countdownSeconds_ = 0;
    bool countdownFinished_ = false;
};
} // namespace orange
