cmake_minimum_required(VERSION 3.15)

project(RollLockGhost)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED BAKKESMOD_PLUGIN_SDK_DIR)
    if(WIN32)
        set(BAKKESMOD_PLUGIN_SDK_DIR "C:/bakkesmodsdk")
    else()
        set(BAKKESMOD_PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bakkesmodsdk")
    endif()
endif()

if (NOT EXISTS ${BAKKESMOD_PLUGIN_SDK_DIR})
    message(FATAL_ERROR "BakkesMod SDK not found at ${BAKKESMOD_PLUGIN_SDK_DIR}")
endif()

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(VERSION_BUILD 0)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

add_library(RollLockGhost SHARED
    RollLockGhost.cpp
    RollLockGhost.h
    pch.h
    pch.cpp
)

target_include_directories(RollLockGhost PRIVATE
    ${BAKKESMOD_PLUGIN_SDK_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_precompile_headers(RollLockGhost PRIVATE pch.h)

target_link_directories(RollLockGhost PRIVATE ${BAKKESMOD_PLUGIN_SDK_DIR}/lib)

if(WIN32)
    target_link_libraries(RollLockGhost PRIVATE
        pluginsdk
    )
    
    set_target_properties(RollLockGhost PROPERTIES
        OUTPUT_NAME "RollLockGhost"
        SUFFIX ".dll"
    )
endif()
