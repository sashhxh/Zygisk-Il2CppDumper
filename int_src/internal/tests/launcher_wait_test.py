#!/usr/bin/env python3
"""Exercise startup states and a monotonic deadline, without real sleeps."""
from pathlib import Path
import os,subprocess,tempfile

source=(Path(__file__).resolve().parents[1]/'launcher-template.sh').read_text()
def function(name):
    start=source.index(name+'() {\n')
    return source[start:source.index('\n}\n',start)+3]

for case,step,expected in [
    ('ready',1,0),('delayed',1,0),('no_process',30,1),
    ('ambiguous',30,1),('unreadable_maps',30,1),('no_unity',30,1),
    ('slow_probe',100,1),('already_loaded',1,0),
]:
    with tempfile.TemporaryDirectory(prefix='cfesp wait ') as temp:
        root=Path(temp);(root/'100').mkdir()
        (root/'100/maps').write_text('1000-2000 r-xp 0 00:00 1 /data/app/game/lib/arm64/libunity.so (deleted)\n')
        setup='''
APP_UID=10281
TEST_NOW=1000
CALLS=0
SLEEPS=0
clock_seconds() { WAIT_NOW=$TEST_NOW; TEST_NOW=$((TEST_NOW + TEST_STEP)); }
sleep() { SLEEPS=$((SLEEPS + 1)); }
find_game_pid() {
    CALLS=$((CALLS + 1))
    GAME_PID=100
    GAME_PIDS=100
    case "$TEST_CASE" in
        no_process) GAME_PID=''; GAME_PIDS='' ;;
        ambiguous) GAME_PID=''; GAME_PIDS='100 200' ;;
        unreadable_maps) GAME_PID=200; GAME_PIDS=200 ;;
        delayed) if [ "$CALLS" -lt 3 ]; then GAME_PID=''; GAME_PIDS=''; fi ;;
        slow_probe) GAME_PID=''; GAME_PIDS='' ;;
    esac
}'''
        if case=='no_unity':(root/'100/maps').write_text('1000-2000 r-xp 0 00:00 1 /data/app/game/base.apk\n')
        if case=='already_loaded':(root/'100/maps').write_text('1000-2000 r-xp 0 00:00 1 /data/app/game/lib/arm64/libcfesp.so\n')
        harness='\n'.join(['set -eu','PROC_ROOT=$TEST_ROOT',setup,function('has_mapped_library'),function('wait_for_unity'),
            'result=0; wait_for_unity || result=$?',
            'printf "result=%s ready=%s loaded=%s probes=%s sleeps=%s\\n" "$result" "$READY" "$ALREADY_LOADED" "$CALLS" "$SLEEPS"'])
        result=subprocess.run(['sh'],input=harness,text=True,capture_output=True,
            env=dict(os.environ,TEST_ROOT=temp,TEST_CASE=case,TEST_STEP=str(step)))
        assert result.returncode==0,(case,result.stderr)
        assert f'result={expected}' in result.stdout,(case,result.stdout)
        if case=='slow_probe':assert 'probes=1 sleeps=0' in result.stdout,result.stdout
        if case in ('no_process','ambiguous','unreadable_maps','no_unity'):
            assert 'probes=3 sleeps=2' in result.stdout,result.stdout
            assert 'осталось 0 с' in result.stdout
        if case=='delayed':assert 'ready=1 loaded=0 probes=3' in result.stdout
        if case=='already_loaded':assert 'ready=0 loaded=1' in result.stdout
        print('PASS Unity wait:',case)

# Check the real monotonic reader and map matching, including APK paths,
# deleted mappings and names which merely contain the library filename.
with tempfile.TemporaryDirectory(prefix='cfesp clock ') as temp:
    root=Path(temp);(root/'uptime').write_text('12345.67 54321.0\n')
    for name,present in [('libunity.so',True),('libunity.so (deleted)',True),
                         ('base.apk!/lib/arm64-v8a/libunity.so',True),
                         ('libunity.so.backup',False),('notlibunity.so',False)]:
        maps=root/'maps';maps.write_text('1000-2000 r-xp 0 00:00 1 /data/app/'+name+'\n')
        harness='\n'.join(['set -eu','PROC_ROOT=$TEST_ROOT','fail() { exit 1; }',function('clock_seconds'),function('has_mapped_library'),
            'clock_seconds; [ "$WAIT_NOW" = 12345 ]','result=0; has_mapped_library "$PROC_ROOT/maps" libunity.so || result=$?',
            'printf "%s\\n" "$result"'])
        result=subprocess.run(['sh'],input=harness,text=True,capture_output=True,env=dict(os.environ,TEST_ROOT=temp))
        assert result.returncode==0 and result.stdout==('0\n' if present else '1\n'),(name,result.stdout,result.stderr)
        print('PASS mapping/clock:',name)

for case in ('no_process','process_no_unity'):
    with tempfile.TemporaryDirectory(prefix='cfesp startup ') as temp:
        root=Path(temp);proc=root/'proc';proc.mkdir()
        if case=='process_no_unity':
            (proc/'100').mkdir();(proc/'100/cmdline').write_bytes(b'com.catsbit.oxidesurvivalisland\0')
            (proc/'100/status').write_text('Uid:\t10281\nState:\tS\n')
            (proc/'100/maps').write_text('1000-2000 r-xp 0 00:00 1 /data/app/game/base.apk\n')
        harness='\n'.join(['set -eu','PROC_ROOT=$TEST_PROC','REPORT_ROOT=$TEST_ROOT','USER_ID=0','APP_UID=10281',
            'COMPONENT=game/Activity','GAME_PID=$TEST_PID','GAME_PIDS=$TEST_PID',"WAIT_REASON='Unity ещё не загружена.'",
            'candidate_pids() { printf "%s\\n" "$GAME_PID"; }','read_startup_log() { :; }',function('diagnose_startup'),'diagnose_startup'])
        result=subprocess.run(['sh'],input=harness,text=True,capture_output=True,
            env=dict(os.environ,TEST_PROC=str(proc),TEST_ROOT=temp,TEST_PID='100' if case=='process_no_unity' else ''))
        assert result.returncode==0,(case,result.stderr)
        reports=list(root.glob('cfesp-r32-report.*/startup.txt'));assert len(reports)==1
        assert str(reports[0]) in result.stdout and reports[0].stat().st_mode & 0o777==0o644
        assert 'UID 10281' in reports[0].read_text()
        print('PASS startup report:',case)
