#include "FlyViewOccluderUITest.h"

#include <functional>
#include <optional>

#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtPositioning/QGeoCoordinate>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "Fact.h"
#include "FactValueGrid.h"
#include "FlyViewSettings.h"
#include "GeoMapCamera.h"
#include "GeoScene.h"
#include "MapPositionTracker.h"
#include "MockLink.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"
#include "VideoSettings.h"
#include "Viewer3DSettings.h"

UT_REGISTER_TEST(FlyViewOccluderUITest, TestLabel::Integration)

namespace {
// Generous ceilings: first appearance of loader/engine-gated items vs. QTRY settle polling
constexpr int kItemAppearTimeoutMs = 10000;
constexpr int kSettleTimeoutMs = 5000;
}  // namespace

void FlyViewOccluderUITest::_testDesktopWorkspaceKeepsMapClear()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    Fact* const viewer3DEnabled = SettingsManager::instance()->viewer3DSettings()->enabled();
    const QVariant savedViewer3DEnabled = viewer3DEnabled->rawValue();
    const auto restoreViewer3D =
        qScopeGuard([viewer3DEnabled, savedViewer3DEnabled] { viewer3DEnabled->setRawValue(savedViewer3DEnabled); });
    viewer3DEnabled->setRawValue(false);

    const QSize savedSize = _window->size();
    const int savedMinimumWidth = _window->minimumWidth();
    const auto restoreWindow = qScopeGuard([this, savedSize, savedMinimumWidth] {
        _window->setMinimumWidth(savedMinimumWidth);
        _window->resize(savedSize);
    });
    _window->setMinimumWidth(0);
    _window->resize(2440, 1000);

    QQuickItem* const missionPanel = findVisibleItem(_rootItem, QStringLiteral("flyViewMissionPanel"));
    QVERIFY2(missionPanel, "The desktop Fly workspace must show the current mission beside the map");
    QQuickItem* const instruments = findVisibleItem(_rootItem, QStringLiteral("flyViewBottomRightRowLayout"));
    QVERIFY(instruments);
    QTRY_VERIFY_WITH_TIMEOUT(
        missionPanel->mapToScene(QPointF(missionPanel->width(), 0)).x() + 300 < instruments->mapToScene(QPointF()).x(),
        kSettleTimeoutMs);
    QQuickItem* const widgetLayer = findVisibleItem(_rootItem, QStringLiteral("flyViewWidgetLayer"));
    QVERIFY(widgetLayer);
    QObject* const occluders = widgetLayer->property("occluders").value<QObject*>();
    QVERIFY(occluders);
    const QRectF missionOccluder = occluders->property("missionPanel").toRectF();
    QCOMPARE(missionOccluder.size(), missionPanel->size());
    const qreal toolsMargin = widgetLayer->property("_toolsMargin").toReal();
    QTRY_VERIFY_WITH_TIMEOUT(occluders->property("toolStrip").toRectF().isEmpty(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(missionPanel->x() - toolsMargin) < 1, kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(occluders->property("missionPanel").toRectF().x() - toolsMargin) < 1,
                             kSettleTimeoutMs);
    saveScreenshot(QStringLiteral("fly-dock-left"));
    viewer3DEnabled->setRawValue(true);
    QTRY_VERIFY_WITH_TIMEOUT(!occluders->property("toolStrip").toRectF().isEmpty(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(
        qAbs(missionPanel->x() - occluders->property("toolStrip").toRectF().right() - toolsMargin) < 1,
        kSettleTimeoutMs);
    viewer3DEnabled->setRawValue(false);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(missionPanel->x() - toolsMargin) < 1, kSettleTimeoutMs);
    QVERIFY(verifyText(QStringLiteral("flyViewMissionState"),
                       QCoreApplication::translate("FlyViewWidgetLayer", "No vehicle connected"),
                       QStringLiteral("offline mission state")));
    QVERIFY(verifyEnabled(QStringLiteral("flyDock_takeoff"), false, QStringLiteral("offline takeoff disabled")));
    QVERIFY(verifyText(QStringLiteral("flyTelemetryUnavailable"),
                       QCoreApplication::translate("TelemetryValuesBar", "No vehicle connected"),
                       QStringLiteral("offline telemetry unavailable")));
    QVERIFY(verifyText(QStringLiteral("flyInstrumentUnavailable"),
                       QCoreApplication::translate("FlyViewInstrumentPanel", "No vehicle connected"),
                       QStringLiteral("offline instruments unavailable")));
    QVERIFY(verifyVisibility(QStringLiteral("flyToolStrip_takeoffButton"), false,
                             QStringLiteral("desktop takeoff action appears only in the dock")));
    QVERIFY(verifyVisibility(QStringLiteral("flyToolStrip_landButton"), false,
                             QStringLiteral("desktop landing action appears only in the dock")));

    QQuickItem* const dock = findVisibleItem(_rootItem, QStringLiteral("flyViewRightDock"));
    QVERIFY(dock);
    QVERIFY(dock->mapToScene(QPointF()).x() > _window->width() / 2);
    QTRY_VERIFY_WITH_TIMEOUT(missionPanel->height() < widgetLayer->height() / 2, kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dock->height() < widgetLayer->height() * 0.75, kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(instruments->y() < widgetLayer->height() / 4, kSettleTimeoutMs);
    QQuickItem* const actions = findVisibleItem(_rootItem, QStringLiteral("flyViewFlightActions"));
    QVERIFY(actions);
    QTRY_VERIFY_WITH_TIMEOUT(actions->y() >= instruments->y() + instruments->height(), kSettleTimeoutMs);
    const auto dockContains = [dock](QQuickItem* item) {
        return item->isVisible() && item->width() > 0 && item->height() > 0 &&
               QRectF(QPointF(), dock->size()).contains(dock->mapRectFromItem(item, QRectF(QPointF(), item->size())));
    };
    _window->resize(2438, 1410);
    QTRY_VERIFY_WITH_TIMEOUT(widgetLayer->property("_desktopLayout").toBool(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContains(instruments), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContains(actions), kSettleTimeoutMs);
    QQuickItem* const telemetry = findVisibleItem(_rootItem, QStringLiteral("flyViewTelemetryValues"));
    QVERIFY(telemetry);
    QQuickItem* const instrumentPanel = findVisibleItem(_rootItem, QStringLiteral("flyViewInstrumentPanel"));
    QVERIFY(instrumentPanel);
    auto* const selectedInstrument =
        qobject_cast<QQuickItem*>(instrumentPanel->property("innerControl").value<QObject*>());
    QVERIFY(selectedInstrument);
    QQuickItem* const takeoffButton = findVisibleItem(_rootItem, QStringLiteral("flyDock_takeoff"));
    QVERIFY(takeoffButton);
    const auto dockContainsContent = [&dockContains, telemetry, selectedInstrument, takeoffButton] {
        return dockContains(telemetry) && dockContains(selectedInstrument) && dockContains(takeoffButton);
    };
    QTRY_VERIFY_WITH_TIMEOUT(dockContainsContent(), kSettleTimeoutMs);
    auto* const grid = qobject_cast<FactValueGrid*>(telemetry->property("factValueGrid").value<QObject*>());
    QVERIFY(grid);
    const int savedColumns = grid->columns()->count();
    const int savedRows = grid->property("rowCount").toInt();
    const auto restoreTelemetry = qScopeGuard([grid, savedColumns, savedRows] {
        while (grid->columns()->count() > savedColumns) {
            grid->deleteLastColumn();
        }
        while (grid->property("rowCount").toInt() > savedRows) {
            grid->deleteLastRow();
        }
    });

    for (int column = 0; column < 24; ++column) {
        grid->appendColumn();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!missionPanel->isVisible(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(occluders->property("rightDock").toRectF().isEmpty(), kSettleTimeoutMs);
    while (grid->columns()->count() > savedColumns) {
        grid->deleteLastColumn();
    }
    QTRY_VERIFY_WITH_TIMEOUT(missionPanel->isVisible(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContains(instruments) && dockContains(actions), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContainsContent(), kSettleTimeoutMs);

    for (int row = 0; row < 48; ++row) {
        grid->appendRow();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!missionPanel->isVisible(), kSettleTimeoutMs);
    while (grid->property("rowCount").toInt() > savedRows) {
        grid->deleteLastRow();
    }
    QTRY_VERIFY_WITH_TIMEOUT(missionPanel->isVisible(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContains(instruments) && dockContains(actions), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContainsContent(), kSettleTimeoutMs);

    _window->resize(731, 700);
    QTRY_VERIFY_WITH_TIMEOUT(!missionPanel->isVisible(), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(occluders->property("missionPanel").toRectF().isEmpty(), kSettleTimeoutMs);
    QVERIFY(findVisibleItem(_rootItem, QStringLiteral("flyViewBottomRightRowLayout")));
    QVERIFY(verifyVisibility(QStringLiteral("flyToolStrip_takeoffButton"), true,
                             QStringLiteral("narrow workspace restores the takeoff action")));
    QVERIFY(verifyEnabled(QStringLiteral("flyToolStrip_takeoffButton"), false,
                          QStringLiteral("narrow offline takeoff disabled")));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(instruments->y() + instruments->height() - widgetLayer->height()) < 1,
                             kSettleTimeoutMs);
    _window->resize(2440, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(missionPanel->isVisible(), kSettleTimeoutMs);
    QVERIFY(verifyVisibility(QStringLiteral("flyToolStrip_takeoffButton"), false,
                             QStringLiteral("desktop restoration removes the duplicate takeoff action")));
    QTRY_VERIFY_WITH_TIMEOUT(dockContains(instruments) && dockContains(actions), kSettleTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(dockContainsContent(), kSettleTimeoutMs);
}

void FlyViewOccluderUITest::_testVehicleUnderWidgetRecenters_data()
{
    QTest::addColumn<bool>("geoMapEngine");

    QTest::newRow("GeoMap") << true;
    QTest::newRow("QtLocation") << false;
}

// Map following treats the Fly View widgets as occluders: a vehicle hidden under
// the bottom right instrument row recenters the map, a vehicle in the clear does not
void FlyViewOccluderUITest::_testVehicleUnderWidgetRecenters()
{
    QFETCH(bool, geoMapEngine);

    Fact* const geoEngineFact = SettingsManager::instance()->flyViewSettings()->useGeoMapEngine();
    const QVariant savedEnabled = geoEngineFact->rawValue();
    const auto guard = qScopeGuard([geoEngineFact, savedEnabled] { geoEngineFact->setRawValue(savedEnabled); });
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QRegularExpression::escape(
                         QCoreApplication::translate("Fact", "Restart application for changes to take effect."))));
    geoEngineFact->setRawValue(geoMapEngine);

    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this, geoMapEngine](QPointer<MockLink>, Vehicle* vehicle) {
            QQuickItem* mapItem = nullptr;
            MapPositionTracker* positionTracker = nullptr;
            // Moves the map so the vehicle lands on screenPoint, in mapItem coordinates
            std::function<bool(const QPointF&)> placeVehicleAt;
            if (geoMapEngine) {
                const std::optional<bool> rhiBased = expectSoftwareBackendWarnings(/*strict*/ false);
                QVERIFY2(rhiBased.has_value(), "No renderer interface on the main window");

                mapItem = findVisibleItem(_rootItem, QStringLiteral("geoMapViewport"), kItemAppearTimeoutMs);
                QVERIFY2(mapItem, "GeoMap 3D viewport not visible");
                QQuickItem* const geoMap = mapItem->parentItem();
                auto* const cam = geoMap->findChild<GeoMapCamera*>(QStringLiteral("geoMapCamera"));
                QVERIFY2(cam, "GeoMapCamera not found");
                auto* const scene = geoMap->findChild<GeoScene*>(QStringLiteral("geoMapScene"));
                QVERIFY2(scene, "GeoScene not found");
                positionTracker = geoMap->findChild<MapPositionTracker*>();
                placeVehicleAt = [geoMap, cam, scene](const QPointF& screenPoint) {
                    const auto vehicleCoordinate =
                        geoMap->property("_trackedVehicleCoordinate").value<QGeoCoordinate>();
                    cam->setCenter(scene->centerForCoordinateAtScreenPoint(vehicleCoordinate, screenPoint));
                    return true;
                };
            } else {
                mapItem = findVisibleItem(_rootItem, QStringLiteral("flyViewMap"), kItemAppearTimeoutMs);
                QVERIFY2(mapItem, "QtLocation fly view map not visible");
                positionTracker =
                    qobject_cast<MapPositionTracker*>(mapItem->property("positionTracker").value<QObject*>());
                placeVehicleAt = [mapItem, vehicle](const QPointF& screenPoint) {
                    QPointF vehiclePoint;
                    QGeoCoordinate center;
                    const QPointF mapCenter(mapItem->width() / 2, mapItem->height() / 2);
                    return QMetaObject::invokeMethod(mapItem, "fromCoordinate", Q_RETURN_ARG(QPointF, vehiclePoint),
                                                     Q_ARG(QGeoCoordinate, vehicle->coordinate()),
                                                     Q_ARG(bool, false)) &&
                           QMetaObject::invokeMethod(mapItem, "toCoordinate", Q_RETURN_ARG(QGeoCoordinate, center),
                                                     Q_ARG(QPointF, mapCenter + vehiclePoint - screenPoint),
                                                     Q_ARG(bool, false)) &&
                           mapItem->setProperty("center", QVariant::fromValue(center));
                };
            }
            QVERIFY2(positionTracker, "MapPositionTracker not found");

            QQuickItem* const bottomRightRow =
                findVisibleItem(_rootItem, QStringLiteral("flyViewBottomRightRowLayout"), kItemAppearTimeoutMs);
            QVERIFY2(bottomRightRow, "Bottom right instrument row not visible");
            QTRY_VERIFY_WITH_TIMEOUT(bottomRightRow->width() > 0 && bottomRightRow->height() > 0, kSettleTimeoutMs);

            // The one-shot centering on the first vehicle position would also move the map
            QTRY_VERIFY_WITH_TIMEOUT(positionTracker->firstVehiclePositionReceived(), kItemAppearTimeoutMs);

            QSignalSpy recenterSpy(positionTracker, &MapPositionTracker::recenterVehicleTo);
            QVERIFY(placeVehicleAt(QPointF(mapItem->width() / 2, mapItem->height() / 2)));
            QVERIFY_NO_SIGNAL_WAIT(recenterSpy, TestTimeout::shortMs());

            QVERIFY(placeVehicleAt(mapItem->mapFromItem(
                bottomRightRow, QPointF(bottomRightRow->width() / 2, bottomRightRow->height() / 2))));
            QTRY_VERIFY_WITH_TIMEOUT(recenterSpy.count() > 0, kSettleTimeoutMs);
        });
}

// A collapsed PiP only covers the map with its restore button, so only that is an occluder
void FlyViewOccluderUITest::_testCollapsedPipOccludesRestoreButtonOnly()
{
    // A configured video stream is what makes the Fly View offer PiP
    Fact* const videoSource = SettingsManager::instance()->videoSettings()->videoSource();
    const QVariant savedVideoSource = videoSource->rawValue();
    const auto videoGuard =
        qScopeGuard([videoSource, savedVideoSource] { videoSource->setRawValue(savedVideoSource); });
    videoSource->setRawValue(QString::fromLatin1(VideoSettings::videoSourceUDPH264));

    startUI();
    if (QTest::currentTestFailed())
        return;

    const QPointer<QQuickItem> pipView =
        findVisibleItem(_rootItem, QStringLiteral("flyViewPipView"), kItemAppearTimeoutMs);
    QVERIFY2(pipView, "PiP view not visible");
    QQuickItem* const widgetLayer = findVisibleItem(_rootItem, QStringLiteral("flyViewWidgetLayer"));
    QVERIFY2(widgetLayer, "Fly View widget layer not visible");
    QObject* const occluders = widgetLayer->property("occluders").value<QObject*>();
    QVERIFY2(occluders, "Widget layer occluders not found");

    // The collapsed state is persisted: restore it even if an assertion bails out early
    const auto expandGuard = qScopeGuard([pipView] {
        if (pipView) {
            QMetaObject::invokeMethod(pipView, "_setPipIsExpanded", Q_ARG(QVariant, true));
        }
    });
    const auto pipOccluderSize = [occluders] { return occluders->property("pipView").toRectF().size(); };
    const auto pipOccupiedSize = [pipView] { return pipView->property("occupiedRect").toRectF().size(); };

    QCOMPARE(pipOccupiedSize(), pipView->size());
    QCOMPARE(pipOccluderSize(), pipView->size());

    // The hide button only shows on hover
    QVERIFY(QMetaObject::invokeMethod(pipView, "_setPipIsExpanded", Q_ARG(QVariant, false)));
    QCOMPARE_LT(pipOccupiedSize().height(), pipView->height() / 2);
    QCOMPARE(pipOccluderSize(), pipOccupiedSize());

    QVERIFY(QMetaObject::invokeMethod(pipView, "_setPipIsExpanded", Q_ARG(QVariant, true)));
    QCOMPARE(pipOccluderSize(), pipView->size());

    stopUI();
}
