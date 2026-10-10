// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/study_analysis.hpp"
#include <QWidget>
class QComboBox;
class QLabel;
class QTableWidget;

namespace orange {
class AnalysisChart;
class AnalysisPage : public QWidget {
  public:
    explicit AnalysisPage(QWidget* parent = nullptr);
    // 采用完整题库快照，避免错题本的关键词、来源筛选扭曲统计。
    void setQuestions(const QVector<Question>& questions, const QString& selectedSubject);

  private:
    void updateView();
    StudyAnalysis analysis_;
    QString sidebarSubject_;
    QComboBox *dimension_, *subject_, *group_;
    QLabel *subjectLabel_, *groupLabel_, *summary_;
    AnalysisChart* chart_;
    QTableWidget* ranking_;
};
} // namespace orange
