#!/usr/bin/env python3
"""Loader and cached Unity dispatch fixture; no actual code patching/device."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--vulkan-include',type=Path,default=Path('/usr/include'));a=p.parse_args();root=Path(__file__).resolve().parents[1];imgui=root/'jni/imgui'
with tempfile.TemporaryDirectory(prefix='cfesp-vk-hooks-') as temp:
    work=Path(temp);headers=work/'include';headers.mkdir()
    for name in ('vulkan','vk_video'):
        if (a.vulkan_include/name).exists():(headers/name).symlink_to(a.vulkan_include/name,target_is_directory=True)
    includes=['-I'+str(headers),'-I'+str(root/'jni'),'-I'+str(imgui),'-I'+str(imgui/'backends')]
    subprocess.run(['g++','-std=c++17','-shared','-fPIC','-Wl,-Bsymbolic',*includes,str(root/'tests/vulkan_hooks_fixture.cpp'),'-o',str(work/'libvulkan.so')],check=True)
    command=['g++','-std=c++17','-O1','-DMANGO_VULKAN_TESTS','-DVK_NO_PROTOTYPES','-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES',*includes,str(root/'tests/vulkan_hooks_test.cpp'),str(root/'jni/vulkan_hooks.cpp'),str(root/'jni/vulkan_overlay.cpp'),str(imgui/'backends/imgui_impl_vulkan.cpp')]+[str(imgui/(name+'.cpp')) for name in ('imgui','imgui_draw','imgui_tables','imgui_widgets')]+['-ldl','-pthread','-o',str(work/'test')]
    subprocess.run(command,check=True);subprocess.run([str(work/'test')],check=True,env=dict(os.environ,LD_LIBRARY_PATH=str(work)))
