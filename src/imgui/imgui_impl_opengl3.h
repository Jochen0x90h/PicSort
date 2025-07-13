// based on imgui_impl_opengl3.h of https://github.com/ocornut/imgui

#pragma once

#include <functional>
#include "imgui.h"      // IMGUI_IMPL_API

// Backend API
IMGUI_IMPL_API bool     ImGui_ImplOpenGL3_Init();
IMGUI_IMPL_API void     ImGui_ImplOpenGL3_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplOpenGL3_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data, std::function<bool (char const *)> filter);

// (Optional) Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool     ImGui_ImplOpenGL3_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplOpenGL3_DestroyDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = NULL to handle this manually.
IMGUI_IMPL_API void     ImGui_ImplOpenGL3_UpdateTexture(ImTextureData* tex);
