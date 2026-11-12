import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// List of the mission's visual items. Selecting a row makes it the current plan view item.
QGCListView {
    id: root

    required property var missionController

    signal itemActivated

    readonly property bool _emptyMission: !missionController.visualItems || missionController.visualItems.count <= 1

    objectName:     "planPanel_missionList"
    model:          missionController.visualItems
    spacing:        ScreenTools.defaultFontPixelHeight * 0.1
    clip:           true
    currentIndex:   missionController.currentPlanViewVIIndex

    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

    footer: Item {
        objectName: "planPanel_missionEmptyState"
        width: root.width
        height: visible ? emptyColumn.implicitHeight + ScreenTools.defaultFontPixelHeight : 0
        visible: root._emptyMission

        ColumnLayout {
            id: emptyColumn
            x: ScreenTools.defaultFontPixelWidth
            y: ScreenTools.defaultFontPixelHeight * 0.5
            width: parent.width - x * 2
            spacing: ScreenTools.defaultFontPixelHeight * 0.3

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("No mission items")
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                objectName: "planPanel_missionEmptyHint"
                Layout.fillWidth: true
                text: root.missionController.homePositionSet
                      ? qsTr("Choose a Plan template, or select Mission and use Waypoint on the map.")
                      : qsTr("Set Home on the map in Plan to begin.")
                color: QGroundControl.globalPalette.textSecondary
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }

    delegate: MissionItemRow {
        required property var model

        width:  root.width
        object: model.object

        onClicked: {
            if (!mainWindow.allowViewSwitch()) {
                return
            }
            root.missionController.setCurrentPlanViewSeqNum(object.sequenceNumber, false)
            root.itemActivated()
        }
    }
}
