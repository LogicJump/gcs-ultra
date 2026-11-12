import QtQuick
import QtQuick.Controls
import QtQuick.Window

import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root
    objectName: "workspaceNavigation"
    implicitHeight: Math.max(ScreenTools.defaultFontPixelHeight * 2.2,
                             ScreenTools.isMobile ? ScreenTools.minTouchPixels : 0)
    color: qgcPal.toolbarBackground

    property string currentWorkspace: "fly"
    property bool showAnalyze: true
    property bool showSettings: true
    property var hostWindow: null
    readonly property bool _showWindowControls: ScreenTools.isWindows && hostWindow
    readonly property real _navigationWidth: width - windowControls.width
    readonly property bool compact: _navigationWidth < qgcLogo.width + workspaceRow.implicitWidth + _margin * 5
    readonly property real _margin: ScreenTools.defaultFontPixelWidth
    readonly property var _workspaces: [
        { key: "fly", label: qsTr("Fly"), icon: "/qmlimages/PaperPlane.svg" },
        { key: "plan", label: qsTr("Plan"), icon: "/qmlimages/Plan.svg" },
        { key: "vehicle", label: qsTr("Vehicle"), icon: "/qmlimages/Gears.svg" },
        { key: "analyze", label: qsTr("Analyze"), icon: "/qmlimages/Analyze.svg" },
        { key: "settings", label: qsTr("Settings"), icon: "/qmlimages/Gears.svg" }
    ]

    signal workspaceRequested(string workspace)
    signal menuRequested()

    function currentWorkspaceLabel() {
        for (let workspace of _workspaces) {
            if (workspace.key === currentWorkspace) {
                return workspace.label
            }
        }
        return qsTr("Workspaces")
    }

    QGCPalette { id: qgcPal }

    function toggleMaximized() {
        if (!hostWindow) {
            return
        }
        if (hostWindow.visibility === Window.Maximized) {
            hostWindow.showNormal()
        } else {
            hostWindow.showMaximized()
        }
    }

    MouseArea {
        id: dragArea
        objectName: "window_dragArea"
        anchors.left: root.compact ? compactMenu.right : workspaceRow.right
        anchors.right: windowControls.left
        height: root.height
        enabled: root._showWindowControls
        onDoubleClicked: root.toggleMaximized()

        DragHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
            onActiveChanged: {
                if (active && root.hostWindow) {
                    root.hostWindow.startSystemMove()
                }
            }
        }
    }

    QGCToolBarButton {
        id: qgcLogo
        objectName: "toolbar_qgcLogo"
        height: root.height
        width: Math.min(implicitWidth, root._navigationWidth * 0.4)
        clip: true
        text: QGroundControl.appName
        font.pointSize: ScreenTools.smallFontPointSize
        icon.source: "/res/QGCLogoFull.svg"
        logo: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: qsTr("%1 workspace menu").arg(QGroundControl.appName)
        onClicked: root.menuRequested()
    }

    Row {
        id: workspaceRow
        anchors.left: qgcLogo.right
        anchors.leftMargin: root._margin
        height: root.height
        opacity: root.compact ? 0 : 1
        enabled: !root.compact

        Repeater {
            model: root._workspaces

            QGCButton {
                id: workspaceButton
                required property var modelData
                objectName: "workspace_" + modelData.key
                height: root.height
                text: modelData.label
                iconSource: modelData.icon
                checked: root.currentWorkspace === modelData.key
                checkable: false
                focusPolicy: Qt.StrongFocus
                Accessible.ignored: root.compact
                leftPadding: root._margin * 1.5
                rightPadding: leftPadding
                fontWeight: checked ? Font.DemiBold : Font.Normal
                visible: (modelData.key !== "analyze" || root.showAnalyze)
                         && (modelData.key !== "settings" || root.showSettings)
                onClicked: root.workspaceRequested(modelData.key)

                background: Rectangle {
                    color: workspaceButton.checked ? qgcPal.accentSubtle
                                                   : workspaceButton.hovered ? qgcPal.windowCard : root.color
                    border.width: workspaceButton.activeFocus ? 1 : 0
                    border.color: qgcPal.accent

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 2
                        color: qgcPal.accent
                        visible: workspaceButton.checked
                    }
                }
            }
        }
    }

    QGCButton {
        id: compactMenu
        objectName: "workspace_compactMenu"
        anchors.left: qgcLogo.right
        anchors.leftMargin: root._margin
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, root._navigationWidth - qgcLogo.width - root._margin * 3
                        - (returnFly.visible ? returnFly.width + root._margin : 0))
        text: root.currentWorkspaceLabel()
        elide: Text.ElideRight
        clip: true
        iconSource: "/qmlimages/HamburgerThin.svg"
        backgroundColor: root.color
        showBorder: false
        focusPolicy: Qt.StrongFocus
        visible: root.compact
        onClicked: root.menuRequested()
    }

    QGCButton {
        id: returnFly
        objectName: "workspace_returnFly"
        anchors.right: windowControls.left
        anchors.rightMargin: root._margin
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Fly")
        width: Math.min(implicitWidth, root._navigationWidth * 0.28)
        elide: Text.ElideRight
        clip: true
        iconSource: "/qmlimages/PaperPlane.svg"
        backgroundColor: root.color
        showBorder: false
        focusPolicy: Qt.StrongFocus
        visible: root.compact && root.currentWorkspace !== "fly"
        onClicked: root.workspaceRequested("fly")
    }

    Row {
        id: windowControls
        anchors.right: parent.right
        height: root.height
        width: visible ? implicitWidth : 0
        visible: root._showWindowControls

        Repeater {
            model: [
                { key: "minimize", label: qsTr("Minimize"), icon: "/qmlimages/WindowMinimize.svg" },
                { key: "maximize", label: root.hostWindow && root.hostWindow.visibility === Window.Maximized
                                          ? qsTr("Restore") : qsTr("Maximize"),
                  icon: root.hostWindow && root.hostWindow.visibility === Window.Maximized
                        ? "/qmlimages/WindowRestore.svg" : "/qmlimages/WindowMaximize.svg" },
                { key: "close", label: qsTr("Close"), icon: "/qmlimages/WindowClose.svg" }
            ]

            QGCButton {
                id: windowButton
                required property var modelData
                objectName: "window_" + modelData.key
                width: root.height * 1.25
                height: root.height
                iconSource: modelData.icon
                leftPadding: root._margin
                rightPadding: leftPadding
                focusPolicy: Qt.StrongFocus
                Accessible.name: modelData.label
                ToolTip.visible: hovered
                ToolTip.text: modelData.label
                ToolTip.delay: 700
                onClicked: {
                    if (!root.hostWindow) {
                        return
                    }
                    switch (modelData.key) {
                    case "minimize": root.hostWindow.showMinimized(); break
                    case "maximize": root.toggleMaximized(); break
                    case "close": root.hostWindow.close(); break
                    }
                }

                background: Rectangle {
                    color: windowButton.hovered || windowButton.pressed
                           ? (windowButton.modelData.key === "close" ? qgcPal.colorRed : qgcPal.windowCard)
                           : root.color
                    border.width: windowButton.activeFocus ? 1 : 0
                    border.color: qgcPal.accent
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: qgcPal.borderSubtle
    }
}
