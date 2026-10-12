// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/ai_client.hpp"
#include "orange/ai_import_dialog.hpp"
#include "orange/chapter_catalog.hpp"
#include "orange/main_window.hpp"
#include "orange/math_browser.hpp"
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest>

using namespace orange;
namespace {
QJsonObject draft() {
    return {{"title", "第5题 · 方程"}, {"content", "求 $x^2=4$ 的解。"},
        {"answer", "$$x=\\pm2$$"}, {"notes", "建议难度2/5：基础方程。"},
        {"difficulty", 2}, {"chapter", mathChapters().first().label()}, {"uncertainty", ""},
        {"source", "伪造来源"}, {"subject", "english"}};
}
QByteArray response(const QJsonObject& question = draft()) {
    const auto text = QString::fromUtf8(QJsonDocument(QJsonObject{{"questions", QJsonArray{question}}})
                                        .toJson(QJsonDocument::Compact));
    return QJsonDocument(QJsonObject{{"status", "completed"}, {"output", QJsonArray{
        QJsonObject{{"type", "message"}, {"content", QJsonArray{
            QJsonObject{{"type", "output_text"}, {"text", text}}}}}}}}).toJson();
}
AiInput input() {
    AiInput input;
    input.subject = "mathematics";
    input.source = "测试来源";
    input.numbers = "5";
    input.mistake = "用户明确提供的错因";
    input.instructions = "说明通用方法";
    input.images = {QByteArray("local-test-image")};
    return input;
}
// 仅监听本机回环地址，以假密钥验证HTTP协议、错误及异步GUI；不调用外部服务。
class LocalApi : public QTcpServer {
  public:
    QByteArray body = response();
    int code = 200;
    bool delay = false;
    int requests = 0;
    QByteArray lastRequest;
    explicit LocalApi(QObject* parent = nullptr) : QTcpServer(parent) {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto* socket = nextPendingConnection();
                socket->setParent(this);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto bytes = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", bytes);
                    const int end = bytes.indexOf("\r\n\r\n");
                    if (end < 0 || socket->property("handled").toBool()) return;
                    qint64 length = -1;
                    for (const auto& header : bytes.left(end).split('\n'))
                        if (header.toLower().startsWith("content-length:"))
                            length = header.mid(header.indexOf(':') + 1).trimmed().toLongLong();
                    if (length < 0 || bytes.size() < end + 4 + length) return;
                    socket->setProperty("handled", true);
                    ++requests;
                    lastRequest = bytes;
                    if (delay) return;
                    QByteArray headers = "HTTP/1.1 " + QByteArray::number(code) + " Test\r\n";
                    if (code == 302) headers += "Location: /should-not-follow\r\n";
                    headers += "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) +
                               "\r\nConnection: close\r\n\r\n";
                    socket->write(headers + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    AiSettings settings() const {
        AiSettings settings;
        settings.endpoint = QUrl(QString("http://127.0.0.1:%1/v1/responses").arg(serverPort()));
        settings.apiKey = "fake-key-never-use-external";
        // 本机HTTP代理直接转发给本机测试服务，避免依赖真实系统代理。
        settings.proxy = QString("http://127.0.0.1:%1").arg(serverPort());
        return settings;
    }
};
}
class AiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory_;
  private slots:
    void initTestCase() {
        QVERIFY(settingsDirectory_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
        QCoreApplication::setOrganizationName("OrangeAiTests");
        QCoreApplication::setApplicationName("OrangeAiTests");
        qRegisterMetaType<QVector<Question>>();
    }
    void requestUsesOnlySelectedContext() {
        AiSettings settings;
        settings.apiKey = "fake-key-not-in-json";
        const auto json = buildAiRequest(settings, input());
        QCOMPARE(json.value("store").toBool(), false);
        QVERIFY(!QJsonDocument(json).toJson().contains(settings.apiKey.toUtf8()));
        const auto contents = json.value("input").toArray().first().toObject().value("content").toArray();
        QCOMPARE(contents.size(), 2);
        QCOMPARE(contents[1].toObject().value("type").toString(), QString("input_image"));
        QCOMPARE(contents[1].toObject().value("detail").toString(), QString("high"));
        const auto schema = json.value("text").toObject().value("format").toObject().value("schema").toObject();
        const auto properties = schema.value("properties").toObject().value("questions").toObject()
            .value("items").toObject().value("properties").toObject();
        QCOMPARE(properties.value("chapter").toObject().value("enum").toArray().size(), mathChapters().size() + 1);
        settings.endpoint = QUrl("http://remote.example/v1/responses");
        QVERIFY_EXCEPTION_THROWN(buildAiRequest(settings, input()), std::runtime_error);
        settings.endpoint = QUrl("https://remote.example/v1/responses?secret=key");
        QVERIFY_EXCEPTION_THROWN(buildAiRequest(settings, input()), std::runtime_error);
    }
    void parsingAndUserAuthority() {
        auto question = draft();
        question["uncertainty"] = "照片中的负号需核对";
        const auto rows = parseAiResponse(response(question), input(), "test-model");
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().source, input().source);
        QCOMPARE(rows.first().subject, input().subject);
        QVERIFY(rows.first().notes.contains(input().mistake));
        QVERIFY(rows.first().notes.contains("照片中的负号需核对"));
        QVERIFY(rows.first().conversation.contains("test-model"));
        question["chapter"] = "编造章节";
        QVERIFY_EXCEPTION_THROWN(parseAiResponse(response(question), input(), "test"), std::runtime_error);
        question = draft(); question["difficulty"] = 2.5;
        QVERIFY_EXCEPTION_THROWN(parseAiResponse(response(question), input(), "test"), std::runtime_error);
        question = draft(); question.remove("content");
        QVERIFY_EXCEPTION_THROWN(parseAiResponse(response(question), input(), "test"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseAiResponse("{broken", input(), "test"), std::runtime_error);
        auto incomplete = QJsonDocument::fromJson(response()).object(); incomplete["status"] = "incomplete";
        QVERIFY_EXCEPTION_THROWN(parseAiResponse(QJsonDocument(incomplete).toJson(), input(), "test"), std::runtime_error);
        const auto refusal = QJsonDocument(QJsonObject{{"status", "completed"}, {"output", QJsonArray{
            QJsonObject{{"type", "message"}, {"content", QJsonArray{QJsonObject{{"type", "refusal"}}}}}}}}).toJson();
        QVERIFY_EXCEPTION_THROWN(parseAiResponse(refusal, input(), "test"), std::runtime_error);
    }
    void encryptedAndSessionSettings() {
        try {
        AiSettings settings;
        settings.apiKey = "fake-key-settings-roundtrip";
        saveAiSettings(settings);
        QCOMPARE(loadAiSettings().apiKey, settings.apiKey);
        QVERIFY(QSettings().value("ai/protectedKey").toByteArray().isEmpty());
        if (canRememberApiKey()) {
            settings.rememberKey = true;
            saveAiSettings(settings);
            QCOMPARE(loadAiSettings().apiKey, settings.apiKey);
            QVERIFY(loadAiSettings().rememberKey);
            QVERIFY(!QSettings().value("ai/protectedKey").toByteArray().contains(settings.apiKey.toUtf8()));
            QFile file(QSettings().fileName());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(!file.readAll().contains(settings.apiKey.toUtf8()));
            file.close(); // Windows下先关闭读取句柄，允许QSettings原子替换设置文件。
            settings.rememberKey = false;
            saveAiSettings(settings);
            QVERIFY(QSettings().value("ai/protectedKey").toByteArray().isEmpty());
        }
        ApiSettingsDialog dialog;
        QCOMPARE(dialog.findChild<QLineEdit*>("apiKey")->echoMode(), QLineEdit::Password);
        QTest::mouseClick(dialog.findChild<QPushButton*>("clearApiKey"), Qt::LeftButton);
        QVERIFY(loadAiSettings().apiKey.isEmpty());
        } catch (const std::exception& e) { QFAIL(e.what()); }
    }
    void transportErrorsCancellationAndRedirects() {
        LocalApi server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        AiClient client;
        QSignalSpy ok(&client, &AiClient::succeeded);
        QSignalSpy error(&client, &AiClient::failed);
        client.start(server.settings(), input());
        QTRY_COMPARE(ok.size(), 1);
        QCOMPARE(error.size(), 0);
        QVERIFY(!client.busy());
        QVERIFY(server.lastRequest.toLower().contains("authorization: bearer fake-key-never-use-external"));
        const auto payload = QJsonDocument::fromJson(server.lastRequest.mid(server.lastRequest.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(payload.value("store").toBool(), false);
        server.code = 401;
        server.body = "service-error-might-contain-secret";
        client.start(server.settings(), input());
        QTRY_COMPARE(error.size(), 1);
        QVERIFY(error.last().first().toString().contains("401"));
        QVERIFY(!error.last().first().toString().contains("secret"));
        server.code = 302;
        const int before = server.requests;
        client.start(server.settings(), input());
        QTRY_COMPARE(error.size(), 2);
        QTest::qWait(40);
        QCOMPARE(server.requests, before + 1); // 禁止跨地址自动重定向和转发密钥。
        server.code = 200;
        server.delay = true;
        client.start(server.settings(), input());
        QTRY_COMPARE(server.requests, before + 2);
        client.cancel();
        QTRY_COMPARE(error.size(), 3);
        QCOMPARE(ok.size(), 1);
        QVERIFY(error.last().first().toString().contains("取消"));
    }
    void previewEditAndExplicitImport() {
        LocalApi server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        saveAiSettings(server.settings());
        QTemporaryDir directory;
        Repository repo(directory.path());
        MainWindow window(repo);
        window.show();
        AiImportDialog dialog(repo, &window);
        dialog.show();
        QImage image(20, 30, QImage::Format_RGB32); image.fill(Qt::white);
        dialog.addImage(image, "本地测试图片");
        QTest::mouseClick(dialog.findChild<QPushButton*>("rotateAiPhoto"), Qt::LeftButton);
        auto* subject = dialog.findChild<QComboBox*>("aiSubject");
        subject->setCurrentIndex(subject->findData("mathematics"));
        dialog.findChild<QLineEdit*>("aiSource")->setText("界面指定来源");
        dialog.findChild<QTextEdit*>("aiMistake")->setPlainText("界面指定错因");
        QTest::qWait(30);
        QCOMPARE(server.requests, 0); // 打开界面、选择图片、保存设置均不能自动发送。
        QCOMPARE(repo.list().size(), 0);
        QTest::mouseClick(dialog.findChild<QPushButton*>("sendAiImages"), Qt::LeftButton);
        auto* drafts = dialog.findChild<QListWidget*>("aiDrafts");
        QTRY_COMPARE(drafts->count(), 1);
        QCOMPARE(repo.list().size(), 0);
        QCOMPARE(drafts->item(0)->checkState(), Qt::Unchecked);
        auto* save = dialog.findChild<QPushButton*>("saveAiDrafts");
        QVERIFY(!save->isEnabled());
        const auto payload = QJsonDocument::fromJson(server.lastRequest.mid(server.lastRequest.indexOf("\r\n\r\n") + 4)).object();
        const auto url = payload.value("input").toArray().first().toObject().value("content").toArray()[1]
            .toObject().value("image_url").toString();
        QCOMPARE(QImage::fromData(QByteArray::fromBase64(url.section(',', 1).toLatin1())).size(), QSize(30, 20));
        auto* preview = dynamic_cast<MathBrowser*>(dialog.findChild<QTextBrowser*>("aiDraftPreview"));
        QVERIFY(preview);
        QCOMPARE(preview->errorCount(), 0);
        QVERIFY(preview->toPlainText().contains("界面指定错因"));
        const auto screenshots = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(dialog.grab().save(screenshots + "/ai-preview.png"));
        QTimer::singleShot(30, &dialog, [&dialog] {
            auto* editor = dialog.findChild<QDialog*>("questionDialog");
            QVERIFY(editor);
            editor->findChild<QLineEdit*>("editTitle")->setText("人工核对后的标题");
            QTest::mouseClick(editor->findChild<QPushButton*>("saveQuestion"), Qt::LeftButton);
        });
        QTest::mouseClick(dialog.findChild<QPushButton*>("editAiDraft"), Qt::LeftButton);
        QCOMPARE(repo.list().size(), 0); // 应用到草稿不等于保存错题。
        drafts->item(0)->setCheckState(Qt::Checked);
        QVERIFY(save->isEnabled());
        QSignalSpy imported(&dialog, &AiImportDialog::imported);
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(imported.size(), 1);
        QCOMPARE(repo.list().size(), 1);
        QCOMPARE(repo.list().first().title, QString("人工核对后的标题"));
        QCOMPARE(repo.list().first().source, QString("界面指定来源"));
        QCOMPARE(repo.history(repo.list().first().id).size(), 1);
        QCOMPARE(drafts->count(), 0);
        QVERIFY(!save->isEnabled());
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(repo.list().size(), 1);
        if (!screenshots.isEmpty()) QVERIFY(dialog.grab().save(screenshots + "/ai-import.png"));
    }
    void croppingUploadsOnlyProcessedImage() {
        LocalApi server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        saveAiSettings(server.settings());
        QTemporaryDir directory;
        Repository repo(directory.path());
        AiImportDialog dialog(repo);
        dialog.show();
        QImage image(640, 480, QImage::Format_RGB32); image.fill(Qt::white);
        dialog.addImage(image, "裁剪测试");
        auto* photo = dialog.findChild<QWidget*>("aiPhotoPreview");
        QVERIFY(photo);
        QTest::qWait(20);
        const auto center = photo->rect().center();
        QTest::mousePress(photo, Qt::LeftButton, Qt::NoModifier, center - QPoint(30, 25));
        QTest::mouseMove(photo, center + QPoint(30, 25));
        QTest::mouseRelease(photo, Qt::LeftButton, Qt::NoModifier, center + QPoint(30, 25));
        QTest::mouseClick(dialog.findChild<QPushButton*>("cropAiPhoto"), Qt::LeftButton);
        auto* subject = dialog.findChild<QComboBox*>("aiSubject");
        subject->setCurrentIndex(subject->findData("mathematics"));
        QTest::mouseClick(dialog.findChild<QPushButton*>("sendAiImages"), Qt::LeftButton);
        QTRY_COMPARE(dialog.findChild<QListWidget*>("aiDrafts")->count(), 1);
        const auto payload = QJsonDocument::fromJson(server.lastRequest.mid(server.lastRequest.indexOf("\r\n\r\n") + 4)).object();
        const auto url = payload.value("input").toArray().first().toObject().value("content").toArray()[1]
            .toObject().value("image_url").toString();
        const auto cropped = QImage::fromData(QByteArray::fromBase64(url.section(',', 1).toLatin1()));
        QVERIFY(!cropped.isNull());
        QVERIFY(cropped.width() < image.width() && cropped.height() < image.height());
        dialog.close();
        QCOMPARE(repo.list().size(), 0); // 关闭未勾选、未保存的草稿不收录。
    }
};
QTEST_MAIN(AiTests)
#include "ai_tests.moc"
