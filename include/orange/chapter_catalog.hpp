// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QVector>

namespace orange {
// 目录依据用户提供的教材照片；标签仍存入 schema 1 的单值 chapter 字段。
struct ChapterEntry {
    QString group;
    QString heading;
    int page;
    QString label() const {
        return group + " · " + heading;
    }
};
const QVector<ChapterEntry>& mathChapters();
} // namespace orange
