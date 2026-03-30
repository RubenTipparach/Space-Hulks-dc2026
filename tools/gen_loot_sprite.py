#!/usr/bin/env python3
"""Generate 16x16 loot chest PNG sprite."""
import struct, zlib, os

def write_png(filename, pixels, w=16, h=16):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')
    with open(filename, 'wb') as f:
        f.write(sig + ihdr + idat + iend)

T = (0, 0, 0, 0)
BLK = (20, 20, 25, 255)
WOOD = (120, 80, 40, 255)
DKWOOD = (80, 50, 25, 255)
LTWOOD = (160, 110, 60, 255)
GOLD = (220, 180, 50, 255)
DKGOLD = (170, 130, 30, 255)
HINGE = (140, 140, 150, 255)

def make_chest():
    """Treasure chest - front-facing, slightly open lid."""
    p = [T] * 256

    # Row 0-3: Lid (slightly raised/open)
    # Top of lid
    for x in range(4, 12): p[2*16+x] = BLK
    for x in range(5, 11): p[2*16+x] = DKWOOD
    # Lid front face
    for x in range(4, 12): p[3*16+x] = BLK
    for x in range(5, 11): p[3*16+x] = WOOD
    for x in range(4, 12): p[4*16+x] = BLK
    for x in range(5, 11): p[4*16+x] = WOOD
    # Lid trim
    p[3*16+5] = DKGOLD; p[3*16+10] = DKGOLD
    p[4*16+5] = DKGOLD; p[4*16+10] = DKGOLD
    # Lid edge
    for x in range(4, 12): p[5*16+x] = BLK
    for x in range(5, 11): p[5*16+x] = DKWOOD

    # Row 5-6: Gap showing golden glow (chest is open)
    for x in range(5, 11): p[6*16+x] = GOLD
    for x in range(6, 10): p[6*16+x] = (255, 220, 80, 255)  # bright glow

    # Row 7-12: Chest body
    for y in range(7, 13):
        for x in range(3, 13): p[y*16+x] = BLK
        for x in range(4, 12): p[y*16+x] = WOOD

    # Wood grain / planks
    for y in range(8, 12):
        p[y*16+7] = DKWOOD  # center plank line
        p[y*16+8] = DKWOOD

    # Metal bands
    for x in range(4, 12):
        p[7*16+x] = DKGOLD   # top band
        p[10*16+x] = DKGOLD  # middle band
        p[12*16+x] = DKGOLD  # bottom band

    # Lock/clasp in center
    p[8*16+7] = GOLD; p[8*16+8] = GOLD
    p[9*16+7] = GOLD; p[9*16+8] = GOLD
    p[9*16+7] = DKGOLD  # keyhole

    # Metal corner reinforcements
    p[7*16+4] = HINGE; p[7*16+11] = HINGE
    p[12*16+4] = HINGE; p[12*16+11] = HINGE

    # Row 13: Base/shadow
    for x in range(3, 13): p[13*16+x] = BLK

    # Row 14-15: Floor shadow
    for x in range(4, 12): p[14*16+x] = (15, 15, 18, 200)

    return p

if __name__ == '__main__':
    outdir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sprites')
    path = os.path.join(outdir, 'loot_chest.png')
    write_png(path, make_chest())
    print(f'Wrote {path}')
