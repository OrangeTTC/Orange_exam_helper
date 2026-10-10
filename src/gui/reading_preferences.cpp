// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/reading_preferences.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>

namespace orange {
namespace {
const QString fontSizeKey = "reading/fontSizePx";
}
ReadingPreferences::ReadingPreferences(QObject* parent) : QObject(parent) {
    bool valid = false;
    const int saved = QSettings().value(fontSizeKey, defaultFontSize).toInt(&valid);
    fontSize_ = valid ? qBound(minimumFontSize, saved, maximumFontSize) : defaultFontSize;
}
ReadingPreferences& ReadingPreferences::instance() {
    static ReadingPreferences preferences;
    return preferences;
}
void ReadingPreferences::setFontSize(int pixels) {
    const int size = qBound(minimumFontSize, pixels, maximumFontSize);
    if (size == fontSize_)
        return;
    fontSize_ = size;
    QSettings settings;
    settings.setValue(fontSizeKey, size);
    settings.sync();
    emit fontSizeChanged(size);
}
QWidget* createReadingToolbar(QWidget* parent) {
    auto* toolbar = new QWidget(parent);
    toolbar->setObjectName("readingToolbar");
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto* label = new QLabel("字号", toolbar);
    auto* smaller = new QToolButton(toolbar);
    smaller->setText("A−");
    smaller->setObjectName("fontSmaller");
    smaller->setToolTip("缩小正文与公式");
    smaller->setAccessibleName("减小阅读字号");
    auto* size = new QSpinBox(toolbar);
    size->setObjectName("readingFontSize");
    size->setRange(ReadingPreferences::minimumFontSize, ReadingPreferences::maximumFontSize);
    size->setSuffix(" px");
    size->setKeyboardTracking(false);
    size->setToolTip("正文与公式同步缩放，设置自动保存");
    size->setAccessibleName("阅读字号");
    auto* larger = new QToolButton(toolbar);
    larger->setText("A+");
    larger->setObjectName("fontLarger");
    larger->setToolTip("放大正文与公式");
    larger->setAccessibleName("增大阅读字号");
    auto* reset = new QPushButton("默认", toolbar);
    reset->setObjectName("fontReset");
    reset->setToolTip("恢复默认字号 15 px");
    layout->addWidget(label);
    label->setBuddy(size);
    layout->addWidget(smaller);
    layout->addWidget(size);
    layout->addWidget(larger);
    layout->addWidget(reset);
    layout->addStretch();
    auto& preferences = ReadingPreferences::instance();
    const auto update = [size, smaller, larger](int pixels) {
        size->setValue(pixels);
        smaller->setEnabled(pixels > ReadingPreferences::minimumFontSize);
        larger->setEnabled(pixels < ReadingPreferences::maximumFontSize);
    };
    update(preferences.fontSize());
    QObject::connect(&preferences, &ReadingPreferences::fontSizeChanged, toolbar, update);
    QObject::connect(size, &QSpinBox::valueChanged, toolbar,
                     [&preferences](int pixels) { preferences.setFontSize(pixels); });
    QObject::connect(smaller, &QToolButton::clicked, toolbar,
                     [&preferences] { preferences.setFontSize(preferences.fontSize() - 1); });
    QObject::connect(larger, &QToolButton::clicked, toolbar,
                     [&preferences] { preferences.setFontSize(preferences.fontSize() + 1); });
    QObject::connect(reset, &QPushButton::clicked, toolbar, [&preferences] {
        preferences.setFontSize(ReadingPreferences::defaultFontSize);
    });
    return toolbar;
}
} // namespace orange
