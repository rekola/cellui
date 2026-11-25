cmake_minimum_required(VERSION 3.10)

# Cell UI: what the terminal (and later a window) UI is built from. It builds
# on its own and includes nothing from the application: everything it needs
# is under this directory.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cellui LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
  endif()
  add_compile_options(-Wall -Wextra
    -Werror=return-local-addr -Werror=multichar -Werror=enum-compare -Werror=return-type
    -Werror=conversion-null -Werror=parentheses -Werror=switch -Werror=address -Werror=trigraphs
    -Werror=pointer-arith -Werror=cast-qual -Werror=non-virtual-dtor -Werror=write-strings
    -Werror=sign-conversion -Werror=sign-compare -Werror=sign-promo -Werror=duplicated-cond
    -Werror=duplicated-branches -Werror=logical-op -Werror=redundant-decls -Werror=cast-align
    -Wformat=2 -Werror=format -Werror=shadow -Werror=missing-declarations)
  enable_testing()
  set(CELLUI_STANDALONE ON)
endif()

# Tests, the gallery and the tools: on when built on its own, off when an
# application pulls cellui in (cellui's own CI runs them).
option(CELLUI_BUILD_TESTS "Build cellui's tests, gallery and tools" ${CELLUI_STANDALONE})

# cellui/Utf8.h's grapheme-cluster-aware width and truncation come from
# libunistring or from utf8proc (CELLUI_UNICODE=unistring|utf8proc; auto takes
# libunistring where it is found, else utf8proc). utf8proc is the installed one
# or, where there is none (MSYS2 has no package), a pinned release built here. No pkg-config file ships
# with libunistring-dev on Ubuntu.
set(CELLUI_UNICODE "auto" CACHE STRING "Unicode backend: auto, unistring or utf8proc")
set_property(CACHE CELLUI_UNICODE PROPERTY STRINGS auto unistring utf8proc)
if(NOT CELLUI_UNICODE MATCHES "^(auto|unistring|utf8proc)$")
  message(FATAL_ERROR "CELLUI_UNICODE must be auto, unistring or utf8proc")
endif()
set(CELLUI_USE_UTF8PROC OFF)
if(NOT CELLUI_UNICODE STREQUAL "utf8proc")
  find_library(UNISTRING_LIBRARY unistring)
  find_path(UNISTRING_INCLUDE_DIR unigbrk.h)
endif()
if(CELLUI_UNICODE STREQUAL "utf8proc" OR (CELLUI_UNICODE STREQUAL "auto" AND NOT (UNISTRING_LIBRARY AND UNISTRING_INCLUDE_DIR)))
  find_library(UTF8PROC_LIBRARY utf8proc)
  find_path(UTF8PROC_INCLUDE_DIR utf8proc.h)
  if(NOT (UTF8PROC_LIBRARY AND UTF8PROC_INCLUDE_DIR))
    # Not installed (MSYS2 has no package): build a pinned release, static.
    include(FetchContent)
    FetchContent_Declare(utf8proc
      GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc
      GIT_TAG v2.9.0)
    set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    # Its CMakeLists asks for a minimum CMake that CMake 4 no longer accepts.
    set(CMAKE_POLICY_VERSION_MINIMUM_SAVED "${CMAKE_POLICY_VERSION_MINIMUM}")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_MakeAvailable(utf8proc)
    set(CMAKE_POLICY_VERSION_MINIMUM "${CMAKE_POLICY_VERSION_MINIMUM_SAVED}")
    set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
    set(CELLUI_UTF8PROC_FETCHED ON)
  endif()
  set(CELLUI_USE_UTF8PROC ON)
elseif(NOT (UNISTRING_LIBRARY AND UNISTRING_INCLUDE_DIR))
  message(FATAL_ERROR "libunistring not found (install libunistring-dev, or use -DCELLUI_UNICODE=utf8proc)")
endif()
if(CELLUI_USE_UTF8PROC)
  message(STATUS "cellui Unicode backend: utf8proc")
else()
  message(STATUS "cellui Unicode backend: libunistring")
endif()

# canvas draws the scopes' pixel versions. Its warnings are its own, so it is
# built from a plain checkout outside this project's flag set (an including
# project may give the flags in CELLUI_CANVAS_CXX_FLAGS).
if(NOT TARGET canvas)
  include(FetchContent)
  FetchContent_Declare(canvas
    GIT_REPOSITORY https://github.com/rekola/canvas
    GIT_TAG bd7a85dbee1f9a314a8fdad0e964f837f8d0ba47
    # No CMakeLists there: MakeAvailable only downloads, and the
    # add_subdirectory below applies our flags.
    SOURCE_SUBDIR cellui_no_add_subdirectory)
  FetchContent_MakeAvailable(canvas)
  if(NOT DEFINED CELLUI_CANVAS_CXX_FLAGS)
    set(CELLUI_CANVAS_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
  endif()
  set(CANVAS_CXX_FLAGS_SAVED "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CELLUI_CANVAS_CXX_FLAGS}")
  set(CANVAS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  if(WIN32)
    set(CANVAS_BACKEND_CAIRO OFF CACHE BOOL "" FORCE)
    set(CANVAS_BACKEND_GDIPLUS ON CACHE BOOL "" FORCE)
  endif()
  if(APPLE)
    set(CANVAS_BACKEND_CAIRO OFF CACHE BOOL "" FORCE)
    set(CANVAS_BACKEND_QUARTZ ON CACHE BOOL "" FORCE)
  endif()
  add_subdirectory(${canvas_SOURCE_DIR} ${canvas_BINARY_DIR} EXCLUDE_FROM_ALL)
  set(CMAKE_CXX_FLAGS "${CANVAS_CXX_FLAGS_SAVED}")
endif()

add_library(cellui STATIC
  src/Application.cpp
  src/BackendChoice.cpp
  src/BufferPlane.cpp
  src/CellBuffer.cpp
  src/BlendMode.cpp
  src/Color.cpp
  src/ChoiceDialog.cpp
  src/ChoiceModal.cpp
  src/ScrollBody.cpp
  src/ModalDialog.cpp
  src/CommandPalette.cpp
  src/FuzzyMatch.cpp
  src/KeyStrokeNames.cpp
  src/Gallery.cpp
  src/InfoDialog.cpp
  src/InfoModal.cpp
  src/ChartModal.cpp
  src/FormModal.cpp
  src/InlineEditor.cpp
  src/EscapeCoalescer.cpp
  src/EventLoop.cpp
  src/Wakeup.cpp
  src/ThreadPosts.cpp
  src/LinkStatus.cpp
  src/KeyReader.cpp
  src/LineEditor.cpp
  src/Markdown.cpp
  src/MenuBar.cpp
  src/Table.cpp
  src/TagBar.cpp
  src/Charts.cpp
  src/term/CellCharts.cpp
  src/LineChart.cpp
  src/Scrim.cpp
  src/term/SubcellGlyphs.cpp
  src/Utf8.cpp
  src/term/BackdropEmulation.cpp
  src/term/Discovery.cpp
  src/term/FrameEncoder.cpp
  src/term/FramePacer.cpp
  src/term/Deflate.cpp
  src/term/ImageEncoder.cpp
  src/ImageScale.cpp
  src/term/KittyImages.cpp
  src/term/InputParser.cpp
  src/term/TermEvent.cpp
  src/TestBackend.cpp)
if(NOT WIN32)
  target_sources(cellui PRIVATE src/PollEventLoop.cpp src/term/OutputQueue.cpp src/term/TerminalSession.cpp)
  find_package(Threads REQUIRED)
  target_link_libraries(cellui PUBLIC Threads::Threads)
endif()
# The GTK 4 window backend (cellui/gtk/): CELLUI_ENABLE_GTK, set by the
# including project (SYNTH_ENABLE_GTK) or, standalone, found here.
if(CELLUI_STANDALONE AND NOT DEFINED CELLUI_ENABLE_GTK AND NOT WIN32 AND NOT APPLE)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(CELLUI_GTK4_PROBE QUIET gtk4>=4.10)
  endif()
  if(CELLUI_GTK4_PROBE_FOUND)
    set(CELLUI_ENABLE_GTK ON)
  endif()
endif()
if(CELLUI_ENABLE_GTK)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CELLUI_GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.10)
  target_sources(cellui PRIVATE src/gtk/GlibEventLoop.cpp src/gtk/Window.cpp)
  target_link_libraries(cellui PRIVATE PkgConfig::CELLUI_GTK4)
  target_compile_definitions(cellui PUBLIC CELLUI_HAS_GTK)
  # Needs a display: run under Xvfb (xvfb-run) when there is none, skipped
  # when neither exists.
  if(CELLUI_BUILD_TESTS)
    add_executable(cellui_gtk_tests tests/test_main.cpp tests/GtkWindowTests.cpp)
    target_link_libraries(cellui_gtk_tests PRIVATE cellui)
    find_program(CELLUI_XVFB_RUN xvfb-run)
    if(CELLUI_XVFB_RUN)
      add_test(NAME cellui_gtk_tests COMMAND ${CELLUI_XVFB_RUN} -a $<TARGET_FILE:cellui_gtk_tests>)
    else()
      add_test(NAME cellui_gtk_tests COMMAND cellui_gtk_tests)
    endif()
    set_tests_properties(cellui_gtk_tests PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: no display")
  endif()
endif()
# The Vulkan layer (cellui/vulkan/): CELLUI_ENABLE_VULKAN, set by the
# including project or, by default, on when the Vulkan headers and glslang are
# found. The shaders are compiled to SPIR-V here and embedded as headers; the
# loader is linked, so a machine without a Vulkan driver reports "no device"
# instead of failing to start.
if(NOT DEFINED CELLUI_ENABLE_VULKAN)
  find_package(Vulkan QUIET)
  find_program(CELLUI_GLSLANG glslangValidator)
  if(Vulkan_FOUND AND CELLUI_GLSLANG)
    set(CELLUI_ENABLE_VULKAN ON)
  endif()
endif()
if(CELLUI_ENABLE_VULKAN)
  find_package(Vulkan REQUIRED)
  find_program(CELLUI_GLSLANG glslangValidator REQUIRED)
  set(CELLUI_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
  file(MAKE_DIRECTORY ${CELLUI_SHADER_DIR})
  set(CELLUI_SHADER_HEADERS)
  foreach(shader solid.vert solid.frag)
    string(REPLACE "." "_" shader_var ${shader})
    set(header ${CELLUI_SHADER_DIR}/${shader}.h)
    add_custom_command(OUTPUT ${header}
      COMMAND ${CELLUI_GLSLANG} -V --target-env vulkan1.3 --vn ${shader_var}_spv -o ${header}
              ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders/${shader}
      DEPENDS src/vulkan/shaders/${shader}
      COMMENT "SPIR-V ${shader}")
    list(APPEND CELLUI_SHADER_HEADERS ${header})
  endforeach()
  add_library(cellui_vulkan STATIC src/vulkan/Context.cpp ${CELLUI_SHADER_HEADERS})
  target_include_directories(cellui_vulkan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CELLUI_SHADER_DIR})
  target_link_libraries(cellui_vulkan PRIVATE Vulkan::Vulkan)
  target_compile_definitions(cellui_vulkan PUBLIC CELLUI_HAS_VULKAN)
  if(CELLUI_BUILD_TESTS)
    # What the machine's Vulkan offers, for a person to run on real hardware.
    add_executable(cellui_vulkan_probe tools/vulkan_probe.cpp)
    target_link_libraries(cellui_vulkan_probe PRIVATE cellui_vulkan)
    add_executable(cellui_vulkan_tests tests/test_main.cpp tests/VulkanTests.cpp)
    target_link_libraries(cellui_vulkan_tests PRIVATE cellui_vulkan)
    add_test(NAME cellui_vulkan_tests COMMAND cellui_vulkan_tests)
    set_tests_properties(cellui_vulkan_tests PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: no Vulkan")
  endif()
endif()
# The Windows window backend (cellui/win32/): a Win32 window drawn with
# Direct2D and DirectWrite, on a loop that waits on the message queue. All of
# it is part of Windows, so there is nothing to find.
if(WIN32)
  target_sources(cellui PRIVATE src/win32/Win32EventLoop.cpp src/win32/Window.cpp)
  target_link_libraries(cellui PRIVATE user32 gdi32 d2d1 dwrite dwmapi ole32 uuid)
  target_compile_definitions(cellui PUBLIC CELLUI_HAS_WIN32)
endif()
# The pixel versions draw with canvas' Cairo backend, its Quartz 2D one on
# Apple platforms or its GDI+ one on Windows. The cell rasterizer (the GTK
# window's, the terminal's backdrops) is not built on Windows, whose window
# draws cells itself.
target_sources(cellui PRIVATE src/Button.cpp src/Dial.cpp src/NumberParse.cpp src/DialImage.cpp src/HeatmapImage.cpp src/NativeContext.cpp src/PixelCharts.cpp src/Lamp.cpp src/HdrSplit.cpp src/HdrBloom.cpp src/SparklineImage.cpp src/TraceImage.cpp src/WaveformImage.cpp)
if(NOT WIN32)
  target_sources(cellui PRIVATE src/CanvasRasterizer.cpp)
  target_compile_definitions(cellui PRIVATE CELLUI_CANVAS_RASTERIZER)
endif()
if(NOT WIN32 AND NOT APPLE)
  # Text in a font the one asked for lacks is found with fontconfig (Core
  # Text falls back by itself).
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CELLUI_FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
  target_link_libraries(cellui PRIVATE PkgConfig::CELLUI_FONTCONFIG)
endif()
target_compile_definitions(cellui PRIVATE CELLUI_PIXEL_SCOPES)
# canvas' headers are reached as SYSTEM includes.
target_include_directories(cellui SYSTEM PUBLIC ${canvas_SOURCE_DIR}/include)
# fast_float, vendored in third_party/ (NumberParse.cpp).
target_include_directories(cellui SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(cellui PUBLIC canvas)
target_include_directories(cellui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(cellui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(CELLUI_USE_UTF8PROC)
  target_compile_definitions(cellui PRIVATE CELLUI_UTF8PROC=1)
  if(CELLUI_UTF8PROC_FETCHED)
    # The fetched target carries its include directory and UTF8PROC_STATIC.
    target_link_libraries(cellui PUBLIC utf8proc)
  else()
    target_include_directories(cellui PRIVATE ${UTF8PROC_INCLUDE_DIR})
    target_link_libraries(cellui PUBLIC ${UTF8PROC_LIBRARY})
    # utf8proc's header declares dllimport symbols unless told it is static.
    if(WIN32 AND UTF8PROC_LIBRARY MATCHES "\\.a$" AND NOT UTF8PROC_LIBRARY MATCHES "\\.dll\\.a$")
      target_compile_definitions(cellui PRIVATE UTF8PROC_STATIC=1)
    endif()
  endif()
else()
  target_include_directories(cellui PRIVATE ${UNISTRING_INCLUDE_DIR})
  target_link_libraries(cellui PUBLIC ${UNISTRING_LIBRARY})
endif()

if(CELLUI_BUILD_TESTS)
# Prints the gallery page (every style and widget the library has).
add_executable(cellui_gallery gallery/main.cpp)
target_link_libraries(cellui_gallery PRIVATE cellui)
if(NOT WIN32)
  # Driven by tools/pacing/run_assertions.py.
  add_executable(cellui_pacing_probe tools/pacing_probe.cpp)
  target_link_libraries(cellui_pacing_probe PRIVATE cellui)
  # Times the deflate encoder on raw pictures (see the file's top comment).
  add_executable(cellui_deflate_bench tools/deflate_bench.cpp)
  target_link_libraries(cellui_deflate_bench PRIVATE cellui)
  # Times the picture encoder and hashes its bytes, so a speed-up can be shown to leave them alone.
  add_executable(cellui_sixel_bench tools/sixel_bench.cpp)
  target_link_libraries(cellui_sixel_bench PRIVATE cellui)
  # The frame pacer against shaped ptys (bandwidth, latency, a silent terminal).
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cellui_pacing_harness
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/pacing/run_assertions.py $<TARGET_FILE:cellui_pacing_probe>)
    # Timing-based: parallel load on a busy machine would fail its checks.
    set_tests_properties(cellui_pacing_harness PROPERTIES TIMEOUT 400 RUN_SERIAL TRUE)
    # tools/e2e/ptyscreen.py, the pty screen reader applications' end-to-end
    # scripts use; skipped (77) where pyte is not installed.
    add_test(NAME cellui_ptyscreen
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/e2e/selftest.py)
    set_tests_properties(cellui_ptyscreen PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
  endif()
endif()

# The library's own tests: plain functions registered with TEST(), run by
# tests/test_main.cpp; no framework dependency.
add_executable(cellui_tests
  tests/test_main.cpp
  tests/BackendChoiceTests.cpp
  tests/BufferPlaneTests.cpp
  tests/CellBufferTests.cpp
  tests/ChoiceDialogTests.cpp
  tests/ModalDialogTests.cpp
  tests/FormModalTests.cpp
  tests/ChartModalTests.cpp
  tests/ChoiceListTests.cpp
  tests/ScrollBodyTests.cpp
  tests/CommandPaletteTests.cpp
  tests/ColorTests.cpp
  tests/DiscoveryTests.cpp
  tests/KittyImagesTests.cpp
  tests/EventTests.cpp
  tests/InputTests.cpp
  tests/SignalTests.cpp
  tests/WidgetTests.cpp
  tests/FadeTests.cpp
  tests/FrameEncoderTests.cpp
  tests/FramePacerTests.cpp
  tests/DeflateTests.cpp
  tests/LinkStatusTests.cpp
  tests/ImageEncoderTests.cpp
  tests/GalleryTests.cpp
  tests/HeatmapChartTests.cpp
  tests/InfoDialogTests.cpp
  tests/MarkdownTests.cpp
  tests/ThemeTests.cpp
  tests/SpinBoxTests.cpp
  tests/ButtonTests.cpp
  tests/CheckboxTests.cpp
  tests/WheelAccelerationTests.cpp
  tests/SubcellGlyphsTests.cpp
  tests/ViewportTests.cpp
  tests/InlineEditorTests.cpp
  tests/InputParserTests.cpp
  tests/KeymapTests.cpp
  tests/GlyphMeterTests.cpp
  tests/EscapeCoalescerTests.cpp
  tests/KeyReaderTests.cpp
  tests/LineEditorTests.cpp
  tests/MenuBarTests.cpp
  tests/TableTests.cpp
  tests/LayoutTests.cpp
  tests/StatusLineTests.cpp
  tests/TestBackendTests.cpp
  tests/ApplicationTests.cpp
  tests/MeterTests.cpp
  tests/NumberParseTests.cpp
  tests/Utf8Tests.cpp)
if(NOT WIN32)
  target_sources(cellui_tests PRIVATE tests/BackdropEmulationTests.cpp tests/CanvasRasterizerTests.cpp tests/EventLoopTests.cpp tests/ThreadPostsTests.cpp tests/TerminalSessionTests.cpp tests/HeatmapImageTests.cpp tests/HdrSplitTests.cpp tests/HdrBloomTests.cpp tests/LampTests.cpp
    tests/SparklineTests.cpp
  tests/StageStatsTests.cpp tests/DialTests.cpp)
endif()
if(WIN32)
  target_sources(cellui_tests PRIVATE tests/Win32Tests.cpp)
endif()
target_link_libraries(cellui_tests PRIVATE cellui)
if(NOT WIN32 AND NOT APPLE)
  target_link_libraries(cellui_tests PRIVATE PkgConfig::CELLUI_FONTCONFIG)
endif()
target_compile_definitions(cellui_tests PRIVATE CELLUI_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
add_test(NAME cellui_tests COMMAND cellui_tests)

# Nothing in the library may reach into the application.
add_test(NAME cellui_includes_nothing_from_src
  COMMAND ${CMAKE_COMMAND} -DDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_includes.cmake)
endif()
