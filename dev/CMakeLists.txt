cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Preferences
set(OpenGL_GL_PREFERENCE GLVND)

# Packages
find_package(SoapySDR REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# ImGui sources
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/implot")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

# Common DSP & SDR sources
set(CORE_SOURCES
    src/dsp/modulation.cpp
    src/dsp/filters.cpp
    src/dsp/fft.cpp
    src/sdr/pcm_player.cpp
    src/sdr/sdr_soapy.cpp
    src/sdr/sdr_controller.cpp
)

# UI Sources
set(UI_SOURCES
    src/ui/theme.cpp
    src/ui/widgets.cpp
    src/ui/dashboard.cpp
)

# Main GUI Executable
add_executable(sdr_gui
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

target_include_directories(sdr_gui PRIVATE
    hdr
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${SoapySDR_INCLUDE_DIRS}
)

target_link_libraries(sdr_gui PRIVATE
    ${SoapySDR_LIBRARIES}
    SDL2::SDL2
    OpenGL::GL
    Threads::Threads
)

# Legacy CLI targets
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_main.cpp")
    add_executable(cli_main src/cli_main.cpp)
    target_include_directories(cli_main PRIVATE hdr)
    target_link_libraries(cli_main PRIVATE ${SoapySDR_LIBRARIES})
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_transmitter.cpp")
    add_executable(cli_tr src/cli_transmitter.cpp)
    target_include_directories(cli_tr PRIVATE hdr)
    target_link_libraries(cli_tr PRIVATE ${SoapySDR_LIBRARIES})
endif()
