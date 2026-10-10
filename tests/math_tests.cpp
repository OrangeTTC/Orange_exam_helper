// Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
#include "orange/math_browser.hpp"
#include "orange/reading_preferences.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QToolButton>
#include <QtTest>
#include <memory>

class MathTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory_;
  private slots:
    void initTestCase() {
        QVERIFY(settingsDirectory_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
        QCoreApplication::setOrganizationName("OrangeTests");
    }
    void cleanup() {
        orange::ReadingPreferences::instance().setFontSize(15);
    }
    void fontSizeSyncAndPersistence() {
        orange::MathBrowser first, second;
        first.resize(1000, 700);
        second.resize(1000, 700);
        const QString source = "阅读正文 $\\dfrac{x+1}{x^2}$";
        first.showText(source);
        second.showText(source);
        const auto imageBefore = first.document()
                                     ->resource(QTextDocument::ImageResource, QUrl("orange-math:0"))
                                     .value<QImage>();
        auto& preferences = orange::ReadingPreferences::instance();
        preferences.setFontSize(30);
        QCOMPARE(first.document()->defaultFont().pixelSize(), 30);
        QCOMPARE(second.document()->defaultFont().pixelSize(), 30);
        const auto imageAfter = first.document()
                                    ->resource(QTextDocument::ImageResource, QUrl("orange-math:0"))
                                    .value<QImage>();
        QVERIFY(imageAfter.width() > imageBefore.width() * 1.5);
        QVERIFY(imageAfter.height() > imageBefore.height() * 1.5);
        QCOMPARE(first.sourceText(), source);
        QCOMPARE(first.errorCount(), 0);
        // 重新读取本地设置，验证新窗口/下次启动能恢复，而非仅有内存值。
        orange::ReadingPreferences restored;
        QCOMPARE(restored.fontSize(), 30);
        QSettings settings;
        QVERIFY(settings.fileName().startsWith(settingsDirectory_.path()));
        QCOMPARE(settings.value("reading/fontSizePx").toInt(), 30);
        std::unique_ptr<QWidget> toolbar(orange::createReadingToolbar());
        auto* size = toolbar->findChild<QSpinBox*>("readingFontSize");
        QCOMPARE(size->value(), 30);
        preferences.setFontSize(100);
        QCOMPARE(size->value(), 32);
        QVERIFY(!toolbar->findChild<QToolButton*>("fontLarger")->isEnabled());
        preferences.setFontSize(-5);
        QCOMPARE(size->value(), 12);
        QVERIFY(!toolbar->findChild<QToolButton*>("fontSmaller")->isEnabled());
        QTest::mouseClick(toolbar->findChild<QPushButton*>("fontReset"), Qt::LeftButton);
        QCOMPARE(size->value(), 15);
        QCOMPARE(first.sourceText(), source);
    }
    void largeFormulaKeepsRequestedSize() {
        orange::MathBrowser browser;
        browser.resize(300, 300);
        browser.show();
        browser.showText("$$\\int_0^{2\\pi}d\\theta\\int_0^{\\pi/4}d\\varphi\\int_0^2 "
                         "f(r^2)r^2\\sin\\varphi\\,dr$$");
        orange::ReadingPreferences::instance().setFontSize(32);
        QTRY_VERIFY(browser.horizontalScrollBar()->maximum() > 0);
        QCOMPARE(browser.errorCount(), 0);
        QCOMPARE(browser.document()->defaultFont().pixelSize(), 32);
    }
    void commonFormulas_data() {
        QTest::addColumn<QString>("source");
        QTest::newRow("fraction") << QString("正文 $\\frac{x^2+1}{\\sqrt{1-x}}$ 尾部");
        QTest::newRow("readable-fraction") << QString("正文 $\\dfrac{f(x)-f(1)}{x-1}$");
        QTest::newRow("limit") << QString(
            "$$\\limsup\\limits_{n\\to\\infty}\\sqrt[n]{|u_n|}=x^2$$");
        QTest::newRow("series") << QString(
            "$$\\sum_{n=1}^{\\infty}\\left(\\frac{3+(-1)^n}{4}\\right)^n x^{2n}$$");
        QTest::newRow("integral") << QString("\\[\\int_0^{2\\pi}d\\theta\\int_0^{\\pi/"
                                             "4}d\\varphi\\int_0^2f(r^2)r^2\\sin\\varphi\\,dr\\]");
        QTest::newRow("matrix") << QString("\\(A=\\begin{pmatrix}1&0\\\\0&1\\end{pmatrix}\\)");
        QTest::newRow("cases") << QString(
            "$$f(x)=\\begin{cases}x^2,&x\\ne0\\\\0,&x=0\\end{cases}$$");
        QTest::newRow("greek") << QString("$\\alpha+\\beta+\\lambda+\\Omega+\\rho$");
    }
    void commonFormulas() {
        QFETCH(QString, source);
        orange::MathBrowser browser;
        browser.resize(720, 500);
        browser.showText(source);
        QCOMPARE(browser.errorCount(), 0);
        QCOMPARE(browser.formulaCount(), 1);
        QCOMPARE(browser.sourceText(), source);
        const auto resource =
            browser.document()->resource(QTextDocument::ImageResource, QUrl("orange-math:0"));
        QVERIFY(resource.canConvert<QImage>());
        const auto image = resource.value<QImage>();
        QVERIFY(image.width() > 10);
        QVERIFY(image.height() > 10);
        int ink = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 0)
                    ++ink;
        QVERIFY2(ink > 30, "公式渲染必须产生实际笔画，不能只插入空白图像。");
    }
    void mixedTextAndRecovery() {
        orange::MathBrowser browser;
        browser.resize(600, 400);
        browser.showText("<script>纯文本</script> $x^2$ 后面 \\[\\frac{1}{2}\\] 末尾");
        QCOMPARE(browser.formulaCount(), 2);
        QCOMPARE(browser.errorCount(), 0);
        QVERIFY(browser.toPlainText().contains("<script>纯文本</script>"));
        QVERIFY(browser.toPlainText().contains("末尾"));
        browser.showText("未闭合 $x^2");
        QVERIFY(browser.toPlainText().contains("$x^2"));
        browser.showText("错误 $\\frac{1}{2$ 之后 $x+1$");
        QCOMPARE(browser.errorCount(), 1);
        QCOMPARE(browser.formulaCount(), 1);
        QVERIFY(browser.toPlainText().contains("\\frac{1}{2"));
        browser.showText("$\\input{private.tex}$");
        QCOMPARE(browser.errorCount(), 1);
    }
    void embeddedFontsOutsideProject() {
        QTemporaryDir dir;
        const auto original = QDir::currentPath();
        QVERIFY(QDir::setCurrent(dir.path()));
        orange::MathBrowser browser;
        browser.showText("$\\frac{1}{2}$");
        const auto errors = browser.errorCount();
        QVERIFY(QDir::setCurrent(original));
        QCOMPARE(errors, 0);
    }
    void validateMigrationIfRequested() {
        const auto fileName = qEnvironmentVariable("ORANGE_MATH_CHECK_FILE");
        if (fileName.isEmpty())
            QSKIP("未指定用户题目校验文件；标准公式测试已执行。");
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto questions =
            QJsonDocument::fromJson(file.readAll()).object().value("questions").toArray();
        QCOMPARE(questions.size(), 5);
        const auto screenshotDir = qEnvironmentVariable("ORANGE_SCREENSHOT_DIR");
        for (const auto& value : questions) {
            const auto question = orange::questionFromJson(value.toObject());
            orange::MathBrowser browser;
            browser.resize(900, 1400);
            browser.show();
            browser.showQuestion(question);
            QCOMPARE(browser.errorCount(), 0);
            QVERIFY(browser.formulaCount() > 5);
            if (!screenshotDir.isEmpty())
                QVERIFY(browser.grab().save(screenshotDir + "/" + question.id + ".png"));
        }
    }
};
QTEST_MAIN(MathTests)
#include "math_tests.moc"
