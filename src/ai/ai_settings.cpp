// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/ai_client.hpp"
#include <QSettings>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace orange {
namespace {
QString sessionKey; // 不选择记住时，密钥仅保留在本进程内。
QByteArray protect(const QByteArray& input, bool encrypt) {
#ifdef Q_OS_WIN
    DATA_BLOB source{static_cast<DWORD>(input.size()),
                     reinterpret_cast<BYTE*>(const_cast<char*>(input.constData()))};
    DATA_BLOB output{};
    const bool ok = encrypt
        ? CryptProtectData(&source, L"OrangeStudy API Key", nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &output)
        : CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr,
                             CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok)
        throw std::runtime_error("无法加密或读取密钥，请重新填写；可以取消记住密钥。");
    QByteArray result(reinterpret_cast<char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(input);
    Q_UNUSED(encrypt);
    throw std::runtime_error("本平台暂不提供持久化密钥，请在本次运行中设置。");
#endif
}
}
bool canRememberApiKey() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
AiSettings loadAiSettings() {
    QSettings saved;
    AiSettings result;
    result.endpoint = QUrl(saved.value("ai/endpoint", result.endpoint.toString()).toString());
    result.model = saved.value("ai/model", result.model).toString();
    result.proxy = saved.value("ai/proxy").toString();
    result.apiKey = sessionKey;
    const auto encrypted = saved.value("ai/protectedKey").toByteArray();
    if (canRememberApiKey() && !encrypted.isEmpty()) {
        try {
            result.apiKey = QString::fromUtf8(protect(encrypted, false));
            result.rememberKey = true;
        } catch (const std::exception&) {
            // 不打印密钥或密文；无法解密时允许用户重新填写。
        }
    }
    return result;
}
void saveAiSettings(const AiSettings& settings) {
    validateAiSettings(settings);
    // 先加密成功再改写设置，防止失败时丢失现有凭据。
    const auto encrypted = settings.rememberKey ? protect(settings.apiKey.toUtf8(), true) : QByteArray();
    QSettings saved;
    saved.setValue("ai/endpoint", settings.endpoint.toString());
    saved.setValue("ai/model", settings.model);
    saved.setValue("ai/proxy", settings.proxy);
    if (settings.rememberKey)
        saved.setValue("ai/protectedKey", encrypted);
    else
        saved.remove("ai/protectedKey");
    saved.sync();
    if (saved.status() != QSettings::NoError)
        throw std::runtime_error("无法保存接口设置。");
    sessionKey = settings.apiKey;
}
void clearApiKey() {
    QSettings saved;
    saved.remove("ai/protectedKey");
    saved.sync();
    if (saved.status() != QSettings::NoError)
        throw std::runtime_error("无法清除已保存的密钥。");
    sessionKey.clear();
}
void validateAiSettings(const AiSettings& settings) {
    const auto url = settings.endpoint;
    const bool local = url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != "https" && !(local && url.scheme() == "http")) ||
        !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
        !url.path().endsWith("/responses"))
        throw std::runtime_error("请填写完整 HTTPS Responses 接口地址，例如 https://api.openai.com/v1/responses。");
    if (settings.model.trimmed().isEmpty() || settings.model.size() > 160)
        throw std::runtime_error("请填写支持图片及结构化输出的模型名称。");
    if (settings.apiKey.trimmed().isEmpty() || settings.apiKey.contains('\n') ||
        settings.apiKey.contains('\r') || settings.apiKey.toUtf8().size() > 4096)
        throw std::runtime_error("请在 API 设置中填写有效的 API Key。");
    if (!settings.proxy.isEmpty()) {
        const QUrl proxy(settings.proxy);
        if (!proxy.isValid() || proxy.scheme() != "http" || proxy.host().isEmpty() ||
            proxy.port() <= 0 || proxy.port() > 65535 || !proxy.userInfo().isEmpty() ||
            proxy.hasQuery() || proxy.hasFragment() ||
            (!proxy.path().isEmpty() && proxy.path() != "/"))
            throw std::runtime_error("代理请填写 http://主机:端口；留空使用系统代理。");
    }
}
} // namespace orange
