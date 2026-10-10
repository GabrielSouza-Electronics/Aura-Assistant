"""Check speech asset fidelity, silence analysis, frame boundaries and lifecycle."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import wave
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='gcc')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('gen_chat_speech', root / 'aura_assets/gen/gen_chat_speech.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
with wave.open(str(root / 'aura_assets/audio/chat_test.wav')) as wav:
    assert (wav.getframerate(), wav.getnchannels(), wav.getsampwidth()) == (48000, 1, 2)
    pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype='<i2')
source = (root / 'Components/Audio/chat_test_audio.c').read_text().split('{', 1)[1].split('}', 1)[0]
assert np.array_equal(np.array(list(map(int, re.findall(r'-?\d+', source))), dtype=np.int16), pcm)
cues, _, _ = generator.analyze(pcm, 48000)
installed = (root / 'Components/Audio/chat_test_timeline.h').read_text()
for cue in cues:
    assert '{' + ','.join(str(v) + 'U' for v in cue.values()) + '}' in installed
assert {(c['first_frame'], c['last_frame']) for c in cues} == {(9, 14), (14, 23), (23, 37)}
assert generator.analyze(np.zeros(48000, dtype=np.int16), 48000)[0] == []
noise = np.tile(np.array([-30, 30], dtype=np.int16), 24000)
assert generator.analyze(noise, 48000)[0] == []
# A short spike is not a word; 40 ms or more is eligible for a voiced span.
noise[12000:12480] = 8000
assert generator.analyze(noise, 48000)[0] == []
print('PASS: PCM fidelity, reproducible cues, all speech ranges, silence/noise rejection')
with tempfile.TemporaryDirectory(prefix='aura_speech_') as directory:
    exe = Path(directory) / 'speech.exe'
    subprocess.run([args.compiler, '-x', 'c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'TouchGFX/gui/include'), '-I' + str(root / 'Components/Audio'),
                    str(root / 'tests/host/test_speech_animation.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
