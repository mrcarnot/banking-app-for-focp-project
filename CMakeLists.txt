cmake_minimum_required(VERSION 3.10)
project(BankingApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

# Console application (existing)
add_executable(BankingApp src/main.cpp)

# GUI application (Win32)
if(WIN32)
    add_executable(BankingAppGUI WIN32 src/main_gui.cpp)
    target_link_libraries(BankingAppGUI gdi32 user32)
endif()
