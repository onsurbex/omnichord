#!/usr/bin/env python3
"""Package verified original recordings losslessly, without generating C++ audio arrays."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

def build(source, catalog, output):
    source, catalog, output = map(Path, (source, catalog, output))
    entries = json.loads(catalog.read_text())['entries']
    files = [source / entry['file'] for entry in entries]
    if output.exists() and output.stat().st_mtime >= max(p.stat().st_mtime for p in [catalog, Path(__file__), *files]):
        return
    temporary = output.with_suffix('.tmp')
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for entry, path in zip(entries, files):
                if not path.resolve().is_relative_to(source.resolve()):
                    raise ValueError('Recording outside the sample bank')
                data = path.read_bytes()
                if hashlib.sha256(data).hexdigest() != entry['sha256']:
                    raise ValueError(f'Recording does not match catalog: {path}')
                archive.writestr(source.name + '/' + entry['file'], data)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f'Packaged {len(entries)} original WAVs: {output.stat().st_size} bytes')

if __name__ == '__main__':
    build(*sys.argv[1:])
