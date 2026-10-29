#pragma once
#include "ImGui/imgui.h"

namespace ui {

bool Toggle(const char* label, bool* v);
bool Slider(const char* label, float* v, float lo, float hi, const char* fmt = "%.2f");
bool SliderInt(const char* label, int* v, int lo, int hi);
void Section(const char* text);
bool NotchPill(ImVec2 center, float width, float height, bool* open, ImVec2* dragStart, ImVec2* winStart, ImVec2* winPos, bool* dragging);
bool TabBarAnimated(const char* const* names, int count, int* current);

}
