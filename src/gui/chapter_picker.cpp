// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/chapter_picker.hpp"
#include "orange/chapter_catalog.hpp"
#include <QComboBox>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardItemModel>

namespace orange {
void populateChapters(QComboBox* combo, bool includeMath, const QStringList& extraTags,
                      const QString& firstText, const QString& firstValue) {
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(firstText, firstValue);
    QSet<QString> included{firstValue};
    const auto header = [combo](const QString& text) {
        combo->addItem(text);
        auto* model = qobject_cast<QStandardItemModel*>(combo->model());
        if (model) {
            auto* item = model->item(combo->count() - 1);
            item->setFlags(Qt::NoItemFlags);
            auto font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
    };
    if (includeMath) {
        QString group;
        for (const auto& entry : mathChapters()) {
            if (entry.group != group) {
                group = entry.group;
                header(group);
            }
            combo->addItem(entry.label(), entry.label());
            combo->setItemData(combo->count() - 1,
                               entry.label() + QString(" · 教材第%1页").arg(entry.page),
                               Qt::ToolTipRole);
            included.insert(entry.label());
        }
    }
    bool customHeader = false;
    for (const auto& tag : extraTags) {
        if (tag.isEmpty() || included.contains(tag))
            continue;
        if (includeMath && !customHeader) {
            header("其他 / 自定义章节");
            customHeader = true;
        }
        combo->addItem(tag, tag);
        included.insert(tag);
    }
    combo->setCurrentIndex(0);
    combo->setMaxVisibleItems(18);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(18);
}
} // namespace orange
