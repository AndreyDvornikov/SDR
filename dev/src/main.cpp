#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "sdr/sdr_controller.hpp"
#include "ui/dashboard.hpp"
#include "ui/theme.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    int initial_tab = 0;
    const char* screenshot_path = nullptr;
    int target_frames = 60;
    bool start_fullscreen = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tab") == 0 && i + 1 < argc) {
            initial_tab = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (std::strcmp(argv[i], "--fullscreen") == 0) {
            start_fullscreen = true;
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            target_frames = std::atoi(argv[++i]);
        }
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(
        "ADALM-PLUTO SDR Dashboard | SibSUTI IKS-431",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        window_flags
    );
    if (!window) {
        std::printf("Error creating SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    if (start_fullscreen || screenshot_path) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load custom font with Cyrillic support
    const char* font_path = "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf";
    ImFont* custom_font = nullptr;
    FILE* f_check = std::fopen(font_path, "rb");
    if (f_check) {
        std::fclose(f_check);
        custom_font = io.Fonts->AddFontFromFileTTF(font_path, 15.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    }
    if (!custom_font) {
        io.Fonts->AddFontDefault();
    }

    // Apply aesthetic Cyber Orange Theme
    sdr::ui::apply_cyber_orange_theme();
    sdr::ui::apply_implot_theme();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize SDR Controller and UI
    sdr::SDRController controller;
    sdr::ui::DashboardUI dashboard(controller);

    if (initial_tab >= 0 && initial_tab <= 3) {
        dashboard.set_active_tab(static_cast<sdr::ui::DashboardTab>(initial_tab));
    }

    // Auto-start radio transceiver
    controller.start();

    // Main loop
    bool done = false;
    int frame_count = 0;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Render SDR Dashboard UI
        dashboard.render();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.055f, 0.065f, 0.085f, 1.0f); // WindowBg
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        frame_count++;
        if (screenshot_path && frame_count >= target_frames) {
            int w = (int)io.DisplaySize.x;
            int h = (int)io.DisplaySize.y;
            std::vector<unsigned char> pixels(w * h * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

            std::string ppm_path = std::string(screenshot_path) + ".ppm";
            FILE* f = std::fopen(ppm_path.c_str(), "wb");
            if (f) {
                std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                for (int y = h - 1; y >= 0; --y) {
                    std::fwrite(&pixels[y * w * 3], 1, w * 3, f);
                }
                std::fclose(f);
                std::string cmd = "magick " + ppm_path + " " + std::string(screenshot_path) + " && rm -f " + ppm_path;
                int ret = std::system(cmd.c_str());
                (void)ret;
                std::printf("Saved screenshot (%dx%d): %s\n", w, h, screenshot_path);
            }
            done = true;
        }
    }

    // Stop SDR before exiting
    controller.stop();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
