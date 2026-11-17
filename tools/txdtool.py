#!/usr/bin/env python3
"""txdtool - baca TXD RenderWare (D3D8/D3D9) : list & export ke PNG.
Pakai:  python3 txdtool.py list  file.txd
        python3 txdtool.py export file.txd folder_output [nama_texture ...]
"""
import sys, struct, zlib, os

def chunks(b, p, end):
    while p + 12 <= end:
        t, s, v = struct.unpack_from('<III', b, p)
        yield t, p + 12, s
        p += 12 + s

def parse_txd(b):
    t, s, v = struct.unpack_from('<III', b, 0)
    assert t == 0x16, 'bukan TXD'
    texs = []
    for t, p, s in chunks(b, 12, 12 + s):
        if t == 0x15:  # texture native
            texs.append(parse_native(b, p, s))
    return texs

def parse_native(b, p, size):
    # struct chunk
    t, s, v = struct.unpack_from('<III', b, p)
    q = p + 12
    plat, filt, = struct.unpack_from('<I', b, q)[0], 0
    flags_filter = struct.unpack_from('<I', b, q + 4)[0]
    name = b[q + 8:q + 40].split(b'\0')[0].decode('latin1')
    mask = b[q + 40:q + 72].split(b'\0')[0].decode('latin1')
    raster, = struct.unpack_from('<I', b, q + 72)
    info = {'name': name, 'mask': mask, 'platform': plat}
    if plat == 9 or plat == 8:  # D3D9 / D3D8
        d3dfmt, = struct.unpack_from('<I', b, q + 76) if plat == 9 else (0,)
        if plat == 9:
            w, h, depth, mips, rtype, alpha_cube_comp = struct.unpack_from('<HHBBBB', b, q + 80)
            off = q + 88
            fourcc = struct.unpack_from('<I', b, q + 76)[0]
            info.update(w=w, h=h, depth=depth, mips=mips, raster=raster, d3dfmt=d3dfmt, comp=alpha_cube_comp)
            pal = None
            if raster & 0x2000: pal_n = 256; 
            elif raster & 0x4000: pal_n = 32
            else: pal_n = 0
            if pal_n:
                pal = b[off:off + pal_n * 4]; off += pal_n * 4
            lvl0, = struct.unpack_from('<I', b, off)
            data = b[off + 4: off + 4 + lvl0]
            info.update(data=data, pal=pal)
    else:
        info.update(w=0, h=0, mips=0, unsupported=True)
    return info

def fmt_name(i):
    f = i.get('d3dfmt')
    if f in (0x31545844, 0x33545844, 0x35545844):
        return {0x31545844: 'DXT1', 0x33545844: 'DXT3', 0x35545844: 'DXT5'}[f]
    r = i['raster'] & 0xF00
    return {0x100: '1555', 0x200: '565', 0x300: '4444', 0x400: 'LUM8', 0x500: '8888', 0x600: '888', 0x700: '555'}.get(r, 'fmt?%x' % i['raster']) + ('+PAL8' if i['raster'] & 0x2000 else '') + ('+PAL4' if i['raster'] & 0x4000 else '')

def rgb565(v):
    return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)

def decode_dxt(data, w, h, kind):
    out = bytearray(w * h * 4)
    bw, bh = (w + 3) // 4, (h + 3) // 4
    bs = 8 if kind == 1 else 16
    for by in range(bh):
        for bx in range(bw):
            blk = data[(by * bw + bx) * bs:(by * bw + bx + 1) * bs]
            if len(blk) < bs: continue
            cb = blk[-8:]
            c0, c1, bits = struct.unpack('<HHI', cb)
            p0, p1 = rgb565(c0), rgb565(c1)
            if c0 > c1 or kind != 1:
                cols = [p0 + (255,), p1 + (255,),
                        tuple((2 * a + b) // 3 for a, b in zip(p0, p1)) + (255,),
                        tuple((a + 2 * b) // 3 for a, b in zip(p0, p1)) + (255,)]
            else:
                cols = [p0 + (255,), p1 + (255,), tuple((a + b) // 2 for a, b in zip(p0, p1)) + (255,), (0, 0, 0, 0)]
            alphas = None
            if kind == 3:
                ab = struct.unpack('<Q', blk[:8])[0]
                alphas = [((ab >> (4 * i)) & 15) * 17 for i in range(16)]
            elif kind == 5:
                a0, a1 = blk[0], blk[1]
                ab = int.from_bytes(blk[2:8], 'little')
                if a0 > a1: al = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else: al = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                alphas = [al[(ab >> (3 * i)) & 7] for i in range(16)]
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x >= w or y >= h: continue
                c = cols[(bits >> (2 * i)) & 3]
                a = alphas[i] if alphas else c[3]
                o = (y * w + x) * 4
                out[o:o + 4] = bytes((c[0], c[1], c[2], a))
    return bytes(out)

def to_rgba(i):
    w, h, d = i['w'], i['h'], i['data']
    f = i.get('d3dfmt')
    if f == 0x31545844: return decode_dxt(d, w, h, 1)
    if f == 0x33545844: return decode_dxt(d, w, h, 3)
    if f == 0x35545844: return decode_dxt(d, w, h, 5)
    r = i['raster'] & 0xF00
    out = bytearray(w * h * 4)
    if i['raster'] & 0x2000 and i['pal']:
        pal = i['pal']
        for n in range(w * h):
            k = d[n] * 4; out[n*4:n*4+4] = bytes((pal[k+2], pal[k+1], pal[k], pal[k+3]))
        return bytes(out)
    if r == 0x500:  # BGRA
        for n in range(w * h):
            b_, g, r_, a = d[n*4:n*4+4]; out[n*4:n*4+4] = bytes((r_, g, b_, a))
    elif r == 0x600:
        for n in range(w * h):
            b_, g, r_ = d[n*4:n*4+3]; out[n*4:n*4+4] = bytes((r_, g, b_, 255))
    else:
        raise ValueError('format belum didukung: ' + fmt_name(i))
    return bytes(out)

def write_png(path, w, h, rgba):
    raw = b''.join(b'\0' + rgba[y*w*4:(y+1)*w*4] for y in range(h))
    def ch(t, d): c = struct.pack('>I', len(d)) + t + d; return c + struct.pack('>I', zlib.crc32(t + d))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))

if __name__ == '__main__':
    cmd, path = sys.argv[1], sys.argv[2]
    texs = parse_txd(open(path, 'rb').read())
    if cmd == 'list':
        print('%d texture di %s' % (len(texs), path))
        for t in texs:
            print('%-24s %5dx%-5d mips=%d %s' % (t['name'], t['w'], t['h'], t['mips'], fmt_name(t) if 'raster' in t else 'unsupported(platform %d)' % t['platform']))
    elif cmd == 'export':
        out = sys.argv[3]; want = set(a.lower() for a in sys.argv[4:]); os.makedirs(out, exist_ok=True)
        for t in texs:
            if want and t['name'].lower() not in want: continue
            try: write_png(os.path.join(out, t['name'] + '.png'), t['w'], t['h'], to_rgba(t)); print('ok', t['name'])
            except Exception as e: print('gagal', t['name'], e)
