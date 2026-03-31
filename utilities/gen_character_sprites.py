#!/usr/bin/env python3
"""Generate 16x16 character PNG sprites for Engineer and Scientist classes."""
import struct, zlib, os

def write_png(filename, pixels, w=16, h=16):
    """Write a 16x16 RGBA pixel array to PNG."""
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

# Color palette (matching sr_sprites.h ABGR -> RGBA)
BLK  = (20, 20, 25, 255)
DKGRAY = (51, 51, 51, 255)     # _DG
GRAY   = (102, 102, 102, 255)  # _MG
LTGRAY = (170, 170, 170, 255)  # _LG
WHITE  = (255, 255, 255, 255)  # _WH
ORANGE = (238, 136, 68, 255)   # _OR
DKYEL  = (170, 170, 34, 255)   # _Y1
YELLOW = (221, 221, 68, 255)   # _Y2
LTBLUE = (68, 119, 238, 255)   # _B3
SKIN   = (200, 170, 140, 255)


def make_engineer():
    """Engineer - bulky armor, orange visor, welding tool look."""
    p = [T] * 256

    # Row 0-1: Heavy helmet top
    for x in range(5, 11): p[0*16+x] = DKGRAY
    for x in range(4, 12): p[1*16+x] = DKGRAY
    for x in range(5, 11): p[1*16+x] = GRAY

    # Row 2: Orange visor
    for x in range(4, 12): p[2*16+x] = BLK
    for x in range(5, 11): p[2*16+x] = ORANGE

    # Row 3-4: Lower helmet / face guard
    for x in range(4, 12): p[3*16+x] = BLK
    for x in range(5, 11): p[3*16+x] = GRAY
    for x in range(5, 11): p[4*16+x] = DKGRAY
    p[4*16+7] = GRAY; p[4*16+8] = GRAY  # mouth grille

    # Row 5: Neck
    for x in range(6, 10): p[5*16+x] = DKGRAY

    # Row 6-9: Heavy torso (yellow-orange work armor)
    for y in range(6, 10):
        for x in range(3, 13): p[y*16+x] = BLK
        for x in range(4, 12): p[y*16+x] = DKYEL
    # Shoulder pads (wide)
    p[6*16+3] = ORANGE; p[6*16+4] = ORANGE; p[6*16+11] = ORANGE; p[6*16+12] = ORANGE
    p[7*16+3] = DKYEL;  p[7*16+12] = DKYEL

    # Chest harness
    p[7*16+7] = ORANGE; p[7*16+8] = ORANGE
    p[8*16+6] = ORANGE; p[8*16+9] = ORANGE
    p[9*16+7] = ORANGE; p[9*16+8] = ORANGE

    # Arms (heavy gloves)
    for y in range(7, 10):
        p[y*16+2] = DKYEL; p[y*16+13] = DKYEL
    p[9*16+2] = ORANGE; p[9*16+13] = ORANGE  # welding gloves
    p[10*16+2] = ORANGE; p[10*16+13] = ORANGE

    # Row 10: Utility belt
    for x in range(4, 12): p[10*16+x] = BLK
    for x in range(5, 11): p[10*16+x] = DKGRAY
    p[10*16+7] = YELLOW; p[10*16+8] = YELLOW  # buckle

    # Row 11-13: Legs (heavy work pants)
    for y in range(11, 14):
        for x in range(5, 8): p[y*16+x] = DKGRAY
        for x in range(8, 11): p[y*16+x] = DKGRAY

    # Knee pads
    p[12*16+5] = ORANGE; p[12*16+10] = ORANGE

    # Row 14-15: Heavy boots
    for y in range(14, 16):
        for x in range(4, 8):  p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    for x in range(4, 8):  p[14*16+x] = DKGRAY
    for x in range(8, 12): p[14*16+x] = DKGRAY

    return p


def make_scientist():
    """Scientist - sleek white suit, cyan visor, lab coat feel."""
    p = [T] * 256

    # Row 0-1: Sleek helmet
    for x in range(6, 10): p[0*16+x] = LTGRAY
    for x in range(5, 11): p[1*16+x] = LTGRAY
    for x in range(6, 10): p[1*16+x] = WHITE

    # Row 2: Cyan visor
    for x in range(5, 11): p[2*16+x] = BLK
    for x in range(6, 10): p[2*16+x] = LTBLUE

    # Row 3-4: Lower helmet
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = LTGRAY
    for x in range(5, 11): p[4*16+x] = LTGRAY
    p[4*16+7] = WHITE; p[4*16+8] = WHITE

    # Row 5: Neck
    for x in range(6, 10): p[5*16+x] = DKGRAY

    # Row 6-9: Sleek white lab-coat armor
    for y in range(6, 10):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = WHITE
    # Shoulder trim
    p[6*16+4] = LTGRAY; p[6*16+5] = LTGRAY; p[6*16+10] = LTGRAY; p[6*16+11] = LTGRAY
    # Chest display (cyan tech)
    p[7*16+7] = LTBLUE; p[7*16+8] = LTBLUE
    p[8*16+7] = LTBLUE; p[8*16+8] = LTBLUE
    p[8*16+6] = (40, 80, 160, 255)  # side panel
    p[8*16+9] = (40, 80, 160, 255)
    # Lapel lines
    for y in range(7, 10):
        p[y*16+5] = LTGRAY; p[y*16+10] = LTGRAY

    # Arms (slim white sleeves)
    for y in range(7, 10):
        p[y*16+3] = WHITE; p[y*16+12] = WHITE
    p[9*16+3] = LTGRAY; p[9*16+12] = LTGRAY
    p[10*16+3] = SKIN; p[10*16+12] = SKIN  # hands

    # Row 10: Belt
    for x in range(5, 11): p[10*16+x] = BLK
    for x in range(6, 10): p[10*16+x] = DKGRAY
    p[10*16+7] = LTBLUE; p[10*16+8] = LTBLUE  # tech buckle

    # Row 11-12: Coat tails (long coat extends)
    for y in range(11, 13):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = LTGRAY

    # Row 13: Legs peek
    for x in range(5, 8): p[13*16+x] = DKGRAY
    for x in range(8, 11): p[13*16+x] = DKGRAY

    # Row 14-15: Boots
    for y in range(14, 16):
        for x in range(4, 8):  p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK

    return p


if __name__ == '__main__':
    outdir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sprites')

    sprites = {
        'engineer.png':  make_engineer(),
        'scientist.png': make_scientist(),
    }

    for name, pixels in sprites.items():
        path = os.path.join(outdir, name)
        write_png(path, pixels)
        print(f'Wrote {path}')
