qt_add_executable(LanMessengerMobile main.cpp)
set_source_files_properties(Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(LanMessengerMobile
    URI SenderMobile
    VERSION 1.0
    QML_FILES Main.qml)
target_link_libraries(LanMessengerMobile PRIVATE lm_sendercore Qt6::Quick Qt6::QuickControls2)
set_target_properties(LanMessengerMobile PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
if(MINGW)
    target_link_options(LanMessengerMobile PRIVATE -static-libgcc -static-libstdc++)
endif()
