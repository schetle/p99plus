cmake_minimum_required(VERSION 3.16)
project(p99plus C CXX)

# Build with:  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake && cmake --build build
# The EQ Titanium client is 32-bit, so this MUST be an i686 build.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(TP ${CMAKE_SOURCE_DIR}/third_party)

# Version comes from the VERSION file (one line, e.g. 0.2.0); editing it re-runs configure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/VERSION)
file(READ ${CMAKE_SOURCE_DIR}/VERSION APP_VERSION)
string(STRIP "${APP_VERSION}" APP_VERSION)

# --- MinHook -----------------------------------------------------------------
add_library(minhook STATIC
  ${TP}/minhook/src/buffer.c
  ${TP}/minhook/src/hook.c
  ${TP}/minhook/src/trampoline.c
  ${TP}/minhook/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC ${TP}/minhook/include)

# --- Dear ImGui (DX9 + Win32 backends) ----------------------------------------
add_library(imgui STATIC
  ${TP}/imgui/imgui.cpp
  ${TP}/imgui/imgui_draw.cpp
  ${TP}/imgui/imgui_tables.cpp
  ${TP}/imgui/imgui_widgets.cpp
  ${TP}/imgui/backends/imgui_impl_dx9.cpp
  ${TP}/imgui/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC ${TP}/imgui ${TP}/imgui/backends)

# --- The two DLLs ----------------------------------------------------------------
# dinput8.dll (src/host/) is the proxy the game loads; it stays loaded and loads the core.
# <name>_core.dll (everything else under src/) holds the features and can be reloaded while the
# game runs (see src/shared/core_api.h). Every .cpp is picked up, so a new feature folder needs
# no edit here.
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HOST_ONLY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/host/*.cpp)
set(CORE_SOURCES ${ALL_SOURCES})
list(REMOVE_ITEM CORE_SOURCES ${HOST_ONLY_SOURCES})
set(HOST_SOURCES ${HOST_ONLY_SOURCES} ${CMAKE_SOURCE_DIR}/src/util/hook.cpp)

function(app_dll target)
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
  set_target_properties(${target} PROPERTIES PREFIX "")
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX DIRECTINPUT_VERSION=0x0800
    APP_VERSION="${APP_VERSION}")
  target_compile_options(${target} PRIVATE -Wall -Wno-unknown-pragmas)
  # Static runtime so no libgcc/libstdc++ DLLs are needed in the EQ folder.
  target_link_options(${target} PRIVATE -static -static-libgcc -static-libstdc++)
endfunction()

add_library(${PROJECT_NAME}_host SHARED ${HOST_SOURCES})
app_dll(${PROJECT_NAME}_host)
set_target_properties(${PROJECT_NAME}_host PROPERTIES OUTPUT_NAME "dinput8")
target_link_libraries(${PROJECT_NAME}_host PRIVATE minhook dxguid user32 gdi32)
# --kill-at strips the @20 stdcall suffix so the export is plain "DirectInput8Create".
target_link_options(${PROJECT_NAME}_host PRIVATE -Wl,--kill-at)

add_library(${PROJECT_NAME}_core SHARED ${CORE_SOURCES})
app_dll(${PROJECT_NAME}_core)
set_target_properties(${PROJECT_NAME}_core PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_core")
target_link_libraries(${PROJECT_NAME}_core PRIVATE imgui minhook dxguid winmm dwmapi imm32 gdi32 user32)
