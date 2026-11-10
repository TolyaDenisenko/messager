cmake_minimum_required(VERSION 3.21)
project(LanMessenger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Gui)

# Общие библиотеки
add_subdirectory(common)       # протокол и сетевой ввод-вывод
add_subdirectory(sendercore)   # движок отправителя (ПК + мобильный)

# Windows: агент (служба + UI) и отправитель для ПК
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_subdirectory(agent)    # LanMessengerAgent.exe — Windows-служба
    add_subdirectory(agentui)  # LanMessengerUI.exe — окно сообщения у пользователя
    add_subdirectory(sender)   # LanMessengerSender.exe — отправитель для ПК
    add_subdirectory(tests)    # lm_selftest.exe — самотест полного цикла на localhost
endif()

# Мобильный отправитель (Android/iOS; на ПК собирается для проверки)
option(LM_BUILD_MOBILE "Собрать мобильного отправителя (Qt Quick)" ON)
if(LM_BUILD_MOBILE)
    find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
    if(Qt6Quick_FOUND AND Qt6QuickControls2_FOUND)
        add_subdirectory(sendermobile)
    else()
        message(STATUS "Qt Quick не найден — мобильный отправитель пропущен")
    endif()
endif()
