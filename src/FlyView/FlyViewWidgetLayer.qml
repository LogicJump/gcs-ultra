import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap
import QGroundControl.GeoMap
import QGroundControl.Viewer3D

// This is the ui overlay layer for the widgets/tools for Fly View
Item {
    id: _root

    property rect   pipViewRect:            Qt.rect(0, height, 0, 0)
    property var    occluders:              _occluders
    property var    mapControl
    property var    viewer3DCameraController

    property var    _activeVehicle:         QGroundControl.multiVehicleManager.activeVehicle
    property var    _planMasterController:  globals.planMasterControllerFlyView
    property var    _missionController:     _planMasterController.missionController
    property var    _geoFenceController:    _planMasterController.geoFenceController
    property var    _rallyPointController:  _planMasterController.rallyPointController
    property var    _guidedController:      globals.guidedControllerFlyView
    property real   _margins:               ScreenTools.defaultFontPixelWidth / 2
    property real   _toolsMargin:           ScreenTools.defaultFontPixelWidth * 0.75
    property real   _rightPanelWidth:       ScreenTools.defaultFontPixelWidth * 30
    property real   _layoutSpacing:         ScreenTools.defaultFontPixelWidth
    property bool   _showSingleVehicleUI:   true

    readonly property real _missionPanelWidth: ScreenTools.defaultFontPixelWidth * 30
    readonly property real _toolStripRight: toolStrip.visible ? toolStrip.x + toolStrip.width : 0
    readonly property real _desktopDockWidth: Math.max(_rightPanelWidth, bottomRightRowLayout.stackedWidth,
                                                       topRightColumnLayout.implicitWidth) + _toolsMargin * 2
    readonly property real _desktopDockHeight: topRightColumnLayout.implicitHeight
                                               + bottomRightRowLayout.stackedHeight
                                               + actionHeader.implicitHeight + actionGrid.implicitHeight
                                               + _toolsMargin * 4 + ScreenTools.defaultFontPixelHeight
    readonly property bool _desktopLayout: !ScreenTools.isMobile
                                           && width >= ScreenTools.defaultFontPixelWidth * 160
                                           && height >= ScreenTools.defaultFontPixelHeight * 40
                                           && _desktopDockWidth <= width * 0.25
                                           && toolStrip.width + _toolsMargin * 2 + _missionPanelWidth <= width * 0.25
                                           && _desktopDockHeight <= height
                                           && !virtualJoystickMultiTouch._virtualJoystickEnabled
                                           && !topRightPanel.visible
                                           && mapControl && mapControl.pipState.state === mapControl.pipState.fullState
    readonly property bool _vehicleConnected: _activeVehicle && !_activeVehicle.vehicleLinkManager.communicationLost
    readonly property bool _missionReady: (!_activeVehicle || _activeVehicle.initialPlanRequestComplete) && !_missionController.syncInProgress
    readonly property bool _hasMission: _missionReady && _missionController.visualItems.count > 1

    QGCPalette { id: workspacePalette }

    FlyViewOccluders {
        id:         _occluders
        pipView:    pipViewRect
        missionPanel: missionPanelControl.visible ? Qt.rect(missionPanelControl.x, missionPanelControl.y, missionPanelControl.width, missionPanelControl.height) : Qt.rect(0, 0, 0, 0)
        rightDock: rightDockBackground.visible ? Qt.rect(rightDockBackground.x, rightDockBackground.y, rightDockBackground.width, rightDockBackground.height) : Qt.rect(_root.width, 0, 0, 0)
        toolStrip:  toolStrip.visible ? Qt.rect(toolStrip.x, toolStrip.y, toolStrip.width, toolStrip.height) : Qt.rect(0, 0, 0, 0)
        mapScale:   mapScaleRow.coversMap ? Qt.rect(mapScaleRow.x, mapScaleRow.y, mapScaleRow.width, mapScaleRow.height) : Qt.rect(mapScaleRow.x, mapScaleRow.y, 0, 0)
        topRight: {
            if (topRightPanel.visible) {
                return Qt.rect(topRightPanel.x, topRightPanel.y, topRightPanel.width, topRightPanel.height)
            }
            const column = topRightColumnLayout
            return column.visible ? Qt.rect(column.x, column.y, column.width, column.childrenRect.height) : Qt.rect(_root.width, 0, 0, 0)
        }
        bottomRight: {
            const row = bottomRightRowLayout
            return row.visible ? Qt.rect(row.x, row.y, row.width, row.height) : Qt.rect(_root.width, _root.height, 0, 0)
        }
        // The loader spans the full width but only the stick pads at each end cover the map
        virtualJoystickLeft: {
            const vj = virtualJoystickMultiTouch
            return vj.visible ? Qt.rect(vj.x, vj.y, vj.height, vj.height) : Qt.rect(0, _root.height, 0, 0)
        }
        virtualJoystickRight: {
            const vj = virtualJoystickMultiTouch
            return vj.visible ? Qt.rect(vj.x + vj.width - vj.height, vj.y, vj.height, vj.height) : Qt.rect(_root.width, _root.height, 0, 0)
        }
    }

    Rectangle {
        id:                 missionPanelControl
        objectName:         "flyViewMissionPanel"
        anchors.left:       parent.left
        anchors.leftMargin: _root._toolStripRight + _toolsMargin
        anchors.top:        parent.top
        width:              _root._missionPanelWidth
        height:             _root._hasMission ? Math.max(0, pipViewRect.y - _toolsMargin)
                                              : Math.min(missionColumn.implicitHeight + _toolsMargin * 2, Math.max(0, pipViewRect.y - _toolsMargin))
        visible:            _root._desktopLayout
        color:              workspacePalette.window

        DeadMouseArea { anchors.fill: parent }

        ColumnLayout {
            id:                 missionColumn
            anchors.fill:       parent
            anchors.margins:    _toolsMargin
            spacing:            ScreenTools.defaultFontPixelHeight * 0.5

            QGCLabel {
                Layout.fillWidth:   true
                text:               qsTr("Mission")
                font.weight:        Font.DemiBold
            }

            QGCLabel {
                objectName:         "flyViewMissionState"
                Layout.fillWidth:   true
                text:               !_root._activeVehicle ? qsTr("No vehicle connected")
                                    : !_root._vehicleConnected ? qsTr("Vehicle connection lost")
                                    : _root._activeVehicle.flightMode || qsTr("Waiting for flight mode")
                color:              workspacePalette.textSecondary
                wrapMode:           Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth:   true
                visible:            _root._hasMission && _root._vehicleConnected && _root._missionController.currentMissionIndex >= 0
                text:               qsTr("Current item: %1").arg(_root._missionController.currentMissionIndex)
                wrapMode:           Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth:   true
                visible:            _root._hasMission

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               qsTr("Planned distance")
                    color:              workspacePalette.textSecondary
                    font.pointSize:     ScreenTools.smallFontPointSize
                    wrapMode:           Text.WordWrap
                }

                QGCLabel {
                    text: {
                        const distance = _root._missionController.missionPlannedDistance
                        return isNaN(distance) ? "—"
                            : QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(distance).toFixed(0)
                              + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString
                    }
                }
            }

            Rectangle {
                Layout.fillWidth:   true
                implicitHeight:     1
                color:              workspacePalette.borderSubtle
            }

            QGCLabel {
                Layout.fillWidth:   true
                visible:            !_root._missionReady
                text:               qsTr("Reading mission…")
                color:              workspacePalette.textSecondary
            }

            ListView {
                id:                 missionList
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                clip:               true
                visible:            _root._hasMission
                model:              _root._missionController.visualItems
                ScrollBar.vertical: ScrollBar { }

                delegate: Rectangle {
                    id: missionRow
                    required property var object
                    readonly property bool current: _root._missionReady && _root._vehicleConnected && object.isCurrentItem

                    width:      missionList.width
                    height:     Math.max(ScreenTools.defaultFontPixelHeight * 2, missionRowContent.implicitHeight + _root._toolsMargin)
                    color:      current ? workspacePalette.accentSubtle : "transparent"

                    RowLayout {
                        id:                 missionRowContent
                        anchors.left:       parent.left
                        anchors.right:      parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins:    _root._margins
                        spacing:            _root._toolsMargin

                        QGCLabel {
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 3
                            text:                  missionRow.object.sequenceNumber
                            color:                 missionRow.current ? workspacePalette.accent : workspacePalette.textSecondary
                        }

                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               missionRow.object.commandName
                            wrapMode:           Text.WordWrap
                            font.weight:        missionRow.current ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth:   true
                visible:            _root._missionReady && !_root._hasMission
                text:               qsTr("No mission loaded")
                wrapMode:           Text.WordWrap
            }

            QGCButton {
                Layout.fillWidth:   true
                visible:            _root._missionReady && !_root._hasMission
                text:               qsTr("Plan mission")
                onClicked:          mainWindow.navigateWorkspace("plan")
            }

        }
    }

    Rectangle {
        id:                 rightDockBackground
        objectName:         "flyViewRightDock"
        anchors.top:        parent.top
        anchors.right:      parent.right
        width:              _root._desktopDockWidth
        height:             Math.min(parent.height, _root._desktopDockHeight)
        color:              workspacePalette.window
        visible:            _root._desktopLayout

        DeadMouseArea { anchors.fill: parent }
    }

    GuidedActionTakeoff { id: dockTakeoffAction; objectName: "flyDock_takeoffAction" }
    GuidedActionLand { id: dockLandAction; objectName: "flyDock_landAction" }
    GuidedActionRTL { id: dockRTLAction }
    GuidedActionPause { id: dockPauseAction }

    ColumnLayout {
        objectName:             "flyViewFlightActions"
        anchors.right:          parent.right
        anchors.rightMargin:    _toolsMargin
        anchors.top:            bottomRightRowLayout.bottom
        anchors.topMargin:      ScreenTools.defaultFontPixelHeight
        width:                  rightDockBackground.width - _toolsMargin * 2
        visible:                _root._desktopLayout
        spacing:                _toolsMargin

        QGCLabel {
            id:                 actionHeader
            Layout.fillWidth:   true
            text:               qsTr("Flight actions")
            font.weight:        Font.DemiBold
        }

        GridLayout {
            id:               actionGrid
            Layout.fillWidth: true
            columns:          2
            columnSpacing:    _root._toolsMargin
            rowSpacing:       _root._toolsMargin

            Repeater {
                model: [
                    { name: "takeoff", action: dockTakeoffAction },
                    { name: "land", action: dockLandAction },
                    { name: "rtl", action: dockRTLAction },
                    { name: "pause", action: dockPauseAction }
                ]

                QGCButton {
                    id: dockActionButton
                    required property var modelData

                    objectName:         "flyDock_" + modelData.name
                    Layout.fillWidth:   true
                    text:               modelData.action.text
                    visible:            modelData.action.visible
                    enabled:            modelData.action.enabled
                    onClicked: {
                        if (mainWindow.allowViewSwitch()) {
                            modelData.action.triggered(dockActionButton)
                        }
                    }
                }
            }
        }
    }

    FlyViewTopRightPanel {
        id:                     topRightPanel
        anchors.top:            parent.top
        anchors.right:          parent.right
        maximumHeight:          parent.height - (bottomRightRowLayout.height + _margins * 4)
    }

    FlyViewTopRightColumnLayout {
        id:                 topRightColumnLayout
        anchors.top:        parent.top
        anchors.right:      parent.right
        anchors.topMargin:  _root._desktopLayout ? _toolsMargin : 0
        anchors.rightMargin: _root._desktopLayout ? _toolsMargin : 0
        spacing:            _layoutSpacing
        visible:           !topRightPanel.visible
    }

    FlyViewBottomRightRowLayout {
        id:                 bottomRightRowLayout
        objectName:         "flyViewBottomRightRowLayout"
        anchors.topMargin:  _root._desktopLayout ? _toolsMargin : 0
        anchors.bottom:     parent.bottom
        anchors.right:      parent.right
        anchors.rightMargin: _root._desktopLayout ? _toolsMargin : 0
        anchors.bottomMargin: _root._desktopLayout ? _toolsMargin : 0
        stacked:            _root._desktopLayout
        rowSpacing:         _layoutSpacing
        columnSpacing:      _layoutSpacing

        states: State {
            name: "desktop"
            when: _root._desktopLayout

            // Switch anchors together so a transient double anchor cannot stretch the grid.
            AnchorChanges {
                target:         bottomRightRowLayout
                anchors.top:    topRightColumnLayout.bottom
                anchors.bottom: undefined
            }
        }
    }

    FlyViewMissionCompleteDialog {
        missionController:      _missionController
        geoFenceController:     _geoFenceController
        rallyPointController:   _rallyPointController
    }

    // Prevent the map's PinchHandler from stealing touch grabs from the joystick pads (issue #13450)
    Binding {
        target:   mapControl
        property: "pinchZoomDisabledByVirtualJoysticks"
        value:    virtualJoystickMultiTouch.visible && virtualJoystickMultiTouch.item && virtualJoystickMultiTouch.item.stickActive
    }

    //-- Virtual Joystick
    Loader {
        id:                         virtualJoystickMultiTouch
        z:                          QGroundControl.zOrderTopMost + 1
        anchors.right:              parent.right
        anchors.rightMargin:        anchors.leftMargin
        height:                     Math.min(parent.height * 0.25, ScreenTools.defaultFontPixelWidth * 16)
        visible:                    _virtualJoystickEnabled && !QGroundControl.videoManager.fullScreen && !(_activeVehicle ? _activeVehicle.usingHighLatencyLink : false)
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       bottomLoaderMargin
        anchors.left:               parent.left
        anchors.leftMargin:         toolStrip.visible
                                    ? (y > toolStrip.y + toolStrip.height ? toolStrip.width / 2 : toolStrip.width * 1.05 + toolStrip.x)
                                    : _root._toolsMargin
        source:                     "qrc:/qml/QGroundControl/FlyView/VirtualJoystick.qml"
        active:                     _virtualJoystickEnabled && !(_activeVehicle ? _activeVehicle.usingHighLatencyLink : false)

        property bool autoCenterThrottle:      QGroundControl.settingsManager.appSettings.virtualJoystickAutoCenterThrottle.rawValue
        property bool leftHandedMode:          QGroundControl.settingsManager.appSettings.virtualJoystickLeftHandedMode.rawValue
        property bool _virtualJoystickEnabled: QGroundControl.settingsManager.appSettings.virtualJoystick.rawValue
        property var  _pipViewMargin:          pipViewRect.height > 0 ? _root.height - pipViewRect.y + ScreenTools.defaultFontPixelHeight * 2 :
                                               bottomRightRowLayout.height + ScreenTools.defaultFontPixelHeight * 1.5

        property var  bottomLoaderMargin:      _pipViewMargin >= parent.height / 2 ? parent.height / 2 : _pipViewMargin

        property real rootWidth:            _root.width
        property var  itemX:                virtualJoystickMultiTouch.x   // real X on screen

        onRootWidthChanged: virtualJoystickMultiTouch.status == Loader.Ready && visible ? virtualJoystickMultiTouch.item.uiTotalWidth = rootWidth : undefined
        onItemXChanged:     virtualJoystickMultiTouch.status == Loader.Ready && visible ? virtualJoystickMultiTouch.item.uiRealX = itemX : undefined

        //Loader status logic
        onLoaded: {
            if (virtualJoystickMultiTouch.visible) {
                virtualJoystickMultiTouch.item.calibration = true
                virtualJoystickMultiTouch.item.uiTotalWidth = rootWidth
                virtualJoystickMultiTouch.item.uiRealX = itemX
            } else {
                virtualJoystickMultiTouch.item.calibration = false
            }
        }
    }

    FlyViewToolStrip {
        id:                     toolStrip
        anchors.left:           parent.left
        anchors.top:            parent.top
        z:                      QGroundControl.zOrderWidgets
        maxHeight:              pipViewRect.y - y - _toolsMargin
        visible:                !QGroundControl.videoManager.fullScreen && hasVisibleActions
        showFlightActions:      !_root._desktopLayout
    }

    VehicleWarnings {
        anchors.centerIn:   parent
        z:                  QGroundControl.zOrderTopMost
    }

    Row {
        id:                 mapScaleRow
        anchors.left:       parent.left
        anchors.leftMargin: (_root._desktopLayout ? missionPanelControl.x + missionPanelControl.width
                                                  : _root._toolStripRight) + _toolsMargin
        anchors.top:        parent.top
        spacing:            _toolsMargin

        // MapScale auto-hides by fading opacity, not visibility
        property bool coversMap: geoMapControls.visible || (mapScale.visible && mapScale.opacity > 0)

        // Google Earth-style camera controls, GeoMap engine only
        GeoMapControls {
            id:      geoMapControls
            geoMap:  mapControl && mapControl.geoMap ? mapControl.geoMap : null
            visible: !!geoMap && !ScreenTools.isTinyScreen && QGCViewer3DManager.displayMode !== QGCViewer3DManager.View3D && mapControl.pipState.state === mapControl.pipState.fullState
        }

        MapScale {
            id:                     mapScale
            anchors.verticalCenter: parent.verticalCenter
            mapControl:             _mapControl
            autoHide:               true
            // geoMap check: scale is ill-defined under the GeoMap engine's 3D
            // tilt; isTopDown keeps it hidden until the 3D->2D tilt animation lands
            visible:                !ScreenTools.isTinyScreen && QGroundControl.corePlugin.options.flyView.showMapScale && QGCViewer3DManager.displayMode !== QGCViewer3DManager.View3D && mapControl && mapControl.pipState.state === mapControl.pipState.fullState &&
                                    (!mapControl.geoMap || (mapControl.geoMap.camera.mode === GeoMapCamera.Mode2D && mapControl.geoMap.camera.isTopDown))
        }
    }

    Viewer3DScaleBar {
        objectName:         "viewer3DScaleBar"
        anchors.left:       parent.left
        anchors.leftMargin: (_root._desktopLayout ? missionPanelControl.x + missionPanelControl.width
                                                  : _root._toolStripRight) + _toolsMargin
        anchors.top:        parent.top
        controller:         _root.viewer3DCameraController
        autoHide:           true
        visible:            !ScreenTools.isTinyScreen && QGroundControl.corePlugin.options.flyView.showMapScale && QGCViewer3DManager.displayMode === QGCViewer3DManager.View3D && !!_root.viewer3DCameraController
    }
}
