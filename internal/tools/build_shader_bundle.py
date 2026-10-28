#!/usr/bin/env python3
"""Derive one depth-independent Unlit shader from this game's compiled shader.

The compiled GLES/Vulkan programs are preserved. Only ShaderLab render state,
identity, and the bundle's object/container tables change.
Requires UnityPy; input is shaders-builtin from the user-supplied APK.
"""
import argparse
import hashlib
from pathlib import Path

import UnityPy

parser = argparse.ArgumentParser()
parser.add_argument('input', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--header', type=Path)
args = parser.parse_args()
env = UnityPy.load(str(args.input))
bundle = next(iter(env.files.values()))
shader = next(o for o in env.objects if o.type.name == 'Shader' and
              o.read_typetree()['m_ParsedForm']['m_Name'] == 'Universal Render Pipeline/Unlit')
container = next(o for o in env.objects if o.type.name == 'AssetBundle')
tree = shader.read_typetree()
programs = bytes(tree['compressedBlob'])
identity = hashlib.sha256(programs + b'cfesp-unlit-always-r12-backface').digest()
tree['m_Name'] = 'CFESP/UnlitAlways'
tree['m_ParsedForm']['m_Name'] = 'CFESP/UnlitAlways'
tree['m_ParsedForm']['m_CustomEditorName'] = ''
tree['m_ParsedForm']['m_CustomEditorForRenderPipelines'] = []
tree['m_ParsedForm']['m_Dependencies'] = []
tree['m_Dependencies'] = []
tree['m_NonModifiableTextures'] = []
tree['m_AssetGUID'] = {f'data[{i}]': int.from_bytes(identity[i*4:i*4+4], 'little') for i in range(4)}


def constant(value):
    return {'val': float(value), 'name': '<noninit>'}


for sub in tree['m_ParsedForm']['m_SubShaders']:
    for key, value in sub['m_Tags']['tags']:
        if key == 'RenderType':
            sub['m_Tags']['tags'] = [(k, 'Transparent' if k == 'RenderType' else v) for k, v in sub['m_Tags']['tags']]
            break
    for render_pass in sub['m_Passes']:
        state = render_pass['m_State']
        state['zTest'] = constant(8)  # Always: fixed render state, no UI global.
        state['zWrite'] = constant(0)
        state['culling'] = constant(2)  # One face instead of UI shader's two.
        state['alphaToMask'] = constant(0)
        state['stencilReadMask'] = constant(255)
        state['stencilWriteMask'] = constant(0)
        state['stencilRef'] = constant(0)
        for name in ['stencilOp', 'stencilOpFront', 'stencilOpBack']:
            state[name] = {k: constant(8 if k == 'comp' else 0) for k in ['pass', 'fail', 'zFail', 'comp']}
        state['rtBlend0'].update(srcBlend=constant(5), destBlend=constant(10),
                                 srcBlendAlpha=constant(1), destBlendAlpha=constant(10),
                                 blendOp=constant(0), blendOpAlpha=constant(0), colMask=constant(15))
shader.save_typetree(tree)
ptr = {'m_FileID': 0, 'm_PathID': shader.path_id}
asset = {'preloadIndex': 0, 'preloadSize': 1, 'asset': ptr}
bt = container.read_typetree()
bt.update(m_Name='cfesp_unlit_always_r12', m_AssetBundleName='cfesp_unlit_always_r12',
          m_PreloadTable=[ptr], m_Container=[('cfesp/unlit_always', asset)],
          m_MainAsset=asset, m_Dependencies=[], m_IsStreamedSceneAssetBundle=False)
container.save_typetree(bt)
serialized = shader.assets_file
serialized.objects = {shader.path_id: shader, container.path_id: container}
serialized.externals = []
serialized.name = 'CAB-' + identity.hex()[:32]
bundle.files = {serialized.name: serialized}
result = bundle.save(packer='lz4')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_bytes(result)
verified = UnityPy.load(result)
objects = verified.objects
assert sorted(o.type.name for o in objects) == ['AssetBundle', 'Shader']
st = next(o for o in objects if o.type.name == 'Shader').read_typetree()
assert bytes(st['compressedBlob']) == programs
assert st['m_ParsedForm']['m_Name'] == 'CFESP/UnlitAlways'
for sub in st['m_ParsedForm']['m_SubShaders']:
    for p in sub['m_Passes']:
        assert p['m_State']['zTest'] == constant(8)
        assert p['m_State']['culling'] == constant(2)
if args.header:
    lines = ['#pragma once', '#include <cstddef>', 'inline constexpr unsigned char kChamsShaderBundle[]={']
    lines.extend('    ' + ','.join(f'0x{b:02x}' for b in result[i:i+20]) + ',' for i in range(0, len(result), 20))
    lines.extend(['};', 'inline constexpr size_t kChamsShaderBundleSize=sizeof(kChamsShaderBundle);', ''])
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text('\n'.join(lines))
print(f'Created {args.output}: {len(result)} bytes; 2 objects; compiled GPU programs preserved')
