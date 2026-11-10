add_executable(LanMessengerSender WIN32
    main.cpp
    mainwindow.h mainwindow.cpp)
target_link_libraries(LanMessengerSender PRIVATE lm_sendercore Qt6::Widgets)
if(MINGW)
    target_link_options(LanMessengerSender PRIVATE -static-libgcc -static-libstdc++)
endif()
