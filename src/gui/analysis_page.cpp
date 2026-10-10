// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/analysis_page.hpp"
#include "orange/chapter_catalog.hpp"
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace orange {
// 使用 Qt Widgets 自绘，无额外图表运行库。横向扩展保障数学 37 章及更多章节可读。
class AnalysisChart : public QWidget {
  public:
    explicit AnalysisChart(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("analysisChart");
        setMouseTracking(true);
        setMinimumHeight(460);
        setAccessibleName("错题数量折线图");
    }
    void setCounts(const QVector<AnalysisCount>& counts, bool chapters) {
        counts_ = counts;
        chapters_ = chapters;
        setMinimumWidth(chapters ? qMax(760, int(counts.size()) * 148 + 96) : 760);
        QStringList description;
        for (const auto& count : counts)
            description.append(QString("%1：全部 %2，尚未掌握 %3")
                                   .arg(count.label).arg(count.total).arg(count.unmastered));
        setAccessibleDescription(description.join("；"));
        update();
    }
    void copyFrom(const AnalysisChart& chart) {
        setCounts(chart.counts_, chart.chapters_);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), Qt::white);
        auto textFont = font();
        textFont.setPixelSize(13);
        painter.setFont(textFont);
        const QColor orange("#c65a24"), green("#355548"), ink("#454d46");
        painter.setPen(QPen(orange, 3));
        painter.drawLine(68, 25, 96, 25);
        painter.setPen(ink);
        painter.drawText(104, 30, "全部错题");
        painter.setPen(QPen(green, 3, Qt::DashLine));
        painter.drawLine(225, 25, 253, 25);
        painter.setPen(ink);
        painter.drawText(261, 30, "尚未掌握");
        const QRectF plot(64, 75, width() - 96, height() - 270);
        if (counts_.isEmpty()) {
            painter.drawText(rect(), Qt::AlignCenter, "暂无章节记录，录入题目并标注章节后可查看分布。");
            return;
        }
        int maximum = 1;
        for (const auto& count : counts_)
            maximum = qMax(maximum, count.total);
        const int tick = qMax(1, int(std::ceil(maximum / 5.0)));
        const int ceiling = ((maximum + tick - 1) / tick) * tick;
        painter.drawText(12, 58, "题数");
        for (int value = 0; value <= ceiling; value += tick) {
            const auto y = plot.bottom() - plot.height() * value / ceiling;
            painter.setPen(QPen(QColor("#e5e9e2"), 1));
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.setPen(ink);
            painter.drawText(QRectF(8, y - 10, 45, 20), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(value));
        }
        const auto step = plot.width() / counts_.size();
        const auto point = [&](int index, int value) {
            return QPointF(plot.left() + (index + 0.5) * step,
                           plot.bottom() - plot.height() * value / ceiling);
        };
        for (const bool outstanding : {false, true}) {
            const auto color = outstanding ? green : orange;
            QPainterPath path;
            for (int i = 0; i < counts_.size(); ++i) {
                const auto position = point(i, outstanding ? counts_[i].unmastered : counts_[i].total);
                if (i == 0)
                    path.moveTo(position);
                else
                    path.lineTo(position);
            }
            painter.setPen(QPen(color, 2.5, outstanding ? Qt::DashLine : Qt::SolidLine));
            painter.setBrush(Qt::NoBrush); // 折线不填充，防止上一次绘制圆点的画刷形成遮挡。
            painter.drawPath(path);
            for (int i = 0; i < counts_.size(); ++i) {
                const int value = outstanding ? counts_[i].unmastered : counts_[i].total;
                const auto position = point(i, value);
                painter.setBrush(outstanding ? Qt::white : color);
                painter.drawEllipse(position, 4.5, 4.5);
                // 重合点仅标一次数量，避免数字相互遮挡。
                if (outstanding && counts_[i].unmastered == counts_[i].total)
                    continue;
                painter.setPen(color);
                painter.drawText(QRectF(position.x() - 25,
                                        position.y() + (outstanding ? 9 : -26), 50, 20),
                                 Qt::AlignCenter, QString::number(value));
            }
        }
        painter.setPen(ink);
        for (int i = 0; i < counts_.size(); ++i) {
            auto label = counts_[i].label;
            // 目录标签拆行，保持讲次顺序；完整长名也在悬停提示与排行表中显示。
            label.replace(" · ", "\n");
            if (chapters_)
                for (const auto& entry : mathChapters())
                    if (entry.label() == counts_[i].key) {
                        const int space = entry.heading.indexOf(' ');
                        label = entry.group + "\n" + entry.heading.left(space) + "\n" +
                                entry.heading.mid(space + 1);
                        break;
                    }
            const auto x = point(i, 0).x();
            painter.drawText(QRectF(x - step / 2 + 8, plot.bottom() + 42, step - 16, 137),
                             Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, label);
        }
        painter.setPen(QColor("#777e76"));
        painter.drawText(QRectF(64, height() - 25, width() - 96, 20), Qt::AlignLeft,
                         chapters_ ? "章节顺序 · 横向滚动查看全部章节，悬停查看完整名称与题数"
                                   : "科目分类 · 折线用于比较题量，不表示时间趋势");
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        const double step = (width() - 96.0) / qMax(1, int(counts_.size()));
        const int index = int(std::floor((event->position().x() - 64) / step));
        if (index >= 0 && index < counts_.size()) {
            const auto& count = counts_[index];
            QToolTip::showText(event->globalPosition().toPoint(),
                              QString("%1\n全部错题：%2 道\n尚未掌握：%3 道\n已掌握：%4 道")
                                  .arg(count.label).arg(count.total).arg(count.unmastered)
                                  .arg(count.total - count.unmastered), this);
        } else
            QToolTip::hideText();
    }

  private:
    QVector<AnalysisCount> counts_;
    bool chapters_ = false;
};

AnalysisPage::AnalysisPage(QWidget* parent) : QWidget(parent) {
    setObjectName("analysisPage");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* description = new QLabel("按已收录题数统计；尚未掌握 = 待复习 + 复习中。优先关注尚未掌握较多的部分。");
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* controls = new QHBoxLayout;
    dimension_ = new QComboBox;
    dimension_->setObjectName("analysisDimension");
    dimension_->addItem("按科目", "subjects");
    dimension_->addItem("按章节", "chapters");
    controls->addWidget(dimension_);
    subjectLabel_ = new QLabel("科目");
    controls->addWidget(subjectLabel_);
    subject_ = new QComboBox;
    subject_->setObjectName("analysisSubject");
    for (const auto& subject : subjects())
        subject_->addItem(subjectLabel(subject), subject);
    subject_->setCurrentIndex(subject_->findData("mathematics"));
    controls->addWidget(subject_);
    groupLabel_ = new QLabel("数学板块");
    controls->addWidget(groupLabel_);
    group_ = new QComboBox;
    group_->setObjectName("analysisMathGroup");
    group_->addItem("全部数学章节", "");
    for (const auto& group : QStringList{"高等数学", "线性代数", "概率论与数理统计"})
        group_->addItem(group, group);
    group_->addItem("其他 / 待标注", "other");
    controls->addWidget(group_);
    controls->addStretch();
    auto* enlarge = new QPushButton("放大图表");
    enlarge->setObjectName("enlargeAnalysis");
    controls->addWidget(enlarge);
    auto* refresh = new QPushButton("刷新统计");
    refresh->setObjectName("refreshAnalysis");
    controls->addWidget(refresh);
    layout->addLayout(controls);
    summary_ = new QLabel;
    summary_->setObjectName("analysisSummary");
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    auto* scroll = new QScrollArea;
    scroll->setObjectName("analysisScroll");
    scroll->setWidgetResizable(true);
    chart_ = new AnalysisChart;
    scroll->setWidget(chart_);
    layout->addWidget(scroll, 1);
    connect(enlarge, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setObjectName("analysisChartDialog");
        dialog.setWindowTitle("错题分析 · 放大图表");
        auto* layout = new QVBoxLayout(&dialog);
        auto* header = new QHBoxLayout;
        auto* summary = new QLabel(summary_->text());
        summary->setWordWrap(true);
        header->addWidget(summary, 1);
        auto* close = new QPushButton("关闭图表");
        close->setObjectName("closeAnalysisChart");
        header->addWidget(close);
        connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
        layout->addLayout(header);
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto* enlarged = new AnalysisChart;
        enlarged->copyFrom(*chart_);
        scroll->setWidget(enlarged);
        layout->addWidget(scroll, 1);
        dialog.showFullScreen();
        dialog.exec();
    });
    ranking_ = new QTableWidget(0, 4);
    ranking_->setObjectName("analysisRanking");
    ranking_->setHorizontalHeaderLabels({"复习重点（按尚未掌握题数排序）", "全部错题", "尚未掌握", "已掌握"});
    ranking_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ranking_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ranking_->setAlternatingRowColors(true);
    ranking_->verticalHeader()->hide();
    ranking_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ranking_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ranking_->setMinimumHeight(145);
    ranking_->setMaximumHeight(190);
    ranking_->setWordWrap(false);
    layout->addWidget(ranking_);
    for (auto* combo : {dimension_, subject_, group_})
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { updateView(); });
    updateView();
}
void AnalysisPage::setQuestions(const QVector<Question>& questions, const QString& selectedSubject) {
    analysis_ = analyzeQuestions(questions);
    if (selectedSubject != sidebarSubject_ && !selectedSubject.isEmpty()) {
        const QSignalBlocker blocker(subject_);
        subject_->setCurrentIndex(subject_->findData(selectedSubject));
    }
    sidebarSubject_ = selectedSubject;
    updateView();
}
void AnalysisPage::updateView() {
    const bool chapters = dimension_->currentData() == "chapters";
    const auto subject = subject_->currentData().toString();
    subject_->setVisible(chapters);
    subjectLabel_->setVisible(chapters);
    const bool math = chapters && subject == "mathematics";
    group_->setVisible(math);
    groupLabel_->setVisible(math);
    auto counts = chapters ? analysis_.chapters.value(subject) : analysis_.subjects;
    if (math && !group_->currentData().toString().isEmpty()) {
        const auto selected = group_->currentData().toString();
        counts.erase(std::remove_if(counts.begin(), counts.end(), [&](const AnalysisCount& count) {
            for (const auto& entry : mathChapters())
                if (count.key == entry.label())
                    return selected != entry.group;
            return selected != "other";
        }), counts.end());
    }
    chart_->setCounts(counts, chapters);
    auto ranked = counts;
    std::stable_sort(ranked.begin(), ranked.end(), [](const AnalysisCount& a, const AnalysisCount& b) {
        return a.unmastered != b.unmastered ? a.unmastered > b.unmastered : a.total > b.total;
    });
    ranking_->setRowCount(ranked.size());
    int total = 0, unmastered = 0;
    for (int row = 0; row < ranked.size(); ++row) {
        const auto& count = ranked[row];
        total += count.total;
        unmastered += count.unmastered;
        const QStringList cells{count.label, QString::number(count.total),
                                QString::number(count.unmastered), QString::number(count.total - count.unmastered)};
        for (int col = 0; col < cells.size(); ++col) {
            auto* item = new QTableWidgetItem(cells[col]);
            item->setToolTip(count.label + " · " + cells[col]);
            if (col == 0)
                item->setData(Qt::UserRole, count.key);
            else
                item->setTextAlignment(Qt::AlignCenter);
            ranking_->setItem(row, col, item);
        }
    }
    const auto scope = chapters ? subjectLabel(subject) : QString("全部科目");
    QString hint;
    if (unmastered > 0)
        hint = QString("优先复习：%1（尚未掌握 %2 道）").arg(ranked.first().label).arg(ranked.first().unmastered);
    else
        hint = total == 0 ? "暂无已收录错题，可先录入并标注章节。" : "当前范围内错题均已掌握，可安排巩固复习。";
    summary_->setText(QString("%1 · 全部 %2 道 · 尚未掌握 %3 道\n%4").arg(scope).arg(total).arg(unmastered).arg(hint));
}
} // namespace orange
