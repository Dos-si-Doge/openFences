cmake_minimum_required(VERSION 3.25)

project(openFences VERSION 1.0.3 LANGUAGES CXX RC)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "openFences targets 64-bit Windows only. Use an x64 toolchain (vcvars64).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: the installed app has no VC++ redistributable dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(OPENFENCES_BUILD_TESTS "Build unit tests" ON)
option(OPENFENCES_BUILD_APP "Build the application" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

configure_file(src/Version.h.in ${CMAKE_BINARY_DIR}/generated/Version.h @ONLY)

set(OF_COMMON_DEFS UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00 NTDDI_VERSION=0x0A000006)
# /guard:cf: Control Flow Guard checks on indirect calls (completed by the linker option on the exe below).
set(OF_COMMON_OPTS /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus /MP /wd4100 /guard:cf)

# ---- core: model, config, rules and pure layout math (unit tested) ----
add_library(ofcore STATIC
    src/core/Strings.cpp
    src/core/Model.cpp
    src/core/Config.cpp
    src/core/Rules.cpp
    src/core/Geometry.cpp
    src/core/MonitorConfig.cpp
    src/core/Snapshots.cpp
    src/core/Autostart.cpp
    src/core/Policy.cpp
    src/core/Paths.cpp
    src/core/Log.cpp
    src/core/Hotkey.cpp
    src/core/Templates.cpp
    src/core/Welcome.cpp
)
target_include_directories(ofcore PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(ofcore PUBLIC ${OF_COMMON_DEFS})
target_compile_options(ofcore PRIVATE ${OF_COMMON_OPTS})
target_link_libraries(ofcore PUBLIC nlohmann_json::nlohmann_json shell32 advapi32 ole32 shlwapi)

# ---- the application ----
if(OPENFENCES_BUILD_APP)
add_executable(openFences WIN32
    src/app/main.cpp
    src/app/Controller.cpp
    src/app/Commands.cpp
    src/app/Tray.cpp
    src/app/Watchdog.cpp
    src/app/CrashHandler.cpp
    src/app/Ipc.cpp
    src/app/Worker.cpp
    src/desktop/DesktopHost.cpp
    src/desktop/DisplayManager.cpp
    src/desktop/InputHooks.cpp
    src/shell/ShellItem.cpp
    src/shell/Enumerator.cpp
    src/shell/IconCache.cpp
    src/shell/ContextMenu.cpp
    src/shell/DragDrop.cpp
    src/shell/ChangeWatcher.cpp
    src/shell/FileOps.cpp
    src/shell/Wallpaper.cpp
    src/render/RenderDevice.cpp
    src/render/Surface.cpp
    src/render/Painter.cpp
    src/ui/WindowBase.cpp
    src/ui/ItemView.cpp
    src/ui/FenceWindow.cpp
    src/ui/DesktopCanvas.cpp
    src/ui/InlineEditor.cpp
    src/ui/settings/Form.cpp
    src/ui/settings/SettingsWindow.cpp
    src/ui/settings/SettingsPages.cpp
    src/ui/settings/RuleDialog.cpp
    src/ui/welcome/WelcomeWindow.cpp
    src/ui/Help.cpp
    res/openFences.rc
    res/app.manifest
)
target_include_directories(openFences PRIVATE src res)
target_compile_options(openFences PRIVATE ${OF_COMMON_OPTS})
target_link_libraries(openFences PRIVATE ofcore
    user32 gdi32 comctl32 comdlg32 uxtheme dwmapi shcore dbghelp propsys windowscodecs
    d3d11 dxgi d2d1 dwrite dcomp)
# Hardening (ASLR, high-entropy ASLR and DEP are MSVC defaults):
#  /guard:cf                   Control Flow Guard.
#  /DEPENDENTLOADFLAG:0x800    resolve the exe's own DLL imports from System32 only (LOAD_LIBRARY_SEARCH_SYSTEM32), so a
#                              DLL dropped next to the portable exe (e.g. in Downloads) can't be side-loaded. All of
#                              them are system DLLs. Shell extensions that context menus load are unaffected.
target_link_options(openFences PRIVATE /guard:cf /DEPENDENTLOADFLAG:0x800)

# Ship templates and the user guide next to the exe (the installer and portable zip pick them up from here).
# A separate always-run target, so documentation changes are copied even when the exe doesn't relink.
add_custom_target(openFencesAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res/templates $<TARGET_FILE_DIR:openFences>/templates
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/docs/user-guide/openFences-User-Guide.html
            $<TARGET_FILE_DIR:openFences>/openFences-User-Guide.html
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/LICENSE $<TARGET_FILE_DIR:openFences>/LICENSE.txt
    COMMENT "Copying templates, the user guide and the license")
add_dependencies(openFencesAssets openFences)
endif()

if(OPENFENCES_BUILD_TESTS)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(ofcore_tests
        tests/StringsTests.cpp
        tests/RulesTests.cpp
        tests/ConfigTests.cpp
        tests/GeometryTests.cpp
        tests/MonitorConfigTests.cpp
        tests/SnapshotsTests.cpp
        tests/AutostartTests.cpp
        tests/HotkeyTests.cpp
        tests/WelcomeTests.cpp
    )
    # /wd6326: under /analyze, GoogleTest's EXPECT_* macros trip "comparison of a constant with another constant".
    target_compile_options(ofcore_tests PRIVATE ${OF_COMMON_OPTS} /wd6326)
    target_link_libraries(ofcore_tests PRIVATE ofcore GTest::gtest_main)
    target_compile_definitions(ofcore_tests PRIVATE OF_TEMPLATES_DIR=L"${CMAKE_SOURCE_DIR}/res/templates" OF_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
    include(GoogleTest)
    gtest_discover_tests(ofcore_tests DISCOVERY_MODE PRE_TEST)
endif()
