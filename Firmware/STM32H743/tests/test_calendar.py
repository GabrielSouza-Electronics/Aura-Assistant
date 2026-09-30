"""Host checks for production calendar/date/JSON/HTTP code and ToF navigation."""
import argparse
import ctypes as C
import datetime as dt
import json
from pathlib import Path
import random
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

class Date(C.Structure):
    _fields_ = [('year', C.c_uint16)] + [(x, C.c_uint8) for x in ('month','day','hour','minute','second')]
class Holidays(C.Structure):
    _fields_ = [('year', C.c_uint16), ('days', C.c_uint32*12), ('estimated', C.c_uint32*12)]

with tempfile.TemporaryDirectory(prefix='aura_calendar_', ignore_cleanup_errors=True) as directory:
    tmp = Path(directory)
    dll = tmp/'calendar.dll'
    subprocess.run([args.cc,'-std=c11','-Wall','-Wextra','-Werror','-shared',
                    str(root/'Components/Calendar/calendar_data.c'),'-o',str(dll)],check=True)
    lib = C.CDLL(str(dll))
    lib.Cal_Days.argtypes = [C.c_uint,C.c_uint]
    lib.Cal_Weekday.argtypes = [C.c_uint,C.c_uint,C.c_uint]
    lib.Cal_Advance.argtypes = [C.POINTER(Date),C.c_uint32]
    lib.Cal_ParseTime.argtypes = [C.c_char_p,C.POINTER(Date)]
    lib.Cal_ParseTime.restype = C.c_bool
    lib.Cal_ParseHolidays.argtypes = [C.c_char_p,C.c_uint,C.POINTER(Holidays)]
    lib.Cal_ParseHolidays.restype = C.c_bool
    lib.Cal_HttpBody.argtypes = [C.c_void_p,C.c_size_t,C.c_bool,C.c_bool,C.POINTER(C.c_void_p),C.POINTER(C.c_size_t)]
    # Every day in supported range, including leap years and month boundaries.
    date = dt.date(2020,1,1)
    while date.year <= 2099:
        assert lib.Cal_Weekday(date.year,date.month,date.day) == date.weekday()
        date += dt.timedelta(days=1)
    assert lib.Cal_Days(2000,2)==29 and lib.Cal_Days(2100,2)==28
    rng = random.Random(713)
    for _ in range(2000):
        start=dt.datetime(2020,1,1)+dt.timedelta(seconds=rng.randrange(2400000000))
        seconds=rng.randrange(86400*32)
        expected=start+dt.timedelta(seconds=seconds)
        v=Date(start.year,start.month,start.day,start.hour,start.minute,start.second)
        lib.Cal_Advance(C.byref(v),seconds)
        assert (v.year,v.month,v.day,v.hour,v.minute,v.second)==(expected.year,expected.month,expected.day,expected.hour,expected.minute,expected.second)

    fixtures = root/'tests/calendar_fixtures'
    time_json = (fixtures/'time.json').read_bytes()
    holidays_json = (fixtures/'holidays.json').read_bytes()
    value=Date()
    assert lib.Cal_ParseTime(time_json,C.byref(value))
    doc=json.loads(time_json)
    for key,bad in [('month',13),('day',0),('year',2100),('hour',24),('minute',60),('seconds',60),('hour',256),('timeZone','UTC')]:
        invalid=dict(doc);invalid[key]=bad
        before=bytes(value)
        assert not lib.Cal_ParseTime(json.dumps(invalid).encode(),C.byref(value)),key
        assert before==bytes(value)
    for index in range(len(time_json)):
        assert not lib.Cal_ParseTime(time_json[:index],C.byref(value))
    assert not lib.Cal_ParseTime(time_json[:-1]+b',"year":2026}',C.byref(value))
    holidays=Holidays()
    assert lib.Cal_ParseHolidays(holidays_json,2026,C.byref(holidays))
    assert holidays.days[0]&1 and holidays.days[11]&6==6
    assert any(holidays.estimated)
    assert not lib.Cal_ParseHolidays(b'{"country":"AE","year":2024,"years":[2025,2026],"count":0,"holidays":[]}',2024,C.byref(holidays))
    hdoc=json.loads(holidays_json)
    for key,bad in [('country','US'),('year',2027),('count',0)]:
        invalid=dict(hdoc);invalid[key]=bad
        before=bytes(holidays)
        assert not lib.Cal_ParseHolidays(json.dumps(invalid).encode(),2026,C.byref(holidays))
        assert bytes(holidays)==before
    for index in range(len(holidays_json)):
        assert not lib.Cal_ParseHolidays(holidays_json[:index],2026,C.byref(holidays))

    def parse_http(raw,eof=False,decode=False):
        buf=C.create_string_buffer(raw)
        ptr=C.c_void_p();size=C.c_size_t()
        result=lib.Cal_HttpBody(buf,len(raw),eof,decode,C.byref(ptr),C.byref(size))
        return result,C.string_at(ptr,size.value) if result==1 else b''
    for payload in (time_json,holidays_json):
        messages=[b'HTTP/1.1 200 OK\r\nContent-Length: '+str(len(payload)).encode()+b'\r\n\r\n'+payload]
        for width in (1,13,1024,len(payload)):
            chunks=[payload[i:i+width] for i in range(0,len(payload),width)]
            encoded=b''.join(f'{len(c):x};test=1\r\n'.encode()+c+b'\r\n' for c in chunks)
            messages.append(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n'+encoded+b'0\r\nX-Test: yes\r\n\r\n')
        for raw in messages:
            for length in range(len(raw)):
                assert parse_http(raw[:length])[0]==0,(length,raw[:70])
            assert parse_http(raw,decode=True)==(1,payload)
        raw=b'HTTP/1.0 200 OK\r\n\r\n'+payload
        assert parse_http(raw)[0]==0
        assert parse_http(raw,eof=True,decode=True)==(1,payload)
    for raw in (
        b'HTTP/1.1 302 Found\r\n\r\n',
        b'HTTP/1.1 2000 Invalid\r\n\r\n',
        b'HTTP/1.1 200 OK\r\nContent-Length: 9000\r\n\r\n',
        b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}',
        b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 2\r\n\r\n{}',
        b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n',
        b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n',
        b'HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n{}',
    ):
        assert parse_http(raw,eof=True)[0]==-1,raw
    exe=tmp/'navigation.exe'
    subprocess.run([args.cxx,'-std=c++11','-Wall','-Wextra','-Werror',
                    '-I'+str(root/'Components/Calendar'),'-I'+str(root/'TouchGFX/gui/include'),
                    str(root/'tests/host/test_calendar_nav.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    exe=tmp/'service.exe'
    subprocess.run([args.cc,'-std=c11','-Wall','-Wextra','-Werror',
                    *['-I'+str(root/p) for p in ('tests/calendar_fakes','App/Inc','App/Src','Components/Calendar')],
                    str(root/'tests/host/test_calendar_service.c'),str(root/'Components/Calendar/calendar_data.c'),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    if hasattr(C, 'windll'):
        import _ctypes
        _ctypes.FreeLibrary(lib._handle)
print('Calendar: dates, leap years, live JSON fixtures, malformed/truncated responses, HTTP fragmentation and navigation passed.')
