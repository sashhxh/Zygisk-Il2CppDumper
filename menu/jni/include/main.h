#pragma once
#include "Android_draw/draw.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include <atomic>

extern std::atomic<bool> main_thread_flag;
extern bool menu_open;

void SetupStyle();
void LoadFonts();
void RenderMenu();
