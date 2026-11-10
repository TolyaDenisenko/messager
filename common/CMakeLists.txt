add_library(lm_common STATIC
    protocol.h protocol.cpp
    lineio.h lineio.cpp)
target_include_directories(lm_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lm_common PUBLIC Qt6::Core Qt6::Network)
