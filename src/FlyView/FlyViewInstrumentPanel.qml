import QtQuick

import QGroundControl
import QGroundControl.Controls

SelectableControl {
    id:                     control
    z:                      QGroundControl.zOrderWidgets
    selectionUIRightAnchor: true
    selectedControl:        QGroundControl.settingsManager.flyViewSettings.instrumentQmlFile2

    property var  missionController:    _missionController
    property real extraInset:           innerControl.extraInset
    property real extraValuesWidth:     innerControl.extraValuesWidth

    readonly property var _vehicle: QGroundControl.multiVehicleManager.activeVehicle
    readonly property bool _connected: _vehicle && !_vehicle.isOfflineEditingVehicle
                                        && !_vehicle.vehicleLinkManager.communicationLost
    readonly property bool _telemetryAvailable: _connected && _vehicle.telemetryAvailable

    QGCPalette { id: instrumentPalette }

    Rectangle {
        anchors.fill: control.contentItem
        color:        instrumentPalette.window
        radius:       ScreenTools.defaultBorderRadius
        visible:      !control._telemetryAvailable

        QGCLabel {
            objectName:             "flyInstrumentUnavailable"
            anchors.fill:           parent
            anchors.margins:        ScreenTools.defaultFontPixelWidth / 2
            text:                   !control._vehicle || control._vehicle.isOfflineEditingVehicle ? qsTr("No vehicle connected")
                                    : !control._connected ? qsTr("Vehicle connection lost")
                                    : qsTr("Waiting for telemetry")
            color:                  instrumentPalette.textSecondary
            wrapMode:               Text.WordWrap
            horizontalAlignment:    Text.AlignHCenter
            verticalAlignment:      Text.AlignVCenter
        }
    }
}
