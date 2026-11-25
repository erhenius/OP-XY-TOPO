"""Regenerate Vowel-256, a synthetic 32-cycle bank with moving formants."""
from math import cos, exp, pi, sin
from pathlib import Path
import struct
import wave

target = Path(__file__).resolve().parents[1] / "examples/Vowel-256.wav"
pcm = bytearray()
for row in range(32):
    t = row / 31
    center = 3 + 9 * (1 - cos(pi * t)) / 2
    amplitudes = [
        (1 / h ** 1.3 + 0.55 * exp(-0.5 * ((h - center) / 1.45) ** 2)
         + 0.19 * exp(-0.5 * ((h - (19 - 6 * t)) / 2.1) ** 2))
        for h in range(1, 25)
    ]
    cycle = [sum(a * sin(2 * pi * h * x / 256)
                 for h, a in enumerate(amplitudes, 1)) for x in range(256)]
    scale = 26000 / max(abs(v) for v in cycle)
    pcm.extend(struct.pack("<256h", *(round(v * scale) for v in cycle)))
with wave.open(str(target), "wb") as wav:
    wav.setparams((1, 2, 44100, 0, "NONE", "not compressed"))
    wav.writeframes(pcm)
