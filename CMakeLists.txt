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

# --- The proxy: dinput8.dll ---------------------------------------------------
# Every .cpp under src/ is built, so a new feature folder needs no edit here.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_library(p99plus SHARED ${APP_SOURCES})
target_include_directories(p99plus PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(p99plus PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")
target_compile_definitions(p99plus PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX DIRECTINPUT_VERSION=0x0800
  APP_VERSION="${APP_VERSION}")
target_compile_options(p99plus PRIVATE -Wall -Wno-unknown-pragmas)
target_link_libraries(p99plus PRIVATE imgui minhook dxguid winmm dwmapi imm32 gdi32 user32)
# Static runtime so no libgcc/libstdc++ DLLs are needed in the EQ folder;
# --kill-at strips the @20 stdcall suffix so the export is plain "DirectInput8Create".
target_link_options(p99plus PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--kill-at)
