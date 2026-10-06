"""Run native Tasks, navigation and calendar/web transport tests (no ctypes DLL)."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc',default='C:/TouchGFX/4.26.1/env/MinGW/bin/gcc.exe')
parser.add_argument('--cxx',default='C:/TouchGFX/4.26.1/env/MinGW/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
environment=dict(os.environ)
environment['PATH']=str(Path(args.cc).parent)+os.pathsep+environment.get('PATH','')
with tempfile.TemporaryDirectory(prefix='aura_tasks_',ignore_cleanup_errors=True) as folder:
    def run(name,compiler,sources,includes,standard):
        executable=Path(folder)/(name+'.exe')
        command=[compiler,'-std='+standard,'-Wall','-Wextra','-Werror']
        command+=['-I'+str(root/p) for p in includes]
        command+=[str(root/p) for p in sources]
        subprocess.run(command+['-o',str(executable)],check=True,env=environment)
        subprocess.run([str(executable)],check=True,env=environment)
    run('tasks',args.cc,['tests/host/test_tasks.c','Components/Tasks/tasks_data.c','Components/Calendar/calendar_data.c'],
        ['Components/Tasks','Components/Calendar'],'c11')
    run('tasks_nav',args.cxx,['tests/host/test_tasks_nav.cpp'],['TouchGFX/gui/include'],'c++17')
    run('tasks_demo',args.cc,['tests/host/test_tasks_demo.c','App/Src/app_tasks.c',
        'Components/Tasks/tasks_data.c','Components/Calendar/calendar_data.c'],
        ['tests/calendar_fakes','App/Inc','Components/Tasks','Components/Calendar'],'c11')
    run('calendar_service',args.cc,['tests/host/test_calendar_service.c','Components/Calendar/calendar_data.c'],
        ['tests/calendar_fakes','App/Inc','App/Src','Components/Calendar','Components/Tasks'],'c11')
    run('reminders',args.cc,['tests/host/test_reminders.c','App/Src/app_reminders.c',
        'App/Src/app_tasks.c','Components/Tasks/tasks_data.c','Components/Calendar/calendar_data.c'],
        ['tests/calendar_fakes','App/Inc','Components/Tasks','Components/Calendar'],'c11')
    run('calendar_nav',args.cxx,['tests/host/test_calendar_nav.cpp'],
        ['Components/Calendar','TouchGFX/gui/include'],'c++11')
print('All native Tasks/calendar/web tests passed.')
