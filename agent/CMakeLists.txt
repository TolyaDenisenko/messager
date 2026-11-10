add_executable(LanMessengerAgent
    main.cpp
    agentcore.h agentcore.cpp
    messagestore.h messagestore.cpp)
target_link_libraries(LanMessengerAgent PRIVATE lm_common advapi32)
if(MINGW)
    target_link_options(LanMessengerAgent PRIVATE -static-libgcc -static-libstdc++)
endif()
