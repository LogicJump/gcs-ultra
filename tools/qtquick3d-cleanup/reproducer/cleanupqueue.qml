import QtQuick
import QtQuick3D

Item {
    property bool loadView: true
    readonly property bool viewLoaded: viewLoader.item !== null

    height: 200
    width: 200

    Loader {
        id: viewLoader

        anchors.fill: parent
        sourceComponent: loadView ? viewComponent : null
    }

    Component {
        id: viewComponent

        View3D {
            anchors.fill: parent

            PerspectiveCamera {
                z: 300
            }

            Model {
                source: "#Cube"

                materials: PrincipledMaterial {
                    baseColor: "red"
                }
            }
        }
    }
}
