#!/usr/bin/env python3
"""Check menu layout and interaction on the host, without Android GPU claims."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--world-preview',type=Path,help='optional prefix for actual ImGui mesh/atlas exports')
a=p.parse_args();root=Path(__file__).resolve().parents[1];imgui=root/'jni/imgui'
with tempfile.TemporaryDirectory(prefix='mango-menu-') as temp:
    for name in ('reference_menu',):
        binary=Path(temp)/name
        command=[os.environ.get('CXX','g++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'jni'),'-I'+str(imgui)]
        command+=['-DIMGUI_ENABLE_TEST_ENGINE']
        command+=[str(root/'tests'/f'{name}_test.cpp')]+[str(imgui/f'{part}.cpp') for part in ('imgui','imgui_draw','imgui_tables','imgui_widgets')]+['-lz','-o',str(binary)]
        subprocess.run(command,check=True)
        arguments=[str(a.world_preview)] if a.world_preview else []
        subprocess.run([str(binary),*arguments],check=True)
