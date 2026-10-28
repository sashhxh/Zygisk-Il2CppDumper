#!/usr/bin/env python3
"""Run host lifecycle tests; this does not test Android rendering or FPS."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cxx = os.environ.get('CXX', 'g++')
with tempfile.TemporaryDirectory(prefix='mango-tests-') as temporary:
    work = Path(temporary)
    for name in ('chams', 'third_person', 'local_player_view', 'local_view_setup', 'local_weapon_view', 'two_hand_pose', 'custom_avatar', 'avatar_storage', 'local_body_geometry', 'graphics_roots', 'graphics_metadata', 'shader_asset', 'unity_state', 'esp_capture', 'unity_return', 'runtime_image'):
        binary = work / name
        subprocess.run([
            cxx, '-std=c++17', '-g', '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer', '-I' + str(root / 'jni'),
            str(root / 'tests' / (name + '_test.cpp')), '-ldl', '-o', str(binary),
        ], check=True)
        arguments = []
        if name == 'runtime_image':
            fixture = work / 'fixture.cpp'
            fixture.write_text('extern "C" int fixture_version(){return 1;}\n')
            library = work / 'libknown-image.so'
            subprocess.run([
                cxx, '-shared', '-fPIC', str(fixture),
                '-Wl,--build-id=0x0102030405060708090a0b0c0d0e0f1011121314',
                '-o', str(library),
            ], check=True)
            arguments.append(str(library))
        subprocess.run([str(binary), *arguments], check=True)
        print(name + ': PASS', flush=True)
