// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QStringList>
class QComboBox;
namespace orange {
// 共享录入和筛选目录；组标题不可保存为章节，自定义标签保持兼容。
void populateChapters(QComboBox* combo, bool includeMath, const QStringList& extraTags,
                      const QString& firstText, const QString& firstValue);
} // namespace orange
