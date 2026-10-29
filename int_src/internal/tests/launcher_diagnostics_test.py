#!/usr/bin/env python3
"""Exercise the launch report with absent, incomplete and delayed GPU logs."""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'launcher-template.sh').read_text()

def function(name):
    start = source.index(name + '() {\n')
    return source[start:source.index('\n}\n', start) + 3]

for case in ('empty', 'boot', 'current_boot', 'backend_only', 'delayed_frame', 'frame'):
    with tempfile.TemporaryDirectory(prefix='cfesp report ') as temp:
        work = Path(temp)
        reader = '''read_launch_log() {
    printf 'read\n' >> "$TEST_READS"
    case "$TEST_CASE" in
        empty) ;;
        boot) printf '%s\n' 'boot: pid=123 revision=MANGOCHEAT_REV=r32-resource-input-repair' ;;
        current_boot) printf '%s\n' 'boot: pid=123 revision=MANGOCHEAT_REV=r42-reference-ui' ;;
        backend_only) printf '%s\n' 'boot: pid=123 revision=MANGOCHEAT_REV=r32-resource-input-repair' 'Vulkan overlay ready' ;;
        delayed_frame)
            if [ "$(wc -l < "$TEST_READS")" -ge 3 ]; then
                printf '%s\n' 'Vulkan UI frame submitted'
            fi ;;
        frame) printf '%s\n' 'Vulkan UI frame submitted' ;;
    esac
}'''
        harness = '\n'.join([
            'set -eu', 'umask 077', 'REPORT_ROOT=$TEST_ROOT', 'GAME_PID=123',
            "UNITY_GRAPHICS='-force-vulkan'", reader,
            'sleep() { printf "wait\\n" >> "$TEST_WAITS"; }',
            function('diagnose_launch'), 'diagnose_launch',
        ])
        result = subprocess.run(
            ['sh'], input=harness, text=True, capture_output=True,
            env=dict(os.environ, TEST_ROOT=temp, TEST_CASE=case,
                     TEST_READS=str(work / 'reads'), TEST_WAITS=str(work / 'waits')),
        )
        assert result.returncode == 0, (case, result.stderr)
        reports = list(work.glob('cfesp-r32-report.*/launch.txt'))
        assert len(reports) == 1 and str(reports[0]) in result.stdout
        assert reports[0].stat().st_mode & 0o777 == 0o644
        successful = case in ('delayed_frame', 'frame')
        assert ('Первый кадр' in result.stdout) == successful
        assert ('Vulkan UI frame submitted' in reports[0].read_text()) == successful
        waits = (work / 'waits').read_text().count('wait') if (work / 'waits').exists() else 0
        assert waits == (2 if case == 'delayed_frame' else 0 if case == 'frame' else 15)
        if case in ('boot', 'current_boot', 'backend_only'):
            assert 'Библиотека запущена, но Vulkan-меню ещё не готово' in result.stdout
        if case == 'empty':
            assert 'Нет стартового сообщения библиотеки' in result.stdout
        print('PASS launcher report:', case)
