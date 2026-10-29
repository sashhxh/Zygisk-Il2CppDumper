#include "main.h"
#include <atomic>

std::atomic<bool> main_thread_flag{true};
std::atomic<bool> g_frame_done{true};
bool menu_open = true;

int main(int argc, char* argv[]) {
    screen_config();
    int abs_ScreenX = displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width;
    native_window_screen_x = abs_ScreenX;
    native_window_screen_y = abs_ScreenX;

    if (!initGUI_draw(abs_ScreenX, abs_ScreenX, true)) return -1;
    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);

    LoadFonts();
    SetupStyle();

    while (main_thread_flag) {
        g_frame_done.store(false);
        drawBegin();
        RenderMenu();
        drawEnd();
        g_frame_done.store(true);
    }
    while (!g_frame_done.load()) {}
    shutdown(); Touch_Close();
    return 0;
}
