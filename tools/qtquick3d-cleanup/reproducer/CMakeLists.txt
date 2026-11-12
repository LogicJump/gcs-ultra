cmake_minimum_required(VERSION 3.16)

project(tst_quick3dcleanupqueue LANGUAGES CXX)

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick Test)

qt_standard_project_setup()

qt_add_executable(tst_quick3dcleanupqueue tst_quick3dcleanupqueue.cpp)

qt_add_resources(
    tst_quick3dcleanupqueue "cleanupqueue"
    PREFIX "/"
    FILES
        cleanupqueue.qml
)

target_link_libraries(
    tst_quick3dcleanupqueue
    PRIVATE
        Qt6::Quick
        Qt6::Test
)
