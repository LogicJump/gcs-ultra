import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Viewer3D

ToolStripActionList {
    id: _root

    property bool showFlightActions: true

    model: [
        Viewer3DShowAction { },
        GuidedActionTakeoff { visible: _root.showFlightActions && !_guidedController.takeoffLandSlotShowsLand },
        GuidedActionLand { visible: _root.showFlightActions && _guidedController.takeoffLandSlotShowsLand },
        GuidedActionRTL { visible: _root.showFlightActions && (!_activeVehicle || _activeVehicle.rtlFlightMode !== "") },
        GuidedActionPause { visible: _root.showFlightActions && _guidedController.showPause },
        FlyViewAdditionalActionsButton { },
        FlyViewGripperButton { }
    ]
}
