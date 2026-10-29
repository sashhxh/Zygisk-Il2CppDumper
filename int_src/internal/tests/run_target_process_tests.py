#!/usr/bin/env python3
"""Check native target identity using Linux procfs; no ptrace/injection."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='cfesp-target-') as temp:
    binary=Path(temp)/'target_process_test'
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer','-I'+str(root/'jni'),str(root/'tests/target_process_test.cpp'),
        '-pthread','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
