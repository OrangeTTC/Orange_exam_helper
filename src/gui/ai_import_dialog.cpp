// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/ai_import_dialog.hpp"
#include "orange/math_browser.hpp"
#include "orange/question_dialog.hpp"
#include "orange/reading_preferences.hpp"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTransform>
#include <QVBoxLayout>
#include <stdexcept>

namespace orange {
// 图片仅在内存中处理。拖选区域用于本地裁剪；发送前压缩为JPEG。
class PhotoPreview : public QWidget {
  public:
    explicit PhotoPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(260, 220);
        setCursor(Qt::CrossCursor);
        setToolTip("按住鼠标左键拖选区域，再点击裁剪；发送的是处理后的图片。");
    }
    void setImage(const QImage& image) { image_ = image; selection_ = {}; update(); }
    QImage cropped() const {
        const auto rect = selection_.normalized().intersected(imageRect());
        if (rect.width() < 8 || rect.height() < 8) return {};
        const auto display = imageRect();
        const double scale = double(image_.width()) / display.width();
        const QRect area(qRound((rect.x() - display.x()) * scale),
                         qRound((rect.y() - display.y()) * scale),
                         qRound(rect.width() * scale), qRound(rect.height() * scale));
        return image_.copy(area.intersected(image_.rect()));
    }
  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#eeeee5"));
        if (image_.isNull()) {
            painter.drawText(rect(), Qt::AlignCenter, "选择照片或粘贴截图\n可拖入图片文件");
            return;
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(imageRect(), image_);
        painter.setPen(QPen(QColor("#cb652f"), 2));
        painter.drawRect(selection_.normalized().intersected(imageRect()));
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && imageRect().contains(event->position().toPoint())) {
            start_ = event->position().toPoint();
            selection_ = QRect(start_, start_);
            dragging_ = true;
            update();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_) { selection_ = QRect(start_, event->position().toPoint()); update(); }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            selection_ = QRect(start_, event->position().toPoint());
            dragging_ = false;
            update();
        }
    }
  private:
    QRect imageRect() const {
        if (image_.isNull()) return {};
        const auto size = image_.size().scaled(this->size() - QSize(12, 12), Qt::KeepAspectRatio);
        return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    }
    QImage image_;
    QRect selection_;
    QPoint start_;
    bool dragging_ = false;
};

ApiSettingsDialog::ApiSettingsDialog(QWidget* parent) : QDialog(parent) {
    setObjectName("apiSettingsDialog");
    setWindowTitle("GPT API 设置");
    resize(650, 370);
    const auto current = loadAiSettings();
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel("填写自己的API Key。保存设置不会发起请求；图片分析按接口用量计费。");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* form = new QFormLayout;
    auto* endpoint = new QLineEdit(current.endpoint.toString());
    endpoint->setObjectName("apiEndpoint");
    auto* model = new QLineEdit(current.model);
    model->setObjectName("apiModel");
    auto* key = new QLineEdit(current.apiKey);
    key->setObjectName("apiKey");
    key->setEchoMode(QLineEdit::Password);
    key->setPlaceholderText("API Key（不会写入题库、日志或源码）");
    auto* reveal = new QCheckBox("显示密钥");
    connect(reveal, &QCheckBox::toggled, key, [key](bool show) {
        key->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto* remember = new QCheckBox("加密记住密钥（仅限当前Windows用户）");
    remember->setObjectName("rememberApiKey");
    remember->setEnabled(canRememberApiKey());
    remember->setChecked(current.rememberKey);
    remember->setToolTip("默认仅本次运行有效；其他平台暂不持久保存密钥。");
    auto* proxy = new QLineEdit(current.proxy);
    proxy->setObjectName("apiProxy");
    proxy->setPlaceholderText("留空使用系统代理；例如 http://127.0.0.1:12000");
    form->addRow("Responses 接口", endpoint);
    form->addRow("模型（支持图片与结构化输出）", model);
    form->addRow("API Key", key);
    form->addRow("", reveal);
    form->addRow("", remember);
    form->addRow("HTTP 代理", proxy);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    auto* forget = buttons->addButton("清除密钥", QDialogButtonBox::ActionRole);
    forget->setObjectName("clearApiKey");
    connect(forget, &QPushButton::clicked, this, [this, key, remember] {
        try {
            clearApiKey(); key->clear(); remember->setChecked(false);
        } catch (const std::exception& e) {
            QMessageBox::warning(this, "无法清除密钥", QString::fromUtf8(e.what()));
        }
    });
    buttons->button(QDialogButtonBox::Save)->setText("保存设置");
    buttons->button(QDialogButtonBox::Save)->setObjectName("saveApiSettings");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=] {
        try {
            AiSettings settings;
            settings.endpoint = QUrl(endpoint->text().trimmed());
            settings.model = model->text().trimmed();
            settings.apiKey = key->text().trimmed();
            settings.proxy = proxy->text().trimmed();
            settings.rememberKey = remember->isChecked();
            saveAiSettings(settings);
            accept();
        } catch (const std::exception& e) {
            QMessageBox::warning(this, "设置未保存", QString::fromUtf8(e.what()));
        }
    });
}
AiImportDialog::AiImportDialog(Repository& repository, QWidget* parent)
    : QDialog(parent), repository_(repository), client_(new AiClient(this)) {
    setObjectName("aiImportDialog");
    setWindowTitle("GPT 图片录入 · 预览确认后加入错题本");
    resize(1200, 860);
    setAcceptDrops(true);
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* hint = new QLabel("点击“发送图片并分析”才联网，发送所选照片、科目、来源、说明与章节候选标签；其他题库不上传。");
    hint->setWordWrap(true);
    header->addWidget(hint, 1);
    settings_ = new QPushButton("API 设置");
    settings_->setObjectName("apiSettings");
    header->addWidget(settings_);
    layout->addLayout(header);
    connect(settings_, &QPushButton::clicked, this, [this] { ApiSettingsDialog(this).exec(); });
    auto* split = new QSplitter;
    inputPanel_ = new QWidget;
    auto* inputs = new QVBoxLayout(inputPanel_);
    inputs->setContentsMargins(0, 0, 8, 0);
    auto* photoButtons = new QHBoxLayout;
    auto* open = new QPushButton("选择照片");
    open->setObjectName("chooseAiImages");
    auto* paste = new QPushButton("粘贴截图");
    paste->setObjectName("pasteAiImage");
    auto* remove = new QPushButton("移除");
    photoButtons->addWidget(open); photoButtons->addWidget(paste); photoButtons->addWidget(remove);
    inputs->addLayout(photoButtons);
    photosList_ = new QListWidget;
    photosList_->setObjectName("aiPhotos");
    photosList_->setMaximumHeight(110);
    inputs->addWidget(photosList_);
    photo_ = new PhotoPreview;
    photo_->setObjectName("aiPhotoPreview");
    inputs->addWidget(photo_, 1);
    auto* transform = new QHBoxLayout;
    auto* rotate = new QPushButton("旋转90°");
    rotate->setObjectName("rotateAiPhoto");
    auto* crop = new QPushButton("裁剪选区");
    crop->setObjectName("cropAiPhoto");
    transform->addWidget(rotate); transform->addWidget(crop);
    inputs->addLayout(transform);
    auto* form = new QFormLayout;
    subject_ = new QComboBox;
    subject_->setObjectName("aiSubject");
    for (const auto& subject : subjects()) subject_->addItem(subjectLabel(subject), subject);
    source_ = new QLineEdit;
    source_->setObjectName("aiSource");
    source_->setPlaceholderText("例如：2026预测卷（2）；未知时填待标注");
    numbers_ = new QLineEdit;
    numbers_->setObjectName("aiNumbers");
    numbers_->setPlaceholderText("例如：5、6；空值识别所有可见题目");
    form->addRow("科目", subject_); form->addRow("来源", source_); form->addRow("题号", numbers_);
    inputs->addLayout(form);
    mistake_ = new QTextEdit;
    mistake_->setObjectName("aiMistake");
    mistake_->setAcceptRichText(false);
    mistake_->setPlaceholderText("错因（可选）：例如不熟悉最大似然估计与矩估计");
    mistake_->setMaximumHeight(65);
    instructions_ = new QTextEdit;
    instructions_->setObjectName("aiInstructions");
    instructions_->setAcceptRichText(false);
    instructions_->setPlaceholderText("补充要求：例如解析中说明两种估计的通用方法");
    instructions_->setMaximumHeight(65);
    inputs->addWidget(mistake_); inputs->addWidget(instructions_);
    split->addWidget(inputPanel_);
    auto* resultPanel = new QWidget;
    auto* results = new QVBoxLayout(resultPanel);
    results->setContentsMargins(8, 0, 0, 0);
    results->addWidget(new QLabel("分析结果 · 对照照片核对，勾选要加入的错题"));
    draftList_ = new QListWidget;
    draftList_->setObjectName("aiDrafts");
    draftList_->setMaximumHeight(130);
    results->addWidget(draftList_);
    results->addWidget(createReadingToolbar());
    preview_ = new MathBrowser;
    preview_->setObjectName("aiDraftPreview");
    results->addWidget(preview_, 1);
    edit_ = new QPushButton("编辑当前草稿");
    edit_->setObjectName("editAiDraft");
    results->addWidget(edit_);
    split->addWidget(resultPanel);
    split->setStretchFactor(0, 2); split->setStretchFactor(1, 3);
    layout->addWidget(split, 1);
    status_ = new QLabel("未发送。每次最多5张照片；拖选图片区域可裁剪。");
    status_->setObjectName("aiStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto* actions = new QHBoxLayout;
    send_ = new QPushButton("发送图片并分析");
    send_->setObjectName("sendAiImages");
    cancel_ = new QPushButton("取消请求");
    cancel_->setObjectName("cancelAiRequest");
    save_ = new QPushButton("将勾选题目加入错题本");
    save_->setObjectName("saveAiDrafts");
    auto* close = new QPushButton("关闭");
    actions->addWidget(send_); actions->addWidget(cancel_); actions->addStretch();
    actions->addWidget(save_); actions->addWidget(close);
    layout->addLayout(actions);
    for (auto* button : findChildren<QPushButton*>()) button->setAutoDefault(false);
    connect(open, &QPushButton::clicked, this, &AiImportDialog::openImages);
    connect(paste, &QPushButton::clicked, this, [this] {
        addImage(QApplication::clipboard()->image(), "粘贴截图");
    });
    connect(photosList_, &QListWidget::currentRowChanged, this, [this](int row) {
        photo_->setImage(row >= 0 && row < photos_.size() ? photos_.at(row) : QImage());
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = photosList_->currentRow();
        if (row < 0) return;
        photos_.removeAt(row);
        delete photosList_->takeItem(row);
        photo_->setImage(photosList_->currentRow() >= 0 ? photos_.at(photosList_->currentRow()) : QImage());
    });
    connect(rotate, &QPushButton::clicked, this, [this] {
        const int row = photosList_->currentRow();
        if (row < 0) return;
        photos_[row] = photos_[row].transformed(QTransform().rotate(90));
        photo_->setImage(photos_[row]);
    });
    connect(crop, &QPushButton::clicked, this, [this] {
        const auto cropped = photo_->cropped();
        if (cropped.isNull()) { status_->setText("请先在图片上按住左键拖选要保留的区域。"); return; }
        photos_[photosList_->currentRow()] = cropped;
        photo_->setImage(cropped);
    });
    connect(send_, &QPushButton::clicked, this, &AiImportDialog::startAnalysis);
    connect(cancel_, &QPushButton::clicked, client_, &AiClient::cancel);
    connect(close, &QPushButton::clicked, this, &AiImportDialog::reject);
    connect(draftList_, &QListWidget::currentRowChanged, this, &AiImportDialog::updateDraft);
    connect(draftList_, &QListWidget::itemChanged, this, &AiImportDialog::updateDraft);
    connect(edit_, &QPushButton::clicked, this, [this] {
        const int row = draftList_->currentRow();
        if (row < 0) return;
        QuestionDialog editor(drafts_.at(row), this);
        editor.setWindowTitle("编辑AI草稿 · 尚未加入错题本");
        editor.findChild<QPushButton*>("saveQuestion")->setText("应用到草稿");
        if (editor.exec() == QDialog::Accepted) {
            drafts_[row] = editor.question();
            draftList_->item(row)->setText(drafts_[row].title);
            updateDraft();
        }
    });
    connect(save_, &QPushButton::clicked, this, &AiImportDialog::saveSelected);
    connect(client_, &AiClient::failed, this, [this](const QString& message) {
        setBusy(false); status_->setText(message);
    });
    connect(client_, &AiClient::succeeded, this, [this](const QVector<Question>& questions) {
        drafts_ = questions;
        draftList_->clear();
        for (const auto& question : drafts_) {
            auto* item = new QListWidgetItem(question.title, draftList_);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
        if (!drafts_.isEmpty()) draftList_->setCurrentRow(0);
        setBusy(false);
        status_->setText(drafts_.isEmpty() ? "没有识别到题目，请核对图片与题号后重试。"
            : QString("生成%1道草稿，尚未写入题库。核对题干与解析后，勾选需要收录的题目。").arg(drafts_.size()));
    });
    setBusy(false);
}
void AiImportDialog::addImage(const QImage& image, const QString& label) {
    if (client_->busy()) return;
    if (image.isNull()) { status_->setText("无法读取图片，请选择有效图片或复制截图。"); return; }
    if (photos_.size() >= 5) { status_->setText("每次最多5张照片，请移除部分图片或分批录入。"); return; }
    photos_.append(image.width() > 2400 || image.height() > 2400
        ? image.scaled(QSize(2400, 2400), Qt::KeepAspectRatio, Qt::SmoothTransformation) : image);
    photosList_->addItem(label);
    photosList_->setCurrentRow(photos_.size() - 1);
    status_->setText(QString("已选择%1张照片，尚未发送。可旋转或拖选裁剪。").arg(photos_.size()));
}
void AiImportDialog::loadImage(const QString& path) {
    const QFileInfo file(path);
    if (file.size() > 20 * 1024 * 1024) { status_->setText("单张原图不能超过20MB。"); return; }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 50000000) {
        status_->setText("图片无效或像素过多，请缩小后重试。"); return;
    }
    if (size.width() > 2400 || size.height() > 2400)
        reader.setScaledSize(size.scaled(QSize(2400, 2400), Qt::KeepAspectRatio));
    addImage(reader.read(), file.fileName());
}
void AiImportDialog::openImages() {
    const auto paths = QFileDialog::getOpenFileNames(this, "选择题目照片", {},
        "图片 (*.jpg *.jpeg *.png *.webp *.bmp)");
    for (const auto& path : paths) loadImage(path);
}
void AiImportDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (client_->busy()) return;
    if (event->mimeData()->hasImage()) event->acceptProposedAction();
    else for (const auto& url : event->mimeData()->urls())
        if (url.isLocalFile()) { event->acceptProposedAction(); break; }
}
void AiImportDialog::dropEvent(QDropEvent* event) {
    if (client_->busy()) return;
    if (event->mimeData()->hasImage()) addImage(qvariant_cast<QImage>(event->mimeData()->imageData()), "拖入图片");
    else for (const auto& url : event->mimeData()->urls()) if (url.isLocalFile()) loadImage(url.toLocalFile());
    event->acceptProposedAction();
}
void AiImportDialog::startAnalysis() {
    try {
        const auto settings = loadAiSettings();
        AiInput input;
        input.subject = subject_->currentData().toString();
        input.source = source_->text().trimmed().isEmpty() ? "待标注" : source_->text().trimmed();
        input.numbers = numbers_->text().trimmed();
        input.mistake = mistake_->toPlainText();
        input.instructions = instructions_->toPlainText();
        input.chapters = repository_.tags("chapter", input.subject);
        for (const auto& photo : photos_) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            // 透明截图先铺白色背景，避免JPEG转换后出现黑底。
            QImage flattened(photo.size(), QImage::Format_RGB32);
            flattened.fill(Qt::white);
            QPainter painter(&flattened); painter.drawImage(0, 0, photo); painter.end();
            if (!flattened.save(&buffer, "JPEG", 92)) throw std::runtime_error("无法编码图片。");
            input.images.append(bytes);
        }
        buildAiRequest(settings, input); // 先校验；设置错误时保留已有草稿。
        drafts_.clear(); draftList_->clear(); preview_->showText("");
        setBusy(true);
        status_->setText("正在识别和分析，可取消请求；尚未写入题库。请耐心等待。");
        client_->start(settings, input);
    } catch (const std::exception& e) {
        setBusy(false); status_->setText(QString::fromUtf8(e.what()));
    }
}
void AiImportDialog::setBusy(bool busy) {
    inputPanel_->setEnabled(!busy); settings_->setEnabled(!busy);
    send_->setEnabled(!busy); cancel_->setEnabled(busy);
    draftList_->setEnabled(!busy);
    updateDraft();
}
void AiImportDialog::updateDraft() {
    const int row = draftList_->currentRow();
    edit_->setEnabled(!client_->busy() && row >= 0 && row < drafts_.size());
    if (row >= 0 && row < drafts_.size()) preview_->showQuestion(drafts_.at(row));
    else preview_->showText("分析完成后，在这里核对题干、LaTeX解析、章节和错因。");
    bool checked = false;
    for (int i = 0; i < draftList_->count(); ++i)
        checked |= draftList_->item(i)->checkState() == Qt::Checked;
    save_->setEnabled(!client_->busy() && checked);
}
void AiImportDialog::saveSelected() {
    if (client_->busy()) return;
    try {
        QJsonArray rows;
        QVector<int> selected;
        for (int i = 0; i < drafts_.size(); ++i) {
            if (draftList_->item(i)->checkState() != Qt::Checked) continue;
            validate(drafts_.at(i));
            rows.append(toJson(drafts_.at(i))); selected.append(i);
        }
        if (rows.isEmpty()) return;
        // 使用已有事务导入：一题失败全部回滚；保留草稿ID防止重复点击写入。
        const int count = repository_.importJson({{"schema_version", 1}, {"questions", rows}});
        for (auto it = selected.crbegin(); it != selected.crend(); ++it) {
            drafts_.removeAt(*it); delete draftList_->takeItem(*it);
        }
        updateDraft();
        status_->setText(QString("已加入%1道错题；题库、章节索引及统计已同步刷新。").arg(count));
        emit imported();
    } catch (const std::exception& e) { status_->setText(QString::fromUtf8(e.what())); }
}
void AiImportDialog::reject() {
    client_->cancel();
    QDialog::reject();
}
} // namespace orange
