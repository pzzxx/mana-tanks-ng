cmake_minimum_required(VERSION 3.21)

project(ManaTanks VERSION 1.0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(COMMONLIBSSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
if(NOT EXISTS "${COMMONLIBSSE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "CommonLibSSE-NG not found at '${COMMONLIBSSE_DIR}'")
endif()

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("${COMMONLIBSSE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build")
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)
include("${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake")

find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    NAME "Mana Tanks NG"
    AUTHOR "Devin"
    USE_ADDRESS_LIBRARY
    SOURCES ${sources}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

set(MO2_MOD_FOLDER "E:/MO2Mods/Mana Tanks NG" CACHE PATH "MO2 mod folder to deploy into")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MO2_MOD_FOLDER}/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${MO2_MOD_FOLDER}/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
)
