cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(AILevelDesigner VERSION 1.0.0)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Zmienna srodowiskowa GEODE_SDK nie jest ustawiona. "
        "Zainstaluj Geode CLI / SDK: https://docs.geode-sdk.org/getting-started/")
else()
    message(STATUS "Znaleziono Geode SDK: $ENV{GEODE_SDK}")
endif()

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/AIPromptPopup.cpp
)

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
