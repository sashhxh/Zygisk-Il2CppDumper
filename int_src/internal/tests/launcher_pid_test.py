#!/usr/bin/env python3
"""Run actual PID selection against procfs fixtures, including duplicate pidof."""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'launcher-template.sh').read_text()
def function(name):
    start=source.index(name+'() {\n')
    return source[start:source.index('\n}\n',start)+3]
find='\n'.join(function(name) for name in ('candidate_pids','has_mapped_library','find_game_pid'))
package = 'com.catsbit.oxidesurvivalisland'

def process(pid, uid=10281, parent=50, name=package, unity=True, tgid=None, state='S'):
    return dict(pid=pid, uid=uid, parent=parent, name=name, unity=unity,
                tgid=pid if tgid is None else tgid, state=state)

cases = [
    ('one', '100', [process(100)], '100', '100'),
    ('duplicate-pidof', '100 100 100', [process(100)], '100', '100'),
    ('other-user', '200 100', [process(200, uid=1010281), process(100)], '100', '100'),
    ('other-uid', '200 100', [process(200, uid=11000), process(100)], '100', '100'),
    ('thread-id', '101 100', [process(101, tgid=100), process(100)], '100', '100'),
    ('colon-service', '200 100', [process(200, name=package+':service'), process(100)], '100', '100'),
    ('zombie', '200 100', [process(200, state='Z'), process(100)], '100', '100'),
    ('dead', '200 100', [process(200, state='X'), process(100)], '100', '100'),
    ('vanished', '200 100', [process(100)], '100', '100'),
    ('invalid-pidof', '-1 abc 100 100', [process(100)], '100', '100'),
    ('forked-child', '200 100', [process(200, parent=100), process(100)], '100', '200 100'),
    ('forked-child-only-loaded', '200 100', [process(200, parent=100), process(100, unity=False)], '100', '200 100'),
    ('one-loaded-main', '200 100', [process(200, unity=False), process(100)], '100', '200 100'),
    ('ambiguous-mains', '200 100', [process(200), process(100)], '', '200 100'),
    ('no-zygote-single', '100', [process(100, parent=1)], '100', '100'),
    ('pidof-misses-main', '', [process(100)], '100', '100'),
    ('pidof-only-threads', '101', [process(101, tgid=100), process(100)], '100', '100'),
    ('no-process', '', [], '', ''),
]
for name, pids, processes, expected, all_pids in cases:
    with tempfile.TemporaryDirectory(prefix='cfesp PID ') as temp:
        root = Path(temp);proc = root / 'proc';proc.mkdir()
        (proc / '50').mkdir();(proc / '50/cmdline').write_bytes(b'zygote64\0')
        for entry in processes:
            folder = proc / str(entry['pid']);folder.mkdir()
            (folder / 'cmdline').write_bytes(entry['name'].encode()+b'\0ignored argument\0')
            (folder / 'status').write_text(
                f"State:\t{entry['state']}\nTgid:\t{entry['tgid']}\n"
                f"PPid:\t{entry['parent']}\nUid:\t{entry['uid']}\t{entry['uid']}\t{entry['uid']}\t{entry['uid']}\n")
            (folder / 'maps').write_text('1000-2000 r-xp 0 00:00 1 /data/app/game/lib/arm64/libunity.so\n' if entry['unity'] else '')
        harness='\n'.join([
            'set -eu', 'PACKAGE=$TEST_PACKAGE', 'APP_UID=10281', 'PROC_ROOT=$TEST_PROC',
            'pidof() { printf "%s\\n" "$TEST_PIDS"; }', find,
            'set -- preserved arguments', 'find_game_pid',
            '[ "$1" = preserved ] && [ "$2" = arguments ]',
            'printf "selected=%s\\nall=%s\\n" "$GAME_PID" "$GAME_PIDS"',
        ])
        result=subprocess.run(['sh'],input=harness,text=True,capture_output=True,
            env=dict(os.environ,TEST_PACKAGE=package,TEST_PROC=str(proc),TEST_PIDS=pids))
        assert result.returncode==0,(name,result.stderr)
        selected,all_found=result.stdout.splitlines()
        assert selected==f'selected={expected}',(name,result.stdout)
        assert set(all_found.removeprefix('all=').split())==set(all_pids.split()),(name,result.stdout)
        print('PASS PID selection:',name)

# Exercise the actual PackageManager UID parsing and current-user guard.
start=source.index('APP_UID=$(cmd package list packages')
uid_code=source[start:source.index('PROC_ROOT=/proc',start)]
for name, user, output, expected in [
    ('owner',0,f'package:{package} uid:10281\n',0),
    ('work-profile',10,f'package:{package} uid:1010281\n',0),
    ('exact-package',0,f'package:{package}.clone uid:11000\npackage:{package} uid:10281\n',0),
    ('wrong-profile',0,f'package:{package} uid:1010281\n',1),
    ('empty',0,'',1),('malformed',0,f'package:{package} uid:bad\n',1),
]:
    harness='\n'.join(['set -eu','fail() { printf "%s\\n" "$*" >&2; exit 1; }',
        'PACKAGE=$TEST_PACKAGE','USER_ID=$TEST_USER','cmd() { printf "%s" "$TEST_OUTPUT"; }',uid_code])
    result=subprocess.run(['sh'],input=harness,text=True,capture_output=True,
        env=dict(os.environ,TEST_PACKAGE=package,TEST_USER=str(user),TEST_OUTPUT=output))
    assert result.returncode==expected,(name,result.stderr)
    print('PASS package UID:',name)
