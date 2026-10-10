// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <chrono>
#include <functional>

namespace orange {
// 用单调时钟计算真实时长，不依赖 UI 刷新次数或系统日期变化。
// 注入时钟可精确测试暂停/继续，避免测试依赖 sleep。
class StudyTimer {
  public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    explicit StudyTimer(Now now = [] { return Clock::now(); });
    void start();
    void pause();
    void reset();
    bool running() const {
        return running_;
    }
    std::chrono::milliseconds elapsed() const;
    long long seconds() const;

  private:
    Now now_;
    Clock::time_point start_{};
    Clock::duration accumulated_{};
    bool running_ = false;
};
} // namespace orange
