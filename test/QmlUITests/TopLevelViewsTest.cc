#include "TopLevelViewsTest.h"

#include <algorithm>
#include <utility>

#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlExpression>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include "AppSettings.h"
#include "Fact.h"
#include "PlanMasterController.h"
#include "ScreenToolsController.h"
#include "SettingsManager.h"
#include "VideoSettings.h"

UT_REGISTER_TEST(TopLevelViewsTest, TestLabel::Integration)

// This test assumes Advanced UI mode is enabled (the default). All views including
// Analyze are expected to be visible.

// Section nav buttons carry a sectionIndex property; match on the label text so
// tests don't depend on section ordering in the page definition JSON.
static QQuickItem* findVisibleSectionButton(QQuickItem* root, const QString& sectionName)
{
    if (!root || !root->isVisible()) {
        return nullptr;
    }
    const auto children = root->childItems();
    if (root->property("sectionIndex").isValid()) {
        for (auto* label : children) {
            if (label->isVisible() && label->property("text").toString() == sectionName) {
                return root;
            }
        }
    }
    for (auto* child : children) {
        if (auto* found = findVisibleSectionButton(child, sectionName)) {
            return found;
        }
    }
    return nullptr;
}

static void setVideoSource(const char* source)
{
    SettingsManager::instance()->videoSettings()->videoSource()->setRawValue(QString::fromLatin1(source));
}

static void collectSettingsDividers(QQuickItem* item, QList<QQuickItem*>& dividers)
{
    if (!item) {
        return;
    }
    if (item->objectName().startsWith(QStringLiteral("settingsDivider_"))) {
        dividers.append(item);
    }
    const auto children = item->childItems();
    for (auto* child : children) {
        collectSettingsDividers(child, dividers);
    }
}

// Settings sidebar dividers are named settingsDivider_<modelIndex>; returns them in model order
static QList<QQuickItem*> findSettingsDividers(QQuickItem* root)
{
    QList<QQuickItem*> dividers;
    collectSettingsDividers(root, dividers);
    std::sort(dividers.begin(), dividers.end(), [](const QQuickItem* a, const QQuickItem* b) {
        return a->objectName().section('_', 1).toInt() < b->objectName().section('_', 1).toInt();
    });
    return dividers;
}

static void collectVisibleSettingsButtons(QQuickItem* item, QList<QQuickItem*>& buttons)
{
    if (!item || !item->isVisible()) {
        return;
    }
    if (item->objectName().startsWith(QStringLiteral("settingsButton_"))) {
        buttons.append(item);
    }
    const auto children = item->childItems();
    for (auto* child : children) {
        collectVisibleSettingsButtons(child, buttons);
    }
}

// The sidebar lays page buttons out top to bottom in model order, so the top-most visible
// button is the first available page. Custom builds may insert pages ahead of General.
static QQuickItem* findFirstVisibleSettingsButton(QQuickItem* root)
{
    QList<QQuickItem*> buttons;
    collectVisibleSettingsButtons(root, buttons);
    const auto first = std::min_element(buttons.cbegin(), buttons.cend(), [](const QQuickItem* a, const QQuickItem* b) {
        return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y();
    });
    return (first == buttons.cend()) ? nullptr : *first;
}

// Sets the video source and returns a guard restoring the original value on scope exit
[[nodiscard]] static auto setVideoSourceWithRestore(const char* source)
{
    Fact* const videoSource = SettingsManager::instance()->videoSettings()->videoSource();
    const QVariant savedVideoSource = videoSource->rawValue();
    videoSource->setRawValue(QString::fromLatin1(source));
    return qScopeGuard([videoSource, savedVideoSource] { videoSource->setRawValue(savedVideoSource); });
}

[[nodiscard]] static auto setIndoorPaletteWithRestore()
{
    Fact* const indoorPalette = SettingsManager::instance()->appSettings()->indoorPalette();
    const QVariant savedPalette = indoorPalette->rawValue();
    indoorPalette->setRawValue(1);
    return qScopeGuard([indoorPalette, savedPalette] { indoorPalette->setRawValue(savedPalette); });
}

QQuickItem* TopLevelViewsTest::_clickSettingsButton(const QString& pageName)
{
    const QString objectName = QStringLiteral("settingsButton_") + pageName;
    QQuickItem* const button = findVisibleItem(_rootItem, objectName);
    if (!button) {
        QTest::qFail(qPrintable(QStringLiteral("Settings button not found: %1").arg(pageName)), __FILE__, __LINE__);
        return nullptr;
    }
    if (!scrollIntoView(button, QStringLiteral("settings_buttonList"))) {
        QTest::qFail(qPrintable(QStringLiteral("Failed to scroll settings button into view: %1").arg(pageName)),
                     __FILE__, __LINE__);
        return nullptr;
    }
    if (!_clickItemAt(button, 0.5, 0.5, objectName)) {
        return nullptr;
    }
    return button;
}

void TopLevelViewsTest::_testDesktopWindowControls()
{
#ifndef Q_OS_WIN
    QSKIP("Integrated window controls are Windows-only");
#else
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("window_close")), "Integrated close button is missing");
    QVERIFY(_window->flags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(clickButton(QStringLiteral("window_maximize")));
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Maximized, TestTimeout::shortMs());
    QVERIFY(clickButton(QStringLiteral("window_maximize")));
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Windowed, TestTimeout::shortMs());

    QQuickItem* const dragArea = findVisibleItem(_rootItem, QStringLiteral("window_dragArea"));
    QVERIFY(dragArea);
    QTest::mouseDClick(_window, Qt::LeftButton, Qt::NoModifier,
                       dragArea->mapToScene(QPointF(dragArea->width() / 2, dragArea->height() / 2)).toPoint());
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Maximized, TestTimeout::shortMs());
    QVERIFY(clickButton(QStringLiteral("window_maximize")));
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Windowed, TestTimeout::shortMs());

    QVERIFY(clickButton(QStringLiteral("window_minimize")));
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Minimized, TestTimeout::shortMs());
    _window->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(_window));

    _window->setMinimumWidth(0);
    _window->setWidth(480);
    QTRY_COMPARE_WITH_TIMEOUT(_rootItem->width(), qreal(480), TestTimeout::shortMs());
    QQuickItem* const closeButton = findVisibleItem(_rootItem, QStringLiteral("window_close"));
    QQuickItem* const compactMenu = findVisibleItem(_rootItem, QStringLiteral("workspace_compactMenu"));
    QVERIFY(closeButton);
    QVERIFY(compactMenu);
    QVERIFY(compactMenu->mapToScene(QPointF(compactMenu->width(), 0)).x() <= closeButton->mapToScene(QPointF()).x());
    QQmlExpression dirtyMission(qmlContext(_window), _window,
                                QStringLiteral("planView._planMasterController.missionController.dirty = true"));
    dirtyMission.evaluate();
    QVERIFY2(!dirtyMission.hasError(), qPrintable(dirtyMission.error().toString()));
    QVERIFY(clickButton(QStringLiteral("window_close")));
    QVERIFY(waitForDialog(QCoreApplication::translate("MainWindow", "Unsaved Mission")));
    QVERIFY(rejectDialog());
    QVERIFY(_window->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!_window->property("_reentrantCloseGuard").toBool(), TestTimeout::shortMs());
    QQmlExpression planController(qmlContext(_window), _window, QStringLiteral("planView._planMasterController"));
    PlanMasterController* const masterController = planController.evaluate().value<PlanMasterController*>();
    QVERIFY2(!planController.hasError(), qPrintable(planController.error().toString()));
    QVERIFY(masterController);
    QTemporaryDir savedPlan;
    QVERIFY(savedPlan.isValid());
    QVERIFY(masterController->saveToFile(savedPlan.filePath(QStringLiteral("window-close-test"))));
    QVERIFY(!masterController->dirtyForSave());
    QVERIFY(clickButton(QStringLiteral("window_close")));
    QTRY_VERIFY_WITH_TIMEOUT(!_window->isVisible(), TestTimeout::shortMs());
#endif
}

void TopLevelViewsTest::_testDesktopWindowResizeEdges()
{
#ifndef Q_OS_WIN
    QSKIP("Integrated window resize handles are Windows-only");
#else
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    _window->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(_window));
    QTRY_COMPARE_WITH_TIMEOUT(_window->visibility(), QWindow::Windowed, TestTimeout::shortMs());

    QQuickItem* const dragArea = findVisibleItem(_rootItem, QStringLiteral("window_dragArea"));
    QVERIFY(dragArea);
    QTest::mouseMove(_window, dragArea->mapToScene(QPointF(dragArea->width() / 2, dragArea->height() / 2)).toPoint());
    QTRY_COMPARE_WITH_TIMEOUT(_window->cursor().shape(), Qt::ArrowCursor, TestTimeout::shortMs());

    const QList<std::pair<QPoint, Qt::CursorShape>> resizePoints = {
        {QPoint(_window->width() / 2, 1), Qt::SizeVerCursor},
        {QPoint(_window->width() / 2, _window->height() - 2), Qt::SizeVerCursor},
        {QPoint(1, _window->height() / 2), Qt::SizeHorCursor},
        {QPoint(_window->width() - 2, _window->height() / 2), Qt::SizeHorCursor},
        {QPoint(1, 1), Qt::SizeFDiagCursor},
        {QPoint(_window->width() - 2, 1), Qt::SizeBDiagCursor},
        {QPoint(1, _window->height() - 2), Qt::SizeBDiagCursor},
        {QPoint(_window->width() - 2, _window->height() - 2), Qt::SizeFDiagCursor},
    };

    for (const auto& [position, expectedCursor] : resizePoints) {
        QTest::mouseMove(_window, position);
        QTRY_COMPARE_WITH_TIMEOUT(_window->cursor().shape(), expectedCursor, TestTimeout::shortMs());
    }
#endif
}

void TopLevelViewsTest::_testWorkspaceNavigation()
{
    const auto restorePalette = setIndoorPaletteWithRestore();
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QQuickItem* const navigation = findVisibleItem(_rootItem, QStringLiteral("workspaceNavigation"));
    QVERIFY2(navigation, "Persistent workspace navigation is missing");

    const QStringList workspaces = {QStringLiteral("fly"), QStringLiteral("plan"), QStringLiteral("vehicle"),
                                    QStringLiteral("analyze"), QStringLiteral("settings")};
    for (const QString& workspace : workspaces) {
        QVERIFY2(clickButton(QStringLiteral("workspace_") + workspace), qPrintable(workspace));
        QTRY_COMPARE_WITH_TIMEOUT(navigation->property("currentWorkspace").toString(), workspace,
                                  TestTimeout::mediumMs());
        const QString viewName = workspace == QStringLiteral("fly")    ? QStringLiteral("mainView_fly")
                                 : workspace == QStringLiteral("plan") ? QStringLiteral("mainView_plan")
                                                                       : QStringLiteral("mainView_toolDrawer");
        QQuickItem* const view = findVisibleItem(_rootItem, viewName);
        QVERIFY2(view, qPrintable(viewName));
        QTRY_VERIFY2_WITH_TIMEOUT(
            view->mapToScene(QPointF()).y() >= navigation->mapToScene(QPointF()).y() + navigation->height(),
            "Workspace content overlaps the navigation bar", TestTimeout::shortMs());
    }

    QQmlExpression blockNavigation(qmlContext(_window), _window,
                                   QStringLiteral("globals.navigationBlockedReason = 'Navigation test'"));
    blockNavigation.evaluate();
    QVERIFY2(!blockNavigation.hasError(), qPrintable(blockNavigation.error().toString()));
    const auto restoreNavigation = qScopeGuard([this] {
        QQmlExpression expression(qmlContext(_window), _window, QStringLiteral("globals.navigationBlockedReason = ''"));
        expression.evaluate();
    });

    QVERIFY(clickButton(QStringLiteral("workspace_plan")));
    QCOMPARE(navigation->property("currentWorkspace").toString(), QStringLiteral("settings"));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("mainView_toolDrawer")));
}

void TopLevelViewsTest::_testWorkspaceNavigationNarrow()
{
    const auto restorePalette = setIndoorPaletteWithRestore();
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    const int originalWidth = _window->width();
    const int originalMinimumWidth = _window->minimumWidth();
    const auto restoreWidth = qScopeGuard([this, originalWidth, originalMinimumWidth] {
        _window->setMinimumWidth(originalMinimumWidth);
        _window->setWidth(originalWidth);
    });
    _window->setMinimumWidth(0);
    _window->setWidth(480);
    QTRY_COMPARE_WITH_TIMEOUT(_rootItem->width(), qreal(480), TestTimeout::shortMs());

    QQuickItem* const menu = findVisibleItem(_rootItem, QStringLiteral("workspace_compactMenu"));
    QVERIFY2(menu, "Narrow windows must retain the workspace chooser");
    QVERIFY(clickButton(QStringLiteral("workspace_compactMenu")));
    QVERIFY(clickButton(QStringLiteral("toolbar_viewPlan")));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("mainView_plan")));
    QVERIFY(clickButton(QStringLiteral("workspace_returnFly")));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("mainView_fly")));
}

void TopLevelViewsTest::_testNavigateViews()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("toolbar_qgcLogo")), "Q logo button not found");

    // Navigate to a view and verify which main panels become visible
    auto navigateAndVerify = [this](const QString& view, bool expectFlyView, bool expectPlanView,
                                    bool expectToolDrawer) {
        const QString buttonName = QStringLiteral("toolbar_view") + view;
        QVERIFY2(clickToolSelectDropdownButton(buttonName),
                 qPrintable(QStringLiteral("Failed to navigate to %1").arg(view)));

        const int flyTimeout = expectFlyView ? 1000 : 0;
        const int planTimeout = expectPlanView ? 1000 : 0;
        const int drawerTimeout = expectToolDrawer ? 1000 : 0;
        QVERIFY2((findVisibleItem(_rootItem, QStringLiteral("mainView_fly"), flyTimeout) != nullptr) == expectFlyView,
                 qPrintable(QStringLiteral("mainView_fly visibility wrong after switching to %1").arg(view)));
        QVERIFY2(
            (findVisibleItem(_rootItem, QStringLiteral("mainView_plan"), planTimeout) != nullptr) == expectPlanView,
            qPrintable(QStringLiteral("mainView_plan visibility wrong after switching to %1").arg(view)));
        QVERIFY2((findVisibleItem(_rootItem, QStringLiteral("mainView_toolDrawer"), drawerTimeout) != nullptr) ==
                     expectToolDrawer,
                 qPrintable(QStringLiteral("mainView_toolDrawer visibility wrong after switching to %1").arg(view)));
        QTest::qWait(_viewDelay);
    };

    //                      view name      flyView  planView  toolDrawer
    navigateAndVerify(QStringLiteral("Fly"), true, false, false);
    navigateAndVerify(QStringLiteral("Plan"), false, true, false);
    navigateAndVerify(QStringLiteral("Configure"), false, true, true);
    navigateAndVerify(QStringLiteral("Analyze"), false, true, true);
    navigateAndVerify(QStringLiteral("Settings"), false, true, true);

    // Navigate through settings pages that are unconditionally visible
    {
        navigateAndVerify(QStringLiteral("Settings"), false, true, true);

        const QStringList settingsPages = {
            QStringLiteral("General"),        QStringLiteral("Fly View"),   QStringLiteral("Plan View"),
            QStringLiteral("ADSB Server"),    QStringLiteral("Comm Links"), QStringLiteral("App Logging"),
            QStringLiteral("App Log Viewer"), QStringLiteral("Maps"),       QStringLiteral("Remote ID"),
            QStringLiteral("Telemetry"),      QStringLiteral("Video"),      QStringLiteral("Help"),
        };

        for (const QString& page : settingsPages) {
            if (!_clickSettingsButton(page)) {
                return;
            }

            const QString expectedObjectName = QStringLiteral("settingsPage_") + QString(page).remove(' ');
            QVERIFY2(findVisibleItem(_rootItem, expectedObjectName),
                     qPrintable(QStringLiteral("Settings page objectName not found: %1").arg(expectedObjectName)));
        }
    }

    stopUI();
}

void TopLevelViewsTest::_testSettingsSectionVisibility()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    const auto restoreVideoSource = setVideoSourceWithRestore(VideoSettings::videoSourceRTSP);

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    QQuickItem* const videoButton = _clickSettingsButton(QStringLiteral("Video"));
    if (!videoButton) {
        return;
    }
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video")));

    QQuickItem* settingsSection = nullptr;
    QTRY_VERIFY((settingsSection = findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings"))));
    QVERIFY(_clickItemAt(settingsSection, 0.5, 0.5, QStringLiteral("section Settings")));

    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsGroup_Settings")));

    setVideoSource(VideoSettings::videoDisabled);

    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));
    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsGroup_Settings"), 0));
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsGroup_VideoSource"), 0));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video")));
}

// Primary #14898 regression: hidden sections must not reappear in the nav tree
// after another page is selected and the Video page contents unload.
void TopLevelViewsTest::_testSettingsHiddenSectionAfterPageSwitch()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    const auto restoreVideoSource = setVideoSourceWithRestore(VideoSettings::videoSourceRTSP);

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    QQuickItem* const videoButton = _clickSettingsButton(QStringLiteral("Video"));
    if (!videoButton) {
        return;
    }
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video")));
    QTRY_VERIFY(findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));

    setVideoSource(VideoSettings::videoDisabled);
    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));

    // Select another page; the Video entry stays expanded in the nav tree,
    // which is the path that used to resurrect the hidden sections
    if (!_clickSettingsButton(QStringLiteral("General"))) {
        return;
    }
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_General"), 0));
    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));
    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Connection")));

    // Back to Video: panel must show the remaining content, not go blank
    if (!_clickSettingsButton(QStringLiteral("Video"))) {
        return;
    }
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video"), 0));
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsGroup_VideoSource"), 0));
    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));
}

// When a page collapses to a single visible section while one of its sections is
// selected, the selection must normalize to the page itself so the nav still has
// a checked item.
void TopLevelViewsTest::_testSettingsSectionCollapseToSingle()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    const auto restoreVideoSource = setVideoSourceWithRestore(VideoSettings::videoSourceRTSP);

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    QQuickItem* const videoButton = _clickSettingsButton(QStringLiteral("Video"));
    if (!videoButton) {
        return;
    }
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video")));

    // Select the always-visible "Video Source" section
    QQuickItem* sourceSection = nullptr;
    QTRY_VERIFY((sourceSection = findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Video Source"))));
    QVERIFY(_clickItemAt(sourceSection, 0.5, 0.5, QStringLiteral("section Video Source")));
    QTRY_VERIFY(!videoButton->property("checked").toBool());

    // Disabling the source hides all other sections, leaving only "Video Source"
    setVideoSource(VideoSettings::videoDisabled);

    QTRY_VERIFY(!findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Video Source")));
    QTRY_VERIFY(videoButton->property("checked").toBool());
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsGroup_VideoSource"), 0));
}

// When the selected page becomes entirely unavailable the view must fall back to
// the first available page instead of leaving stale content or a blank panel.
void TopLevelViewsTest::_testSettingsPageUnavailableFallback()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    VideoSettings* const videoSettings = SettingsManager::instance()->videoSettings();
    const auto restoreUserVisible = qScopeGuard([videoSettings] { videoSettings->setUserVisible(true); });

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    if (!_clickSettingsButton(QStringLiteral("Video"))) {
        return;
    }
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video"), 0));

    videoSettings->setUserVisible(false);

    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsButton_Video"), 0));
    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video"), 0));

    QQuickItem* const firstButton = findFirstVisibleSettingsButton(_rootItem);
    QVERIFY(firstButton);
    QTRY_VERIFY2(firstButton->property("checked").toBool(), qPrintable(firstButton->objectName()));

    QQuickItem* const rightPanel = findVisibleItem(_rootItem, QStringLiteral("settings_rightPanel"));
    QVERIFY(rightPanel);
    QTRY_VERIFY(rightPanel->property("item").value<QQuickItem*>());
}

// Search must not match or offer navigation to sections that are hidden.
void TopLevelViewsTest::_testSettingsSearchExcludesHiddenSections()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    const auto restoreVideoSource = setVideoSourceWithRestore(VideoSettings::videoSourceRTSP);

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));

    QQuickItem* const searchField = findVisibleItem(_rootItem, QStringLiteral("settings_searchField"));
    QVERIFY(searchField);

    // "zero-copy" is a keyword of the Video page's hideable "Settings" section
    searchField->setProperty("text", QStringLiteral("zero-copy"));
    QQuickItem* videoButton = nullptr;
    QTRY_VERIFY((videoButton = findVisibleItem(_rootItem, QStringLiteral("settingsButton_Video"), 0)));
    QTRY_VERIFY(findVisibleSectionButton(videoButton->parentItem(), QStringLiteral("Settings")));

    setVideoSource(VideoSettings::videoDisabled);

    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsButton_Video"), 0));

    // The page vanished because its only search match is in the now-hidden section,
    // not because the page became unavailable: clearing the search brings it back
    searchField->setProperty("text", QString());
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsButton_Video"), 0));
}

// A divider only shows between two groups that both contain an available page. The
// trailing divider precedes the debug-only pages, so it must hide in release builds.
void TopLevelViewsTest::_testSettingsDividerVisibility()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("settings_buttonList")));

    // SettingsPages.json: dividers after Plan View, after 3D View, after Help
    const QList<QQuickItem*> dividers = findSettingsDividers(_rootItem);
    QCOMPARE(dividers.size(), 3);
    QTRY_VERIFY(dividers[0]->isVisible());
    QTRY_VERIFY(dividers[1]->isVisible());
    QTRY_COMPARE(dividers[2]->isVisible(), ScreenToolsController::isDebug());

    // Search flattens the list into matches only
    QQuickItem* const searchField = findVisibleItem(_rootItem, QStringLiteral("settings_searchField"));
    QVERIFY(searchField);
    searchField->setProperty("text", QStringLiteral("video"));
    for (QQuickItem* divider : dividers) {
        QTRY_VERIFY(!divider->isVisible());
    }

    searchField->setProperty("text", QString());
    QTRY_VERIFY(dividers[0]->isVisible());
    QTRY_VERIFY(dividers[1]->isVisible());
    QTRY_COMPARE(dividers[2]->isVisible(), ScreenToolsController::isDebug());
}

// With no vehicle the component group is empty and Parameters is hidden: the divider
// after Summary shows only because Firmware follows it, and the one before
// Parameters/Firmware must not leave a dangling gap.
void TopLevelViewsTest::_testVehicleConfigDividersNoVehicle()
{
    startUI();
    if (QTest::currentTestFailed())
        return;

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewConfigure")));
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("vehicleConfig_root"), 3000));

    QQuickItem* const parametersButton = findItem(_rootItem, QStringLiteral("vehicleConfig_parametersButton"));
    QQuickItem* const firmwareButton = findItem(_rootItem, QStringLiteral("vehicleConfig_firmwareButton"));
    QQuickItem* const summaryDivider = findItem(_rootItem, QStringLiteral("vehicleConfig_summaryDivider"));
    QQuickItem* const componentsDivider = findItem(_rootItem, QStringLiteral("vehicleConfig_componentsDivider"));
    QVERIFY(parametersButton);
    QVERIFY(firmwareButton);
    QVERIFY(summaryDivider);
    QVERIFY(componentsDivider);

    QTRY_VERIFY(!parametersButton->isVisible());
    QTRY_VERIFY(firmwareButton->isVisible());
    QTRY_VERIFY(summaryDivider->isVisible());
    QTRY_VERIFY(!componentsDivider->isVisible());
}

void TopLevelViewsTest::_testDiscoveredCameraReceiverSettingsVisible()
{
#ifndef QGC_GST_STREAMING
    QSKIP("GStreamer receiver settings are unavailable in this build");
#else
    // The default source: a camera-advertised stream plays without the user picking one, and nothing writes it.
    const auto restoreVideoSource = setVideoSourceWithRestore(VideoSettings::videoDisabled);

    Fact* const lowLatencyMode = SettingsManager::instance()->videoSettings()->lowLatencyMode();
    const QVariant savedLowLatencyMode = lowLatencyMode->rawValue();
    const auto restoreLowLatencyMode =
        qScopeGuard([lowLatencyMode, savedLowLatencyMode] { lowLatencyMode->setRawValue(savedLowLatencyMode); });
    lowLatencyMode->setRawValue(false);

    startUI();
    if (QTest::currentTestFailed())
        return;

    QVERIFY(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewSettings")));
    if (!_clickSettingsButton(QStringLiteral("Video"))) {
        return;
    }

    QQuickItem* const videoPage = findVisibleItem(_rootItem, QStringLiteral("settingsPage_Video"));
    QVERIFY(videoPage);

    // autoStreamConfig and isStreamSource mirror read-only VideoManager properties. Override the
    // page bindings to isolate these UI visibility rules from camera transport.
    QVERIFY(videoPage->setProperty("autoStreamConfig", true));
    QVERIFY(videoPage->setProperty("isStreamSource", true));

    QQuickItem* const sourceGroup = findVisibleItem(_rootItem, QStringLiteral("settingsGroup_VideoSource"));
    QVERIFY(sourceGroup);
    QTRY_VERIFY(!sourceGroup->isEnabled());
    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsGroup_Connection"), 0));
    QTRY_VERIFY(!findVisibleItem(_rootItem, QStringLiteral("settingsTextField_aspectRatio"), 0));

    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsCheckBox_disableWhenDisarmed"), 0));
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsCheckBox_lowLatencyMode"), 0));
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsTextField_rtpJitterLatencyMs"), 0));
    QTRY_VERIFY(findVisibleItem(_rootItem, QStringLiteral("settingsCheckBox_rtspAutoReconnect"), 0));

    stopUI();
#endif
}
