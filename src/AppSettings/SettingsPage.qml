import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

Item {
    id: root

    default property alias contentItem: mainLayout.data
    property int sectionFilter: -1

    QGCFlickable {
        objectName:     "settingsPageFlickable"
        anchors.fill:   parent
        contentWidth:   width
        contentHeight:  mainLayout.y + mainLayout.height + mainLayout.y
        flickableDirection: Flickable.VerticalFlick

        Item {
            id:         mainLayout
            objectName: "settingsPageLayout"
            x:          Math.min(ScreenTools.defaultFontPixelWidth * 2, root.width / 2)
            y:          ScreenTools.defaultFontPixelHeight
            width:      Math.max(0, root.width - x * 2)

            readonly property real columnSpacing: ScreenTools.defaultFontPixelWidth * 2
            readonly property real rowSpacing: ScreenTools.defaultFontPixelHeight
            readonly property int columns: Math.max(1, Math.min(2, _visibleItemCount,
                Math.floor((width + columnSpacing) / (ScreenTools.defaultFontPixelWidth * 66 + columnSpacing))))
            readonly property int _visibleItemCount: _itemStates.filter(state => state.visible).length
            readonly property var _itemStates: {
                const states = []
                for (const child of children) {
                    states.push({ item: child, visible: child.visible, implicitHeight: child.implicitHeight })
                }
                return states
            }

            function arrangeGroups() {
                const columnWidth = Math.max(0, (width - columnSpacing * (columns - 1)) / columns)
                const columnHeights = [0, 0]
                let visibleIndex = 0
                for (const state of _itemStates) {
                    if (!state.visible) {
                        continue
                    }
                    const column = visibleIndex % columns
                    const item = state.item
                    item.width = columnWidth
                    item.height = item.implicitHeight
                    item.x = column * (columnWidth + columnSpacing)
                    item.y = columnHeights[column]
                    columnHeights[column] += item.height + rowSpacing
                    visibleIndex++
                }
                height = Math.max(0, Math.max(...columnHeights) - rowSpacing)
            }

            // Independent column heights remove row gaps without reparenting groups or changing their focus order.
            on_ItemStatesChanged: Qt.callLater(arrangeGroups)
            onColumnsChanged: Qt.callLater(arrangeGroups)
            onWidthChanged: Qt.callLater(arrangeGroups)
            onColumnSpacingChanged: Qt.callLater(arrangeGroups)
            onRowSpacingChanged: Qt.callLater(arrangeGroups)
            Component.onCompleted: Qt.callLater(arrangeGroups)
        }
    }
}
