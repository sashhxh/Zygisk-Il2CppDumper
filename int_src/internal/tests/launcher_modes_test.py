#!/usr/bin/env python3
"""Verify actual launcher option parsing, restart arguments and no-load exit.
Android commands are mocked; this does not verify device graphics or visibility.
"""
from pathlib import Path
import argparse,json,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('launcher',type=Path);a=p.parse_args();source=a.launcher.read_text()
settings=source[source.index('LOAD_LIBRARY='):source.index('[ -x /system/bin/am ]')]
start=source.index('restart_game() {\n');restart=source[start:source.index('\n}\n',start)+3]
start=source.index('if [ "$LOAD_LIBRARY" = 0 ]; then',source.index('\nrestart_game\n'))
no_load=source[start:source.index('\nfi\n',start)+4]
cases=[([],0,'-force-vulkan',True),(['--without-library'],0,'-force-vulkan',False),(['--stock'],0,'',False),(['--help'],0,None,False),(['--invalid'],1,None,False),(['--stock','--gles30'],1,None,False),(['--gles30'],1,None,False)]
for arguments,expected,graphics,inject in cases:
    with tempfile.TemporaryDirectory(prefix='launcher-mode-') as temp:
        work=Path(temp);binary=work/'bin';binary.mkdir();calls=work/'calls'
        am=binary/'am';am.write_text('#!/usr/bin/env python3\nimport os,sys,json\nwith open(os.environ["MODE_CALLS"],"a") as f:f.write(json.dumps(sys.argv[1:])+"\\n")\n');am.chmod(0o755)
        harness='\n'.join(['set -eu','fail() { printf "%s\\n" "$*" >&2; exit 1; }',settings,'PACKAGE=com.catsbit.oxidesurvivalisland','USER_ID=0','COMPONENT=$PACKAGE/TestActivity','find_game_pid() { GAME_PID=""; GAME_PIDS=""; }',restart,'restart_game',no_load,'printf "would-load-library\\n"'])
        result=subprocess.run(['sh','-s','--',*arguments],input=harness,text=True,capture_output=True,env=dict(os.environ,PATH=str(binary)+':'+os.environ['PATH'],MODE_CALLS=str(calls)))
        assert result.returncode==expected,(arguments,result.stderr)
        commands=[json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else []
        if graphics is None:assert not commands
        else:
            assert commands[0]==['force-stop','--user','0','com.catsbit.oxidesurvivalisland']
            assert len(commands)==2 and commands[1][:3]==['start','--user','0'] and '-W' not in commands[1]
            assert commands[1][-3:]==['--es','unity',graphics],commands[1]
        assert ('would-load-library' in result.stdout)==inject
        print('PASS launcher mode:',arguments or ['default'])
