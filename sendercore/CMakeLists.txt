add_library(lm_sendercore STATIC
    iprange.h iprange.cpp
    rowmodel.h rowmodel.cpp
    senderengine.h senderengine.cpp)
target_include_directories(lm_sendercore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lm_sendercore PUBLIC lm_common Qt6::Gui)
