// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/study_analysis.hpp"
#include "orange/chapter_catalog.hpp"
#include <QCollator>
#include <algorithm>

namespace orange {
StudyAnalysis analyzeQuestions(const QVector<Question>& questions) {
    StudyAnalysis result;
    QMap<QString, QMap<QString, AnalysisCount>> counts;
    for (const auto& subject : subjects()) {
        result.subjects.append({subject, subjectLabel(subject), 0, 0});
        counts.insert(subject, {});
    }
    for (const auto& question : questions) {
        for (auto& subject : result.subjects)
            if (subject.key == question.subject) {
                ++subject.total;
                subject.unmastered += question.status != "mastered";
                auto& chapter = counts[question.subject][question.chapter];
                chapter.key = chapter.label = question.chapter;
                ++chapter.total;
                chapter.unmastered += question.status != "mastered";
                break;
            }
    }
    for (const auto& subject : subjects()) {
        auto remaining = counts.value(subject);
        auto& chapters = result.chapters[subject];
        if (subject == "mathematics") {
            // 数学保留完整教材顺序与零题章节，其他旧标签追加到末尾。
            for (const auto& entry : mathChapters()) {
                const auto label = entry.label();
                chapters.append(remaining.take(label));
                chapters.last().key = chapters.last().label = label;
            }
        }
        // 非数学仅使用实际录入的章节，并按名称中的数字自然排序（第2章在第10章前）。
        auto custom = remaining.values();
        QCollator collator;
        collator.setNumericMode(true);
        std::stable_sort(custom.begin(), custom.end(), [&](const AnalysisCount& a, const AnalysisCount& b) {
            return collator.compare(a.label, b.label) < 0;
        });
        for (const auto& chapter : custom)
            chapters.append(chapter);
    }
    return result;
}
} // namespace orange
