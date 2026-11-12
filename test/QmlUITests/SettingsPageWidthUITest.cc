#include "SettingsPageWidthUITest.h"

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

UT_REGISTER_TEST(SettingsPageWidthUITest, TestLabel::Integration)

namespace {

constexpr int kHerelinkWidth = 731;
constexpr int kHerelinkHeight = 411;
constexpr qreal kWidthTolerancePx = 1;

}  // namespace

void SettingsPageWidthUITest::_testPageFitsHerelinkWidth_data()
{
    QTest::addColumn<QString>("pageName");

    // Pages generated from *.SettingsUI.json that are always available
    QTest::newRow("General") << QStringLiteral("General");
    QTest::newRow("Fly View") << QStringLiteral("Fly View");
    QTest::newRow("Plan View") << QStringLiteral("Plan View");
    QTest::newRow("ADSB Server") << QStringLiteral("ADSB Server");
    QTest::newRow("Comm Links") << QStringLiteral("Comm Links");
    QTest::newRow("App Logging") << QStringLiteral("App Logging");
    QTest::newRow("Maps") << QStringLiteral("Maps");
    QTest::newRow("GNSS Receiver") << QStringLiteral("GNSS Receiver");
    QTest::newRow("RTK Corrections") << QStringLiteral("RTK Corrections");
    QTest::newRow("Remote ID") << QStringLiteral("Remote ID");
    QTest::newRow("Telemetry") << QStringLiteral("Telemetry");
}

void SettingsPageWidthUITest::_testPageFitsHerelinkWidth()
{
    QFETCH(QString, pageName);

    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // MainWindowSavedState sets a minimum size that onscreen platforms enforce
    _window->setMinimumWidth(0);
    _window->setMinimumHeight(0);
    _window->resize(kHerelinkWidth, kHerelinkHeight);
    QTRY_COMPARE_WITH_TIMEOUT(_rootItem->width(), qreal(kHerelinkWidth), TestTimeout::shortMs());

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    const QString buttonName = QStringLiteral("settingsButton_") + pageName;
    QQuickItem* const button = findVisibleItem(_rootItem, buttonName);
    QVERIFY2(button, qPrintable(QStringLiteral("Settings page button not found: %1").arg(buttonName)));
    scrollIntoView(button, QStringLiteral("settings_buttonList"));

    const QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, center.toPoint());

    // Page root objectNames are sanitized to [A-Za-z0-9_]
    QString wrapperName = QStringLiteral("settingsPage_") + pageName;
    wrapperName.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")));
    QVERIFY2(findVisibleItem(_rootItem, wrapperName),
             qPrintable(QStringLiteral("Settings page not found: %1").arg(wrapperName)));

    QQuickItem* const flickable = findVisibleItem(_rootItem, QStringLiteral("settingsPageFlickable"));
    QVERIFY2(flickable, "settingsPageFlickable not found");

    QTRY_VERIFY2_WITH_TIMEOUT(flickable->property("contentWidth").toReal() <= flickable->width() + kWidthTolerancePx,
                              qPrintable(QStringLiteral("contentWidth %1 exceeds flickable width %2")
                                             .arg(flickable->property("contentWidth").toReal())
                                             .arg(flickable->width())),
                              TestTimeout::shortMs());

    // contentWidth is capped by construction, so also check no child is wider than the layout
    QQuickItem* const layout = findItem(_rootItem, QStringLiteral("settingsPageLayout"));
    QVERIFY2(layout, "settingsPageLayout not found");
    const qreal rightEdge = flickable->mapToScene(QPointF(flickable->width(), 0)).x();
    for (const QQuickItem* child : layout->childItems()) {
        if (!child->isVisible()) {
            continue;
        }
        const qreal childRight = child->mapToScene(QPointF(child->width(), 0)).x();
        QVERIFY2(
            childRight <= rightEdge + kWidthTolerancePx,
            qPrintable(QStringLiteral("%1 extends %2 px past the page")
                           .arg(child->objectName().isEmpty() ? QStringLiteral("<unnamed child>") : child->objectName())
                           .arg(childRight - rightEdge)));
    }

    stopUI();
}

void SettingsPageWidthUITest::_testGroupsUseAvailableWidth_data()
{
    QTest::addColumn<QString>("pageName");
    QTest::addColumn<QString>("screenshotPrefix");

    QTest::newRow("General") << QStringLiteral("General") << QStringLiteral("settings-general");
    QTest::newRow("Maps") << QStringLiteral("Maps") << QStringLiteral("settings-maps");
    QTest::newRow("Comm Links") << QStringLiteral("Comm Links") << QStringLiteral("settings-comm-links");
}

void SettingsPageWidthUITest::_testGroupsUseAvailableWidth()
{
    QFETCH(QString, pageName);
    QFETCH(QString, screenshotPrefix);

    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));
    QQuickItem* const button = findVisibleItem(_rootItem, QStringLiteral("settingsButton_") + pageName);
    QVERIFY(button);
    scrollIntoView(button, QStringLiteral("settings_buttonList"));
    const QPointF buttonCenter = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, buttonCenter.toPoint());

    QQuickItem* const layout = findVisibleItem(_rootItem, QStringLiteral("settingsPageLayout"));
    QVERIFY(layout);
    QQuickItem* const flickable = findVisibleItem(_rootItem, QStringLiteral("settingsPageFlickable"));
    QVERIFY(flickable);

    QList<QQuickItem*> groups;
    for (QQuickItem* child : layout->childItems()) {
        if (child->isVisible()) {
            groups.append(child);
        }
    }
    QVERIFY(groups.count() >= 2);
    QQuickItem* const first = groups.at(0);
    QQuickItem* const second = groups.at(1);

    _window->resize(2440, 1200);
    QTRY_VERIFY_WITH_TIMEOUT(second->x() >= first->x() + first->width(), TestTimeout::shortMs());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(first->y()) <= kWidthTolerancePx, TestTimeout::shortMs());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(second->y()) <= kWidthTolerancePx, TestTimeout::shortMs());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(first->width() - second->width()) <= kWidthTolerancePx, TestTimeout::shortMs());
    const qreal groupSpacing = layout->property("rowSpacing").toReal();
    if (groups.count() >= 3) {
        QQuickItem* const third = groups.at(2);
        QTRY_COMPARE_WITH_TIMEOUT(third->x(), first->x(), TestTimeout::shortMs());
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(third->y() - first->height() - groupSpacing) <= kWidthTolerancePx,
                                 TestTimeout::shortMs());
    }
    if (groups.count() >= 4) {
        QQuickItem* const fourth = groups.at(3);
        QTRY_COMPARE_WITH_TIMEOUT(fourth->x(), second->x(), TestTimeout::shortMs());
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(fourth->y() - second->height() - groupSpacing) <= kWidthTolerancePx,
                                 TestTimeout::shortMs());
    }
    saveScreenshot(screenshotPrefix + QStringLiteral("-wide"));

    _window->setMinimumWidth(0);
    _window->setMinimumHeight(0);
    _window->resize(kHerelinkWidth, kHerelinkHeight);
    for (int index = 1; index < groups.count(); ++index) {
        QQuickItem* const previous = groups.at(index - 1);
        QQuickItem* const group = groups.at(index);
        QTRY_COMPARE_WITH_TIMEOUT(group->x(), first->x(), TestTimeout::shortMs());
        QTRY_VERIFY_WITH_TIMEOUT(
            qAbs(group->y() - previous->y() - previous->height() - groupSpacing) <= kWidthTolerancePx,
            TestTimeout::shortMs());
    }
    QTRY_VERIFY_WITH_TIMEOUT(
        flickable->property("contentHeight").toReal() >= groups.last()->y() + groups.last()->height(),
        TestTimeout::shortMs());
    saveScreenshot(screenshotPrefix + QStringLiteral("-narrow"));

    // A category/search filter keeps the existing group and gives it the full page width.
    QQuickItem* const page = flickable->parentItem();
    QVERIFY(page);
    QQuickItem* const filteredGroup = groups.last();
    const int sectionIndex = layout->childItems().indexOf(filteredGroup);
    QVERIFY(page->setProperty("sectionFilter", sectionIndex));
    _window->resize(2440, 1200);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(filteredGroup->width() - layout->width()) <= kWidthTolerancePx,
                             TestTimeout::shortMs());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(filteredGroup->x()) <= kWidthTolerancePx, TestTimeout::shortMs());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(filteredGroup->y()) <= kWidthTolerancePx, TestTimeout::shortMs());
    for (QQuickItem* group : groups) {
        if (group != filteredGroup) {
            QTRY_VERIFY_WITH_TIMEOUT(!group->isVisible(), TestTimeout::shortMs());
        }
    }
    saveScreenshot(screenshotPrefix + QStringLiteral("-section"));
    stopUI();
}
