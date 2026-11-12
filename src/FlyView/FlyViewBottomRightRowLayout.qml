import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

GridLayout {
    id: root

    property bool stacked: false
    readonly property real stackedWidth: Math.max(telemetryValues.implicitWidth,
                                                  instrumentPanel.visible ? instrumentPanel.implicitWidth : 0)
    readonly property real stackedHeight: telemetryValues.implicitHeight
                                          + (instrumentPanel.visible ? instrumentPanel.implicitHeight + rowSpacing : 0)

    columns: stacked ? 1 : 2
    TelemetryValuesBar {
        id:                     telemetryValues
        objectName:             "flyViewTelemetryValues"
        Layout.alignment:       root.stacked ? Qt.AlignHCenter : Qt.AlignBottom
        extraWidth:             root.stacked ? 0 : instrumentPanel.extraValuesWidth
        settingsGroup:          factValueGrid.telemetryBarSettingsGroup
        specificVehicleForCard: null // Tracks active vehicle
    }

    FlyViewInstrumentPanel {
        id:                 instrumentPanel
        objectName:         "flyViewInstrumentPanel"
        Layout.alignment:   root.stacked ? Qt.AlignHCenter : Qt.AlignBottom
        visible:            QGroundControl.corePlugin.options.flyView.showInstrumentPanel && _showSingleVehicleUI
    }
}
