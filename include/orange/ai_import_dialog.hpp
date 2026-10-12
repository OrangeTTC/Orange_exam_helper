// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/ai_client.hpp"
#include "orange/repository.hpp"
#include <QDialog>
#include <QImage>
class QComboBox;
class QLineEdit;
class QTextEdit;
class QListWidget;
class QLabel;
class QPushButton;
namespace orange {
class MathBrowser;
class PhotoPreview;
class ApiSettingsDialog : public QDialog {
  public:
    explicit ApiSettingsDialog(QWidget* parent = nullptr);
};
class AiImportDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AiImportDialog(Repository& repository, QWidget* parent = nullptr);
    void addImage(const QImage& image, const QString& label);
  signals:
    void imported();
  protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void reject() override;
  private:
    void openImages();
    void loadImage(const QString& path);
    void startAnalysis();
    void updateDraft();
    void saveSelected();
    void setBusy(bool busy);
    Repository& repository_;
    AiClient* client_;
    QVector<QImage> photos_;
    QVector<Question> drafts_;
    QWidget* inputPanel_;
    QComboBox* subject_;
    QLineEdit *source_, *numbers_;
    QTextEdit *mistake_, *instructions_;
    QListWidget *photosList_, *draftList_;
    PhotoPreview* photo_;
    MathBrowser* preview_;
    QLabel* status_;
    QPushButton *send_, *cancel_, *settings_, *edit_, *save_;
};
} // namespace orange
