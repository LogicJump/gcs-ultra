import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id:             control
    implicitWidth:  mainLayout.width + (_toolsMargin * 2)
    implicitHeight: mainLayout.height + (_toolsMargin * 2)

    property real extraWidth: 0 ///< Extra width to add to the background rectangle

    property alias factValueGrid:           factValueGrid
    property alias settingsGroup:           factValueGrid.settingsGroup
    property alias specificVehicleForCard:  factValueGrid.specificVehicleForCard

    readonly property var _telemetryVehicle: specificVehicleForCard || QGroundControl.multiVehicleManager.activeVehicle
    readonly property bool _connected: _telemetryVehicle && !_telemetryVehicle.isOfflineEditingVehicle
                                        && !_telemetryVehicle.vehicleLinkManager.communicationLost
    readonly property bool _telemetryAvailable: _connected && _telemetryVehicle.telemetryAvailable

    Rectangle {
        id:         backgroundRect
        width:      control.width + extraWidth
        height:     control.height
        color:      qgcPal.window
        radius:     ScreenTools.defaultBorderRadius
        opacity:    0.75
    }

    ColumnLayout {
        id:                 mainLayout
        anchors.margins:    _toolsMargin
        anchors.bottom:     parent.bottom
        anchors.left:       parent.left

        RowLayout {
            visible: factValueGrid.settingsUnlocked

            QGCColoredImage {
                source:             "qrc:/InstrumentValueIcons/lock-open.svg"
                mipmap:             true
                width:              ScreenTools.minTouchPixels * 0.75
                height:             width
                sourceSize.width:   width
                color:              qgcPal.textPrimary
                fillMode:           Image.PreserveAspectFit

                QGCMouseArea {
                    anchors.fill: parent
                    onClicked:    factValueGrid.settingsUnlocked = false
                }
            }
        }

        HorizontalFactValueGrid {
            id: factValueGrid
        }
    }

    Rectangle {
        anchors.fill: mainLayout
        color:        qgcPal.window
        visible:      !control._telemetryAvailable && !factValueGrid.settingsUnlocked

        QGCLabel {
            objectName:             "flyTelemetryUnavailable"
            anchors.fill:           parent
            text:                   !control._telemetryVehicle || control._telemetryVehicle.isOfflineEditingVehicle ? qsTr("No vehicle connected")
                                    : !control._connected ? qsTr("Vehicle connection lost")
                                    : qsTr("Waiting for telemetry")
            color:                  qgcPal.textSecondary
            wrapMode:               Text.WordWrap
            horizontalAlignment:    Text.AlignHCenter
            verticalAlignment:      Text.AlignVCenter
        }
    }

    QGCMouseArea {
        id:                         mouseArea
        x:                          mainLayout.x
        y:                          mainLayout.y
        width:                      mainLayout.width
        height:                     mainLayout.height
        acceptedButtons:            Qt.LeftButton | Qt.RightButton
        propagateComposedEvents:    true
        visible:                    !factValueGrid.settingsUnlocked

        onClicked: (mouse) => {
            if (!ScreenTools.isMobile && mouse.button === Qt.RightButton) {
                factValueGrid.settingsUnlocked = true
                mouse.accepted = true
            }
        }

        onPressAndHold: (mouse) => {
            factValueGrid.settingsUnlocked = true
            mouse.accepted = true
        }
    }
}
