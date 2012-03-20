# Source
set(appmenu_qt_SRCS
    appmenuplatformmenubar.cpp
    )

# Build
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${DBUSMENUQT_INCLUDE_DIRS}
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    ${QT_QTXML_INCLUDE_DIR}
    )

qt4_automoc(${appmenu_qt_SRCS})

link_directories(
    ${DBUSMENUQT_LIBRARY_DIRS}
    )

add_library(appmenu-qt MODULE
    ${appmenu_qt_SRCS}
    )

target_link_libraries(appmenu-qt
    ${DBUSMENUQT_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTXML_LIBRARIES}
    )

# Install
install(TARGETS appmenu-qt
    DESTINATION ${QT_PLUGINS_DIR}/menubar/
    )
