// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/study_timer.hpp"
#include <utility>
namespace orange {
StudyTimer::StudyTimer(Now now) : now_(std::move(now)) {}
void StudyTimer::start() {
    if (!running_) {
        start_ = now_();
        running_ = true;
    }
}
void StudyTimer::pause() {
    if (running_) {
        accumulated_ += now_() - start_;
        running_ = false;
    }
}
void StudyTimer::reset() {
    accumulated_ = {};
    running_ = false;
}
std::chrono::milliseconds StudyTimer::elapsed() const {
    const auto duration = accumulated_ + (running_ ? now_() - start_ : Clock::duration{});
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}
long long StudyTimer::seconds() const {
    return elapsed().count() / 1000;
}
} // namespace orange
