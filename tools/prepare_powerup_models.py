#!/usr/bin/env python3
"""Bake legacy modelscale into MD3 assets; existing clients need no new cgame.
Run on a copy/test installation. Only a new PK3 is written; originals are untouched.
"""
import argparse
import io
import re
import struct
import zipfile
from pathlib import Path


def scale_md3(raw: bytes, scale: float) -> bytes:
    data = bytearray(raw)
    if len(data) < 108 or data[:4] != b'IDP3':
        raise ValueError('not an MD3 model')
    version, = struct.unpack_from('<i', data, 4)
    frames, tags, surfaces, skins, ofs_frames, ofs_tags, ofs_surfaces, end = struct.unpack_from('<8i', data, 76)
    if version != 15 or end > len(data) or min(frames, tags, surfaces) < 0:
        raise ValueError('invalid MD3 header')
    # Frame bounds, local origin, radius; frame name stays untouched.
    for frame in range(frames):
        offset = ofs_frames + frame * 56
        values = struct.unpack_from('<10f', data, offset)
        struct.pack_into('<10f', data, offset, *(x * scale for x in values))
    for tag in range(frames * tags):
        offset = ofs_tags + tag * 112 + 64
        values = struct.unpack_from('<3f', data, offset)
        struct.pack_into('<3f', data, offset, *(x * scale for x in values))
    offset = ofs_surfaces
    for surface in range(surfaces):
        if data[offset:offset + 4] != b'IDP3':
            raise ValueError('invalid surface')
        flags, nframes, nshaders, nverts, ntris, tris, shaders, st, xyz, length = struct.unpack_from('<10i', data, offset + 68)
        if length < 108 or offset + length > len(data) or min(nframes, nverts) < 0:
            raise ValueError('invalid surface bounds')
        for vertex in range(nframes * nverts):
            pos = offset + xyz + vertex * 8
            if pos + 8 > offset + length:
                raise ValueError('vertex outside surface')
            values = [round(v * scale) for v in struct.unpack_from('<3h', data, pos)]
            if any(not -32768 <= v <= 32767 for v in values):
                raise ValueError('scaled vertices exceed MD3 coordinate range')
            struct.pack_into('<3h', data, pos, *values)
        offset += length
    return bytes(data)


def prepare(game_dir: Path, output: Path):
    requests = set()
    for textfile in sorted((game_dir / 'powerups').glob('*.txt*')):
        text = textfile.read_text(encoding='utf-8-sig', errors='strict')
        # Remove comments while preserving quoted strings, including // in strings.
        parts = re.findall(r'"[^"\n]*"|//[^\n]*|/\*[\s\S]*?\*/|[^/"\s]+|/', text)
        text = ' '.join(p for p in parts if not p.startswith(('//', '/*')))
        for block in re.findall(r'\{([^{}]*)\}', text):
            fields = dict(re.findall(r'"?([A-Za-z_]+)"?\s+"([^"\n]*)"', block))
            if 'modelscale' not in fields:
                continue
            scale = float(fields['modelscale'])
            if not 0.125 <= scale <= 8:
                raise ValueError('unsupported scale')
            if scale != 1:
                model = fields['model'].removesuffix('.md3') + '.md3'
                if not model.startswith('models/') or '..' in model or '\\' in model:
                    raise ValueError('invalid model path')
                requests.add((model, scale))
    archives = sorted(game_dir.glob('*.pk3'), reverse=True)
    models = {}
    for name, scale in sorted(requests):
        raw = None
        loose = game_dir / name
        if loose.is_file(): raw = loose.read_bytes()
        else:
            for archive in archives:
                if archive.resolve() == output.resolve(): continue
                try:
                    with zipfile.ZipFile(archive) as z:
                        try: raw = z.read(name)
                        except KeyError: continue
                    break
                except FileNotFoundError: continue # broken map symlinks
        if raw is None: raise FileNotFoundError(name)
        output_name = name[:-4] + f'_pu{int(scale * 1000 + 0.5)}.md3'
        models[output_name] = scale_md3(raw, scale)
    if not models:
        print('No scaled models requested; no PK3 written.')
        return
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(models.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    with zipfile.ZipFile(io.BytesIO(stream.getvalue())) as z:
        if z.testzip(): raise ValueError('PK3 verification failed')
    if output.exists():
        if output.read_bytes() == stream.getvalue(): return
        raise FileExistsError(f'{output}: use a new output filename')
    output.write_bytes(stream.getvalue())
    print(f'Wrote {len(models)} scaled model(s) to {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.game_dir, args.output)
