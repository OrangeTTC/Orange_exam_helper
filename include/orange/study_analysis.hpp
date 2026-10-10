// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/model.hpp"
#include <QMap>

namespace orange {
struct AnalysisCount {
    QString key;
    QString label;
    int total = 0;
    int unmastered = 0;
};
struct StudyAnalysis {
    QVector<AnalysisCount> subjects;
    QMap<QString, QVector<AnalysisCount>> chapters;
};
// 统计已收录题数，非作答次数或错误率；不修改题库及编辑历史。
StudyAnalysis analyzeQuestions(const QVector<Question>& questions);
} // namespace orange
