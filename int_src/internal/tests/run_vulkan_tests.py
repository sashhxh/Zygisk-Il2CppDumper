#!/usr/bin/env python3
"""Offscreen Vulkan/lavapipe test with synchronization validation.
Requires a Vulkan ICD and VK_LAYER_KHRONOS_validation; no Android/WSI claims.
"""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--vulkan-include',type=Path,default=Path('/usr/include'));p.add_argument('--overlay-source',type=Path);p.add_argument('--reference-ui',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[1];imgui=root/'jni/imgui'
with tempfile.TemporaryDirectory(prefix='cfesp-vulkan-') as temp:
    work=Path(temp);headers=work/'include';headers.mkdir()
    for name in ('vulkan','vk_video'):
        if (a.vulkan_include/name).exists():(headers/name).symlink_to(a.vulkan_include/name,target_is_directory=True)
    command=['g++','-std=c++17','-O1','-DVK_NO_PROTOTYPES','-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES','-I'+str(headers),'-I'+str(root/'jni'),'-I'+str(imgui),'-I'+str(imgui/'backends'),str(root/'tests/vulkan_overlay_test.cpp'),str(a.overlay_source or root/'jni/vulkan_overlay.cpp'),str(imgui/'backends/imgui_impl_vulkan.cpp')]+[str(imgui/(name+'.cpp')) for name in ('imgui','imgui_draw','imgui_tables','imgui_widgets')]+['-Wl,-l:libvulkan.so.1','-ldl','-pthread','-o',str(work/'test')]
    if a.reference_ui:command[1:1]=['-DREFERENCE_UI_FIXTURE'];command+=['-lz']
    subprocess.run(command,check=True);subprocess.run([str(work/'test')],check=True)
