#!/usr/bin/env python3
"""Generate 16x16 crew member PNG sprites for the ship hub."""
import struct, zlib, os

def write_png(filename, pixels, w=16, h=16):
    """Write a 16x16 RGBA pixel array to PNG."""
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

    raw = b''
    for y in range(h):
        raw += b'\x00'  # filter byte
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)

    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')
    with open(filename, 'wb') as f:
        f.write(sig + ihdr + idat + iend)

T = (0, 0, 0, 0)  # transparent

# Color palette
BLK = (20, 20, 25, 255)       # dark outline
DKGRAY = (50, 50, 55, 255)    # dark gray
GRAY = (90, 90, 100, 255)     # medium gray
LTGRAY = (140, 140, 150, 255) # light gray
WHITE = (200, 200, 210, 255)  # white
SKIN = (200, 170, 140, 255)   # skin tone
SKIN2 = (180, 150, 120, 255)  # darker skin
GOLD = (220, 180, 50, 255)    # gold/captain
DKGOLD = (170, 130, 30, 255)  # dark gold
GREEN = (50, 180, 80, 255)    # tech green
DKGREEN = (30, 120, 50, 255)  # dark green
ORANGE = (210, 140, 40, 255)  # quartermaster orange
DKORANGE = (160, 100, 20, 255)
CYAN = (60, 200, 200, 255)    # visor cyan
RED = (200, 50, 40, 255)      # red accent
BLUE = (60, 80, 180, 255)     # blue uniform
DKBLUE = (40, 50, 120, 255)   # dark blue
TEAL = (50, 160, 140, 255)    # medical teal
MEDBLU = (80, 120, 200, 255)  # medical blue


def make_captain():
    """CPT HARDEN - Officer with cap, gold trim, blue uniform."""
    p = [T] * 256
    # Row 0-1: Cap top
    for x in range(5, 11): p[0*16+x] = DKGOLD
    for x in range(4, 12): p[1*16+x] = GOLD
    p[1*16+7] = DKGOLD; p[1*16+8] = DKGOLD  # cap detail
    # Row 2: Cap brim
    for x in range(3, 13): p[2*16+x] = BLK
    # Row 3-4: Face
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = SKIN
    for x in range(5, 11): p[4*16+x] = BLK
    for x in range(6, 10): p[4*16+x] = SKIN
    p[4*16+6] = BLK; p[4*16+9] = BLK  # eyes
    # Row 5: Chin
    for x in range(6, 10): p[5*16+x] = BLK
    for x in range(7, 9): p[5*16+x] = SKIN2
    # Row 6: Neck + collar
    for x in range(6, 10): p[6*16+x] = GOLD
    p[6*16+7] = DKGOLD; p[6*16+8] = DKGOLD
    # Row 7-9: Torso (blue uniform with gold shoulders)
    for y in range(7, 10):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = BLUE
    # Gold shoulder pads
    p[7*16+4] = GOLD; p[7*16+5] = GOLD; p[7*16+10] = GOLD; p[7*16+11] = GOLD
    p[8*16+4] = DKGOLD; p[8*16+11] = DKGOLD
    # Gold buttons
    p[8*16+7] = GOLD; p[8*16+8] = GOLD
    p[9*16+7] = GOLD; p[9*16+8] = GOLD
    # Row 10: Belt
    for x in range(5, 11): p[10*16+x] = BLK
    for x in range(6, 10): p[10*16+x] = DKGOLD
    p[10*16+7] = GOLD; p[10*16+8] = GOLD  # buckle
    # Row 11-12: Arms
    for y in range(7, 11):
        p[y*16+3] = BLUE; p[y*16+12] = BLUE
    p[10*16+3] = SKIN; p[10*16+12] = SKIN  # hands
    p[11*16+3] = SKIN; p[11*16+12] = SKIN
    # Row 11-13: Legs
    for y in range(11, 14):
        for x in range(5, 8): p[y*16+x] = DKBLUE
        for x in range(8, 11): p[y*16+x] = DKBLUE
    # Row 14-15: Boots
    for y in range(14, 16):
        for x in range(4, 8): p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    return p


def make_sergeant():
    """SGT REYES - Tech specialist, green armor, visor."""
    p = [T] * 256
    # Row 0-1: Helmet
    for x in range(5, 11): p[0*16+x] = DKGREEN
    for x in range(4, 12): p[1*16+x] = DKGREEN
    p[1*16+5] = GREEN; p[1*16+6] = GREEN; p[1*16+9] = GREEN; p[1*16+10] = GREEN
    # Row 2: Visor
    for x in range(4, 12): p[2*16+x] = BLK
    for x in range(5, 11): p[2*16+x] = CYAN
    # Row 3-4: Face under helmet
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = SKIN2
    for x in range(5, 11): p[4*16+x] = BLK
    for x in range(6, 10): p[4*16+x] = SKIN2
    p[4*16+6] = BLK; p[4*16+9] = BLK
    # Row 5: Chin
    for x in range(6, 10): p[5*16+x] = BLK
    for x in range(7, 9): p[5*16+x] = SKIN2
    # Row 6: Neck
    for x in range(7, 9): p[6*16+x] = GRAY
    # Row 7-10: Green armor torso
    for y in range(7, 11):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = DKGREEN
    # Chest plate
    for x in range(6, 10):
        p[7*16+x] = GREEN
        p[8*16+x] = GREEN
    p[8*16+7] = CYAN; p[8*16+8] = CYAN  # tech light
    # Arms
    for y in range(7, 11):
        p[y*16+3] = DKGREEN; p[y*16+12] = DKGREEN
    p[10*16+3] = SKIN2; p[10*16+12] = SKIN2
    p[11*16+3] = SKIN2; p[11*16+12] = SKIN2
    # Belt
    for x in range(5, 11): p[10*16+x] = BLK
    p[10*16+7] = GREEN; p[10*16+8] = GREEN
    # Legs
    for y in range(11, 14):
        for x in range(5, 8): p[y*16+x] = GRAY
        for x in range(8, 11): p[y*16+x] = GRAY
    # Boots
    for y in range(14, 16):
        for x in range(4, 8): p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    return p


def make_quartermaster():
    """QM CHEN - Cargo vest, orange accents, practical look."""
    p = [T] * 256
    # Row 0-2: Head (no helmet, short hair)
    for x in range(6, 10): p[0*16+x] = BLK  # hair
    for x in range(5, 11): p[1*16+x] = BLK
    for x in range(6, 10): p[1*16+x] = (60, 50, 40, 255)  # dark hair
    # Row 2-4: Face
    for x in range(5, 11): p[2*16+x] = BLK
    for x in range(6, 10): p[2*16+x] = SKIN
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = SKIN
    p[3*16+6] = BLK; p[3*16+9] = BLK  # eyes
    for x in range(5, 11): p[4*16+x] = BLK
    for x in range(6, 10): p[4*16+x] = SKIN
    # Row 5: Chin
    for x in range(6, 10): p[5*16+x] = BLK
    for x in range(7, 9): p[5*16+x] = SKIN
    # Row 6: Neck
    for x in range(7, 9): p[6*16+x] = GRAY
    # Row 7-10: Vest over shirt
    for y in range(7, 11):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = GRAY  # shirt
    # Orange vest overlay
    for y in range(7, 10):
        p[y*16+5] = ORANGE; p[y*16+6] = ORANGE
        p[y*16+9] = ORANGE; p[y*16+10] = ORANGE
    p[7*16+7] = LTGRAY; p[7*16+8] = LTGRAY  # shirt visible
    p[8*16+7] = LTGRAY; p[8*16+8] = LTGRAY
    # Pockets
    p[9*16+5] = DKORANGE; p[9*16+6] = DKORANGE
    p[9*16+9] = DKORANGE; p[9*16+10] = DKORANGE
    # Arms
    for y in range(7, 11):
        p[y*16+3] = ORANGE; p[y*16+12] = ORANGE
    p[10*16+3] = SKIN; p[10*16+12] = SKIN
    p[11*16+3] = SKIN; p[11*16+12] = SKIN
    # Belt
    for x in range(5, 11): p[10*16+x] = BLK
    p[10*16+7] = ORANGE; p[10*16+8] = DKORANGE
    # Legs (cargo pants)
    for y in range(11, 14):
        for x in range(5, 8): p[y*16+x] = DKGRAY
        for x in range(8, 11): p[y*16+x] = DKGRAY
    # Cargo pockets
    p[12*16+5] = GRAY; p[12*16+10] = GRAY
    # Boots
    for y in range(14, 16):
        for x in range(4, 8): p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    return p


def make_private():
    """PVT KOWALSKI - Casual fatigues, dog tags, relaxed pose."""
    p = [T] * 256
    # Row 0-1: Buzz cut head
    for x in range(6, 10): p[0*16+x] = (80, 70, 50, 255)  # short hair
    for x in range(5, 11): p[1*16+x] = (80, 70, 50, 255)
    # Row 2-4: Face
    for x in range(5, 11): p[2*16+x] = BLK
    for x in range(6, 10): p[2*16+x] = SKIN
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = SKIN
    p[3*16+6] = BLK; p[3*16+9] = BLK  # eyes
    for x in range(5, 11): p[4*16+x] = BLK
    for x in range(6, 10): p[4*16+x] = SKIN
    # Row 5: Chin
    for x in range(6, 10): p[5*16+x] = BLK
    for x in range(7, 9): p[5*16+x] = SKIN
    # Row 6: Neck + dog tags
    for x in range(7, 9): p[6*16+x] = LTGRAY  # dog tag chain
    # Row 7-10: Gray fatigues T-shirt
    for y in range(7, 11):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = GRAY
    # T-shirt neckline
    p[7*16+7] = LTGRAY; p[7*16+8] = LTGRAY
    # Dog tags
    p[8*16+7] = WHITE; p[8*16+8] = LTGRAY
    # Arms (bare, sleeves rolled)
    for y in range(7, 9):
        p[y*16+3] = GRAY; p[y*16+12] = GRAY
    for y in range(9, 11):
        p[y*16+3] = SKIN; p[y*16+12] = SKIN
    p[11*16+3] = SKIN; p[11*16+12] = SKIN
    # Belt
    for x in range(5, 11): p[10*16+x] = BLK
    p[10*16+7] = LTGRAY; p[10*16+8] = LTGRAY
    # Legs (camo pants)
    for y in range(11, 14):
        for x in range(5, 8): p[y*16+x] = DKGRAY
        for x in range(8, 11): p[y*16+x] = DKGRAY
    p[11*16+6] = GRAY; p[12*16+9] = GRAY  # camo spots
    # Boots
    for y in range(14, 16):
        for x in range(4, 8): p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    return p


def make_doctor():
    """DR VASQUEZ - White medical coat, teal accents, stethoscope."""
    p = [T] * 256
    # Row 0-1: Hair (pulled back)
    for x in range(5, 11): p[0*16+x] = BLK
    for x in range(5, 11): p[0*16+x] = (50, 30, 20, 255)  # dark hair
    for x in range(4, 12): p[1*16+x] = (50, 30, 20, 255)
    # Row 2-4: Face
    for x in range(5, 11): p[2*16+x] = BLK
    for x in range(6, 10): p[2*16+x] = SKIN
    for x in range(5, 11): p[3*16+x] = BLK
    for x in range(6, 10): p[3*16+x] = SKIN
    p[3*16+6] = BLK; p[3*16+9] = BLK  # eyes
    for x in range(5, 11): p[4*16+x] = BLK
    for x in range(6, 10): p[4*16+x] = SKIN
    # Row 5: Chin
    for x in range(6, 10): p[5*16+x] = BLK
    for x in range(7, 9): p[5*16+x] = SKIN
    # Row 6: Neck + stethoscope
    for x in range(7, 9): p[6*16+x] = TEAL  # stethoscope
    # Row 7-10: White coat
    for y in range(7, 11):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = WHITE
    # Coat lapels
    p[7*16+5] = TEAL; p[7*16+6] = TEAL; p[7*16+9] = TEAL; p[7*16+10] = TEAL
    # Stethoscope on chest
    p[8*16+7] = TEAL; p[8*16+8] = TEAL
    p[9*16+8] = TEAL
    # Teal undershirt visible
    p[7*16+7] = MEDBLU; p[7*16+8] = MEDBLU
    # Arms (white coat sleeves)
    for y in range(7, 11):
        p[y*16+3] = WHITE; p[y*16+12] = WHITE
    p[10*16+3] = SKIN; p[10*16+12] = SKIN
    p[11*16+3] = SKIN; p[11*16+12] = SKIN
    # Coat continues below belt (long coat)
    for y in range(11, 13):
        for x in range(4, 12): p[y*16+x] = BLK
        for x in range(5, 11): p[y*16+x] = LTGRAY
    # Legs peek out
    for y in range(13, 14):
        for x in range(5, 8): p[y*16+x] = DKBLUE
        for x in range(8, 11): p[y*16+x] = DKBLUE
    # Shoes
    for y in range(14, 16):
        for x in range(4, 8): p[y*16+x] = BLK
        for x in range(8, 12): p[y*16+x] = BLK
    return p


if __name__ == '__main__':
    outdir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sprites')

    sprites = {
        'crew_captain.png':  make_captain(),
        'crew_sergeant.png': make_sergeant(),
        'crew_quartermaster.png': make_quartermaster(),
        'crew_private.png':  make_private(),
        'crew_doctor.png':   make_doctor(),
    }

    for name, pixels in sprites.items():
        path = os.path.join(outdir, name)
        write_png(path, pixels)
        print(f'Wrote {path}')
