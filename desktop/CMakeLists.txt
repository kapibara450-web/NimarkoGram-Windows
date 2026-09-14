cmake_minimum_required(VERSION 3.24)

project(NimarkoGramDesktop
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(NIMARKOGRAM_FETCH_TDLIB "Fetch and build the pinned TDLib revision" OFF)
set(NIMARKOGRAM_TDLIB_SOURCE_DIR "" CACHE PATH "Existing TDLib source directory")

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
# TDLib is not a Qt project. Disable Qt automatic generators while TDLib is added.
set(_NIMARKOGRAM_AUTOMOC ${CMAKE_AUTOMOC})
set(_NIMARKOGRAM_AUTOUIC ${CMAKE_AUTOUIC})
set(_NIMARKOGRAM_AUTORCC ${CMAKE_AUTORCC})
set(_NIMARKOGRAM_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_CXX_STANDARD 17)

if (NIMARKOGRAM_TDLIB_SOURCE_DIR)


    set(TD_ENABLE_JNI OFF CACHE BOOL "" FORCE)
    set(TD_ENABLE_DOTNET OFF CACHE BOOL "" FORCE)
    set(TD_ENABLE_LTO OFF CACHE BOOL "" FORCE)
    add_subdirectory("${NIMARKOGRAM_TDLIB_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tdlib")
elseif (NIMARKOGRAM_FETCH_TDLIB)
    include(FetchContent)
    set(TD_ENABLE_JNI OFF CACHE BOOL "" FORCE)
    set(TD_ENABLE_DOTNET OFF CACHE BOOL "" FORCE)
    set(TD_ENABLE_LTO OFF CACHE BOOL "" FORCE)
    set(TD_INSTALL_STATIC_LIBRARIES OFF CACHE BOOL "" FORCE)
    set(TD_INSTALL_SHARED_LIBRARIES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        tdlib
        GIT_REPOSITORY https://github.com/tdlib/td.git
        GIT_TAG d1085f9cebc5a62379991ae1652673954f229c1f
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(tdlib)
endif()
set(CMAKE_AUTOMOC ${_NIMARKOGRAM_AUTOMOC})
set(CMAKE_AUTOUIC ${_NIMARKOGRAM_AUTOUIC})
set(CMAKE_AUTORCC ${_NIMARKOGRAM_AUTORCC})
set(CMAKE_CXX_STANDARD ${_NIMARKOGRAM_CXX_STANDARD})
unset(_NIMARKOGRAM_CXX_STANDARD)
unset(_NIMARKOGRAM_AUTOMOC)
unset(_NIMARKOGRAM_AUTOUIC)
unset(_NIMARKOGRAM_AUTORCC)
set(NIMARKOGRAM_HAS_TDLIB 0)
if (TARGET Td::TdJsonStatic)
    set(NIMARKOGRAM_HAS_TDLIB 1)
endif()

qt_add_executable(NimarkoGramDesktop
    src/main.cpp
    src/core/telegram_client.cpp
    src/core/telegram_client.h
)

target_include_directories(NimarkoGramDesktop PRIVATE src)

target_link_libraries(NimarkoGramDesktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if (NIMARKOGRAM_HAS_TDLIB)
    target_link_libraries(NimarkoGramDesktop PRIVATE Td::TdJsonStatic)
endif()

target_compile_definitions(NimarkoGramDesktop PRIVATE
    NIMARKOGRAM_DESKTOP_VERSION=\"${PROJECT_VERSION}\"
    NIMARKOGRAM_HAS_TDLIB=${NIMARKOGRAM_HAS_TDLIB}
)

if (WIN32)
    set_target_properties(NimarkoGramDesktop PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME NimarkoGram
    )
endif()

install(TARGETS NimarkoGramDesktop
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

set(CPACK_PACKAGE_NAME "NimarkoGram")
set(CPACK_PACKAGE_VENDOR "NimarkoGram Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Windows client for NimarkoGram")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NimarkoGram")
set(CPACK_GENERATOR "ZIP;NSIS")
include(CPack)
