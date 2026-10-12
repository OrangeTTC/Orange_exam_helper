// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "orange/model.hpp"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

namespace orange {
struct AiSettings {
    QUrl endpoint = QUrl("https://api.openai.com/v1/responses");
    QString model = "gpt-4.1";
    QString apiKey;
    QString proxy; // 空值使用系统代理；仅支持不带凭据的 HTTP 代理地址。
    bool rememberKey = false;
};
struct AiInput {
    QString subject = "professional";
    QString source = "待标注";
    QString numbers;
    QString mistake;
    QString instructions;
    QStringList chapters; // 非数学科目仅使用本地已有章节标签，不猜测大纲。
    QList<QByteArray> images; // 本次用户选择并处理的 JPEG，不含本地路径或其他题库。
};
AiSettings loadAiSettings();
void saveAiSettings(const AiSettings& settings);
void clearApiKey();
bool canRememberApiKey();
void validateAiSettings(const AiSettings& settings);
QJsonObject buildAiRequest(const AiSettings& settings, const AiInput& input);
QVector<Question> parseAiResponse(const QByteArray& bytes, const AiInput& input,
                                 const QString& model);

// 独立的异步联网模块：不持有 Repository，不可自行写入题库。
class AiClient : public QObject {
    Q_OBJECT
  public:
    explicit AiClient(QObject* parent = nullptr);
    void start(const AiSettings& settings, const AiInput& input);
    void cancel();
    bool busy() const { return !reply_.isNull(); }
  signals:
    void succeeded(const QVector<orange::Question>& questions);
    void failed(const QString& message);
  private:
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    bool cancelled_ = false;
};
} // namespace orange
Q_DECLARE_METATYPE(QVector<orange::Question>)
