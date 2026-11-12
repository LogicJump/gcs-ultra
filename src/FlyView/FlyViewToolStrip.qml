import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

ToolStrip {
    id: _root

    property bool showFlightActions: true
    readonly property bool hasVisibleActions: {
        const actions = flyViewToolStripActionList.model
        for (let index = 0; index < actions.length; index++) {
            if (actions[index].visible) {
                return true
            }
        }
        return false
    }

    FlyViewToolStripActionList {
        id: flyViewToolStripActionList
        showFlightActions: _root.showFlightActions
    }

    model: flyViewToolStripActionList.model
}
