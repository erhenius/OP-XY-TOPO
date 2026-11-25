"""Run with python3 tests/offline.py /path/to/open-xy, no network or device."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(SDK))
from tooling import machines, native
from tooling.pipeline import config

def run(cmd):
    subprocess.run(cmd, check=True)

def main(tmp):
    assigned, _ = machines.assign_numbers([machines.machine_at(ROOT / 'userwt')])
    assets = native.prepare_assets((SDK / 'profiles/opxy-1.2.1/content/op1_factory.json').read_bytes(),
                                   config(), assigned)
    for path, data in assets.items():
        dest = tmp / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    run(['c++', '-std=c++11', '-O2', '-Wall', '-Werror', '-fno-strict-aliasing',
         '-I'+str(ROOT/'userwt'), '-I'+str(SDK/'sdk/include'),
         '-I'+str(SDK/'sdk/platform/opxy-1.2.1'), str(ROOT/'tests/render.cpp'), '-o', str(tmp/'render')])
    def render(file, width=2048, position=0, warp=0, drift=0):
        with wave.open(str(file),'rb') as w:
            assert w.getsampwidth()==2 and w.getframerate()==44100
            channels = w.getnchannels()
            pcm = w.readframes(w.getnframes())
        (tmp/'in.raw').write_bytes(pcm)
        run([str(tmp/'render'), str(tmp/'in.raw'), str(channels), str(width), str(position),
             str(warp), str(drift), str(tmp/'out.raw')])
        raw=(tmp/'out.raw').read_bytes()
        return struct.unpack('<'+'i'*(len(raw)//4),raw), struct.unpack('<'+'h'*(len(pcm)//2),pcm)[::channels]
    run(['c++','-std=c++11','-O2','-Wall','-Werror',
         '-D__builtin_bfin_ssync()=__atomic_thread_fence(__ATOMIC_SEQ_CST)',
         '-I'+str(ROOT/'userwt'), '-I'+str(SDK/'sdk/include'), '-I'+str(SDK/'sdk/platform/opxy-1.2.1'),
         str(ROOT/'tests/surface.cpp'), '-o', str(tmp/'trace')])
    run([str(tmp/'trace')])
    banks=tmp/'content/samples/userwt'
    last=2**31-1
    endings=[]
    for name in ('glass','reed'):
        path=banks/(name+'.wav')
        for pos, index in ((0,0),(last,7)):
            out,pcm=render(path,position=pos)
            assert out == tuple(v*32768 for v in pcm[index*2048:(index+1)*2048])*2
        endings.append(out)
        mid,_=render(path,position=1<<30)
        assert mid != out
        warped,_=render(path,warp=last)
        drifted,_=render(path,drift=last)
        dry,_=render(path)
        assert dry != warped and dry != drifted
    assert endings[0] != endings[1]
    # A new unlisted stereo file, introduced after the SDK has prepared all build assets.
    for width in (256,512,1024,2048):
        path = banks/'after-build.wav'
        with wave.open(str(path),'wb') as w:
            w.setparams((2,2,44100,0,'NONE','not compressed'))
            w.writeframes(b''.join(struct.pack('<hh',t,-t) for t in range(width)))
        out, pcm=render(path,width)
        assert out==tuple(v*32768 for v in pcm)*2
    with wave.open(str(path),'wb') as w:
        w.setparams((1,2,44100,0,'NONE','not compressed'))
        w.writeframes(struct.pack('<h',1000)*2047)
    assert not any(render(path)[0])
    print('PASS: SDK prepared two banks; both render bit-exact at both scan endpoints.')
    print('PASS: position, warp and drift change the output. New stereo file, all four cycle lengths, malformed length.')
if __name__=='__main__':
    with tempfile.TemporaryDirectory() as d:
        main(Path(d))
