#!/usr/bin/env python3
"""Embed built ARM64 files in one Android shell script (no runtime network)."""
import argparse
import base64
import hashlib
from pathlib import Path

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--library-dir', type=Path, default=root / 'libs/arm64-v8a')
parser.add_argument('--output', type=Path, default=root.parent / 'start.sh')
args = parser.parse_args()
blocks = []
hashes = {}
for name, marker in [('cfinject', 'CFESP_INJECTOR_PAYLOAD'), ('libcfesp.so', 'CFESP_LIBRARY_PAYLOAD')]:
    data = (args.library_dir / name).read_bytes()
    hashes[name] = hashlib.sha256(data).hexdigest()
    blocks.append('    base64 -d > "$RUNTIME/' + name + '" <<\'' + marker + '\'\n'
                  + base64.encodebytes(data).decode('ascii') + marker + '\n')
script = (root / 'launcher-template.sh').read_text().replace(
    '@@PAYLOAD_FUNCTION@@', 'unpack_payload() {\n' + ''.join(blocks) + '}\n'
).replace('@@INJECTOR_SHA@@', hashes['cfinject']).replace('@@LIBRARY_SHA@@', hashes['libcfesp.so'])
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(script)
args.output.chmod(0o755)
print('Created', args.output)
for name, digest in hashes.items():
    print(name, digest)
