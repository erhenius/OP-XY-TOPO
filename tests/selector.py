"""Compile the shipped native adapter with stock calls redirected to the fixture."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sdk = Path(sys.argv[1]).resolve()
tests = Path(__file__).resolve().parent
platform = sdk / 'sdk/platform/opxy-1.2.1'
with tempfile.TemporaryDirectory() as tmp:
    top = Path(tmp)
    shutil.copy(platform/'userwt_selector.cpp', top)
    (top/'stock.hpp').write_text("""
#pragma once
#include <opxy/types.hpp>
namespace opxy { namespace platform {
using Address=__UINTPTR_TYPE__;
const u32 kSelectedTrack=0x840a0534, kScreen=0x8400b9d0, kTracks=8, kSampleBrowserScreen=4;
u32 byte(u32); u32 word(u32); u32 track_engine(u32); Address target(u32);
template<class F> F* stock(u32 a) {return reinterpret_cast<F*>(target(a));}
}}
""")
    cmd=['c++','-std=c++11','-O2','-Wall','-Wextra','-Werror','-fno-strict-aliasing',
         '-D__builtin_bfin_ssync()=__atomic_thread_fence(__ATOMIC_SEQ_CST)',
         '-I'+str(top),'-I'+str(sdk/'sdk/include'),'-I'+str(platform),
         str(tests/'selector.cpp'),'-o',str(top/'selector')]
    subprocess.run(cmd,check=True)
    subprocess.run([str(top/'selector')],check=True)
