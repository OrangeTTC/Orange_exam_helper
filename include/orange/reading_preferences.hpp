// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QObject>
class QWidget;

namespace orange {
// 阅读设置只改变显示；独立于题库，使用当前用户的 Qt 本地设置保存。
class ReadingPreferences : public QObject {
    Q_OBJECT
  public:
    static constexpr int minimumFontSize = 12;
    static constexpr int maximumFontSize = 32;
    static constexpr int defaultFontSize = 15;
    explicit ReadingPreferences(QObject* parent = nullptr);
    static ReadingPreferences& instance();
    int fontSize() const {
        return fontSize_;
    }
    void setFontSize(int pixels);
  signals:
    void fontSizeChanged(int pixels);

  private:
    int fontSize_ = defaultFontSize;
};

// 各阅读窗口共用同一份设置，控件与已经打开的窗口保持同步。
QWidget* createReadingToolbar(QWidget* parent = nullptr);
} // namespace orange
