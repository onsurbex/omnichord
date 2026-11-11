#!/usr/bin/env python3
"""Regenerate the measured GraySlate catalog (development only; requires numpy).

Original WAVs remain untouched. The build uses the committed JSON, not numpy.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import numpy as np

base = Path(__file__).resolve().parents[1] / "samples/GraySlate's Omnichord Pack"
entries = []
pitch_classes = dict(c=0, db=1, d=2, eb=3, e=4, f=5, g=7, ab=8, a=9, bb=10, b=11)
pitch_classes['f#'] = 6
for path in sorted(base.rglob('*.wav'), key=lambda p: p.as_posix().lower()):
    blob = path.read_bytes()
    offset = 12
    fmt = data = None
    while offset + 8 <= len(blob):
        tag, size = struct.unpack_from('<4sI', blob, offset)
        offset += 8
        if tag == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', blob, offset)
        if tag == b'data':
            data = blob[offset:offset + size]
        offset += size + size % 2
    code, channels, rate, _, _, bits = fmt
    assert (code, bits) in ((1, 16), (3, 32), (3, 64))
    dtype = '<i2' if code == 1 else ('<f4' if bits == 32 else '<f8')
    audio = np.frombuffer(data, dtype=dtype).reshape(-1, channels)
    seconds = len(audio) / rate
    audio = audio[:rate * 2].astype(float)
    if code == 1:
        audio /= 32768
    assert np.isfinite(audio).all()
    mono = audio.mean(axis=1)[::8]
    spectrum = np.abs(np.fft.rfft((mono - mono.mean()) * np.hanning(len(mono))))
    frequencies = np.fft.rfftfreq(len(mono), 8 / rate)
    spectrum[(frequencies < 70) | (frequencies > 6000)] = 0
    frequency = frequencies[int(spectrum.argmax())]
    root = int(round(69 + 12 * np.log2(frequency / 440)))
    chord = path.stem.endswith('-chord')
    rhythm = path.parent.name == 'rhythm'
    group = path.parent.name
    if chord:
        root = 48 + pitch_classes[re.sub(r'(m|7)$', '', group)]
    if rhythm:
        root = 60
    rms = float(np.sqrt(np.mean(audio * audio)))
    peak = float(np.max(np.abs(audio)))
    gain = min(6., max(.35, (.075 if chord else .04) / max(rms, .00001)), .95 / max(peak, .00001))
    label = 'Chord' if chord else ('Rhythm ' + path.stem if rhythm else 'Harp ' + path.stem[len(group):].zfill(2))
    entries.append(dict(file=path.relative_to(base).as_posix(), name='GraySlate / ' + group.upper() + ' / ' + label,
                        root=root, gain=round(gain, 5), wholeChord=chord or rhythm, loop=chord,
                        seconds=round(seconds, 5), bits=bits, channels=channels, sha256=hashlib.sha256(blob).hexdigest()))
catalog = base.parent / 'grayslate-index.json'
catalog.write_text(json.dumps(dict(format=1, source=base.name, entries=entries), indent=2) + '\n')
print(f'{len(entries)} recordings indexed; longest {max(e["seconds"] for e in entries):.2f}s')
