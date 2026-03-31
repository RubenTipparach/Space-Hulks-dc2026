#!/usr/bin/env python3
"""Generate 16x16 card art PNG sprites for all combat card types.
Each sprite is a small iconic representation of the card's effect."""
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

# Shared palette
BLK    = (20, 20, 25, 255)
DKGRAY = (51, 51, 51, 255)
GRAY   = (102, 102, 102, 255)
LTGRAY = (170, 170, 170, 255)
WHITE  = (255, 255, 255, 255)

# Card-specific colors
BLUE     = (34, 136, 204, 255)     # shield
DKBLUE   = (20, 80, 140, 255)
RED      = (204, 50, 50, 255)      # shoot / fire
DKRED    = (140, 30, 30, 255)
ORANGE   = (238, 136, 34, 255)     # burst / fire
GREEN    = (34, 204, 34, 255)      # move
DKGREEN  = (20, 140, 20, 255)
YELLOW   = (221, 221, 34, 255)     # melee / overcharge
CYAN     = (34, 204, 204, 255)     # stun / laser
TEAL     = (34, 200, 136, 255)     # repair
MAGENTA  = (204, 34, 204, 255)     # stun
LTBLUE   = (100, 180, 255, 255)    # ice
ACIDGRN  = (34, 204, 170, 255)     # acid
FIRE_ORG = (255, 100, 20, 255)     # fire
LIGHTNING= (255, 255, 68, 255)     # lightning
STEEL    = (160, 170, 180, 255)    # metal
DKSTEEL  = (100, 110, 120, 255)
PURPLE   = (140, 80, 200, 255)     # stun gun


def px_set(p, x, y, c):
    """Safe pixel set."""
    if 0 <= x < 16 and 0 <= y < 16:
        p[y * 16 + x] = c

def hline(p, x1, x2, y, c):
    for x in range(x1, x2 + 1): px_set(p, x, y, c)

def vline(p, x, y1, y2, c):
    for y in range(y1, y2 + 1): px_set(p, x, y, c)

def rect_fill(p, x1, y1, x2, y2, c):
    for y in range(y1, y2 + 1):
        for x in range(x1, x2 + 1): px_set(p, x, y, c)

def rect_outline(p, x1, y1, x2, y2, c):
    hline(p, x1, x2, y1, c)
    hline(p, x1, x2, y2, c)
    vline(p, x1, y1, y2, c)
    vline(p, x2, y1, y2, c)


# ── Card art generators ─────────────────────────────────────────

def make_shield():
    """Shield icon - rounded shield shape."""
    p = [T] * 256
    # Shield outline
    hline(p, 5, 10, 2, BLUE)
    for y in range(3, 6):
        hline(p, 4, 11, y, BLUE)
    hline(p, 5, 10, 6, BLUE)
    hline(p, 5, 10, 7, DKBLUE)
    hline(p, 6, 9, 8, DKBLUE)
    hline(p, 6, 9, 9, DKBLUE)
    hline(p, 7, 8, 10, DKBLUE)
    hline(p, 7, 8, 11, DKBLUE)
    px_set(p, 7, 12, DKBLUE)
    px_set(p, 8, 12, DKBLUE)
    # Inner highlight
    rect_fill(p, 6, 4, 9, 5, (50, 160, 240, 255))
    hline(p, 7, 8, 6, (50, 160, 240, 255))
    # +3 accent
    px_set(p, 7, 4, WHITE); px_set(p, 8, 4, WHITE)
    px_set(p, 7, 5, WHITE)
    return p


def make_shoot():
    """Shoot - crosshair / bullet."""
    p = [T] * 256
    # Crosshair circle
    hline(p, 6, 9, 3, RED)
    hline(p, 6, 9, 12, RED)
    vline(p, 3, 6, 9, RED)
    vline(p, 12, 6, 9, RED)
    # Cross
    vline(p, 7, 1, 14, DKRED)
    vline(p, 8, 1, 14, DKRED)
    hline(p, 1, 14, 7, DKRED)
    hline(p, 1, 14, 8, DKRED)
    # Center dot
    rect_fill(p, 7, 7, 8, 8, RED)
    return p


def make_burst():
    """Burst - explosion / radial blast."""
    p = [T] * 256
    # Central explosion
    rect_fill(p, 6, 6, 9, 9, ORANGE)
    rect_fill(p, 7, 7, 8, 8, YELLOW)
    # Rays outward
    for d in range(1, 5):
        px_set(p, 7 - d, 7 - d, RED)      # top-left
        px_set(p, 8 + d, 7 - d, RED)      # top-right
        px_set(p, 7 - d, 8 + d, RED)      # bottom-left
        px_set(p, 8 + d, 8 + d, RED)      # bottom-right
    px_set(p, 7, 3, ORANGE); px_set(p, 8, 3, ORANGE)  # up
    px_set(p, 7, 12, ORANGE); px_set(p, 8, 12, ORANGE)  # down
    px_set(p, 3, 7, ORANGE); px_set(p, 3, 8, ORANGE)  # left
    px_set(p, 12, 7, ORANGE); px_set(p, 12, 8, ORANGE)  # right
    return p


def make_move():
    """Move - arrow pointing forward/right."""
    p = [T] * 256
    # Arrow shaft
    hline(p, 3, 10, 7, GREEN)
    hline(p, 3, 10, 8, GREEN)
    # Arrow head
    px_set(p, 11, 7, GREEN); px_set(p, 11, 8, GREEN)
    px_set(p, 12, 7, GREEN); px_set(p, 12, 8, GREEN)
    px_set(p, 10, 5, GREEN); px_set(p, 11, 6, GREEN); px_set(p, 12, 7, (100, 255, 100, 255))
    px_set(p, 10, 10, GREEN); px_set(p, 11, 9, GREEN); px_set(p, 12, 8, (100, 255, 100, 255))
    px_set(p, 13, 7, (100, 255, 100, 255)); px_set(p, 13, 8, (100, 255, 100, 255))
    # Speed lines
    hline(p, 2, 5, 5, DKGREEN)
    hline(p, 1, 4, 10, DKGREEN)
    return p


def make_melee():
    """Melee - fist / punch icon."""
    p = [T] * 256
    # Fist
    rect_fill(p, 5, 5, 10, 10, YELLOW)
    rect_fill(p, 6, 6, 9, 9, (255, 255, 100, 255))
    # Knuckle bumps
    hline(p, 5, 10, 4, YELLOW)
    px_set(p, 5, 3, YELLOW); px_set(p, 7, 3, YELLOW); px_set(p, 9, 3, YELLOW)
    # Thumb
    rect_fill(p, 10, 7, 11, 9, YELLOW)
    # Impact lines
    px_set(p, 3, 3, WHITE); px_set(p, 2, 5, WHITE); px_set(p, 3, 7, WHITE)
    px_set(p, 13, 4, WHITE); px_set(p, 13, 8, WHITE)
    return p


def make_overcharge():
    """Overcharge - energy battery glowing."""
    p = [T] * 256
    # Battery body
    rect_outline(p, 5, 4, 10, 13, DKGRAY)
    rect_fill(p, 6, 5, 9, 12, GRAY)
    # Battery top terminal
    rect_fill(p, 6, 2, 9, 3, DKGRAY)
    # Energy fill (cyan glow)
    rect_fill(p, 6, 6, 9, 12, CYAN)
    rect_fill(p, 6, 6, 9, 8, (100, 255, 255, 255))  # bright top
    # Lightning bolt
    px_set(p, 8, 6, YELLOW); px_set(p, 7, 7, YELLOW)
    px_set(p, 8, 8, YELLOW); px_set(p, 7, 9, YELLOW)
    return p


def make_repair():
    """Repair - wrench / medical cross hybrid."""
    p = [T] * 256
    # Cross shape (healing)
    rect_fill(p, 6, 3, 9, 12, TEAL)
    rect_fill(p, 3, 6, 12, 9, TEAL)
    # Inner highlight
    rect_fill(p, 7, 5, 8, 10, (60, 240, 170, 255))
    rect_fill(p, 5, 7, 10, 8, (60, 240, 170, 255))
    # Center
    rect_fill(p, 7, 7, 8, 8, WHITE)
    return p


def make_stun():
    """Stun - concentric rings / shockwave."""
    p = [T] * 256
    # Outer ring
    hline(p, 5, 10, 2, MAGENTA); hline(p, 5, 10, 13, MAGENTA)
    vline(p, 2, 5, 10, MAGENTA); vline(p, 13, 5, 10, MAGENTA)
    px_set(p, 3, 3, MAGENTA); px_set(p, 3, 12, MAGENTA)
    px_set(p, 12, 3, MAGENTA); px_set(p, 12, 12, MAGENTA)
    # Inner ring
    hline(p, 6, 9, 5, (220, 80, 220, 255)); hline(p, 6, 9, 10, (220, 80, 220, 255))
    vline(p, 5, 6, 9, (220, 80, 220, 255)); vline(p, 10, 6, 9, (220, 80, 220, 255))
    # Center flash
    rect_fill(p, 7, 7, 8, 8, WHITE)
    return p


def make_fortify():
    """Fortify - double shield / wall."""
    p = [T] * 256
    # Back shield (darker)
    rect_fill(p, 6, 2, 12, 9, DKBLUE)
    hline(p, 7, 11, 10, DKBLUE)
    hline(p, 8, 10, 11, DKBLUE)
    px_set(p, 9, 12, DKBLUE)
    # Front shield (brighter)
    rect_fill(p, 3, 4, 9, 11, BLUE)
    hline(p, 4, 8, 12, BLUE)
    hline(p, 5, 7, 13, BLUE)
    px_set(p, 6, 14, BLUE)
    # Inner highlight
    rect_fill(p, 5, 6, 7, 9, (80, 180, 255, 255))
    # +6 marker
    px_set(p, 6, 7, WHITE); px_set(p, 6, 8, WHITE)
    return p


def make_double_shot():
    """Double Shot - two bullets / dual barrels."""
    p = [T] * 256
    # Two bullet trails
    # Top bullet
    rect_fill(p, 3, 4, 9, 5, RED)
    rect_fill(p, 10, 4, 12, 5, (255, 100, 100, 255))
    px_set(p, 13, 4, WHITE); px_set(p, 13, 5, WHITE)
    # Bottom bullet
    rect_fill(p, 3, 9, 9, 10, RED)
    rect_fill(p, 10, 9, 12, 10, (255, 100, 100, 255))
    px_set(p, 13, 9, WHITE); px_set(p, 13, 10, WHITE)
    # Muzzle flash
    px_set(p, 2, 3, ORANGE); px_set(p, 2, 6, ORANGE)
    px_set(p, 2, 8, ORANGE); px_set(p, 2, 11, ORANGE)
    return p


def make_dash():
    """Dash - running boots with speed lines."""
    p = [T] * 256
    # Boot shape
    rect_fill(p, 6, 6, 11, 10, GRAY)
    rect_fill(p, 8, 4, 11, 5, GRAY)  # ankle
    hline(p, 5, 12, 11, DKGRAY)  # sole
    rect_fill(p, 4, 11, 12, 12, DKGRAY)  # thick sole
    # Speed lines
    hline(p, 1, 4, 5, YELLOW)
    hline(p, 0, 3, 8, YELLOW)
    hline(p, 1, 4, 11, YELLOW)
    # Impact star at front
    px_set(p, 13, 7, WHITE); px_set(p, 13, 8, WHITE)
    px_set(p, 14, 7, YELLOW)
    return p


def make_ice():
    """Ice - snowflake / ice crystal."""
    p = [T] * 256
    # Central cross
    vline(p, 7, 2, 13, LTBLUE); vline(p, 8, 2, 13, LTBLUE)
    hline(p, 2, 13, 7, LTBLUE); hline(p, 2, 13, 8, LTBLUE)
    # Diagonal arms
    for d in range(1, 5):
        px_set(p, 7 - d, 7 - d, (140, 210, 255, 255))
        px_set(p, 8 + d, 7 - d, (140, 210, 255, 255))
        px_set(p, 7 - d, 8 + d, (140, 210, 255, 255))
        px_set(p, 8 + d, 8 + d, (140, 210, 255, 255))
    # Center gem
    rect_fill(p, 7, 7, 8, 8, WHITE)
    # Branch tips
    px_set(p, 4, 2, LTBLUE); px_set(p, 11, 2, LTBLUE)
    px_set(p, 4, 13, LTBLUE); px_set(p, 11, 13, LTBLUE)
    return p


def make_acid():
    """Acid - bubbling droplet."""
    p = [T] * 256
    # Main droplet
    px_set(p, 7, 2, ACIDGRN); px_set(p, 8, 2, ACIDGRN)
    hline(p, 6, 9, 3, ACIDGRN)
    hline(p, 5, 10, 4, ACIDGRN)
    rect_fill(p, 4, 5, 11, 10, ACIDGRN)
    hline(p, 5, 10, 11, ACIDGRN)
    hline(p, 6, 9, 12, ACIDGRN)
    # Highlight
    rect_fill(p, 6, 5, 7, 7, (80, 255, 200, 255))
    # Bubbles
    px_set(p, 9, 6, (80, 255, 200, 255))
    px_set(p, 5, 9, (80, 255, 200, 255))
    # Dripping
    px_set(p, 5, 13, ACIDGRN); px_set(p, 10, 12, ACIDGRN)
    px_set(p, 10, 13, (30, 160, 130, 255))
    return p


def make_fire():
    """Fire - flame icon."""
    p = [T] * 256
    # Inner flame (bright)
    rect_fill(p, 6, 7, 9, 12, YELLOW)
    hline(p, 7, 8, 6, YELLOW)
    # Outer flame (orange/red)
    hline(p, 5, 10, 10, ORANGE); hline(p, 5, 10, 11, ORANGE)
    rect_fill(p, 4, 11, 11, 13, ORANGE)
    hline(p, 5, 10, 14, RED)
    # Flame tips
    px_set(p, 7, 3, RED); px_set(p, 8, 4, ORANGE); px_set(p, 7, 5, ORANGE)
    px_set(p, 5, 5, RED); px_set(p, 10, 6, RED)
    px_set(p, 4, 8, RED); px_set(p, 11, 9, RED)
    # Hot center
    rect_fill(p, 7, 8, 8, 10, WHITE)
    return p


def make_lightning():
    """Lightning - zigzag bolt."""
    p = [T] * 256
    # Main bolt
    hline(p, 6, 9, 1, LIGHTNING)
    px_set(p, 8, 2, LIGHTNING); px_set(p, 9, 2, LIGHTNING)
    px_set(p, 7, 3, LIGHTNING); px_set(p, 8, 3, LIGHTNING)
    hline(p, 5, 8, 4, LIGHTNING)
    px_set(p, 7, 5, LIGHTNING); px_set(p, 8, 5, LIGHTNING)
    hline(p, 6, 10, 6, LIGHTNING)
    px_set(p, 8, 7, LIGHTNING); px_set(p, 9, 7, LIGHTNING)
    px_set(p, 7, 8, LIGHTNING); px_set(p, 8, 8, LIGHTNING)
    hline(p, 5, 8, 9, LIGHTNING)
    px_set(p, 7, 10, LIGHTNING); px_set(p, 8, 10, LIGHTNING)
    hline(p, 6, 10, 11, LIGHTNING)
    px_set(p, 7, 12, LIGHTNING); px_set(p, 8, 12, LIGHTNING)
    px_set(p, 6, 13, LIGHTNING); px_set(p, 7, 13, LIGHTNING)
    hline(p, 6, 9, 14, LIGHTNING)
    # Glow
    px_set(p, 5, 3, (200, 200, 40, 255)); px_set(p, 10, 8, (200, 200, 40, 255))
    return p


def make_sniper():
    """Sniper - long rifle with scope."""
    p = [T] * 256
    # Rifle barrel (long horizontal)
    hline(p, 1, 13, 7, DKGRAY)
    hline(p, 1, 13, 8, GRAY)
    # Scope on top
    rect_fill(p, 5, 4, 8, 6, DKGRAY)
    rect_fill(p, 6, 5, 7, 5, (60, 100, 200, 255))  # scope lens (blue)
    # Stock (right side)
    rect_fill(p, 10, 6, 14, 9, DKGRAY)
    rect_fill(p, 11, 7, 13, 8, GRAY)
    # Muzzle (left)
    px_set(p, 0, 7, LTGRAY); px_set(p, 0, 8, LTGRAY)
    # Trigger guard
    px_set(p, 8, 9, DKGRAY); px_set(p, 9, 9, DKGRAY)
    px_set(p, 8, 10, DKGRAY)
    # Crosshair hint
    px_set(p, 2, 5, RED); px_set(p, 2, 10, RED)
    px_set(p, 0, 7, RED); px_set(p, 0, 8, RED)
    return p


def make_shotgun():
    """Shotgun - short wide barrel, spread pattern."""
    p = [T] * 256
    # Short barrel
    rect_fill(p, 2, 7, 9, 8, DKGRAY)
    rect_fill(p, 3, 7, 8, 8, GRAY)
    # Wide muzzle
    rect_fill(p, 0, 6, 1, 9, GRAY)
    # Stock
    rect_fill(p, 10, 6, 14, 9, DKGRAY)
    rect_fill(p, 11, 7, 14, 8, (100, 70, 40, 255))  # wood stock
    # Pump grip
    rect_fill(p, 5, 9, 8, 10, LTGRAY)
    # Spread pattern (scatter shot)
    px_set(p, 0, 3, RED); px_set(p, 1, 2, RED)
    px_set(p, 2, 4, RED)
    px_set(p, 0, 12, RED); px_set(p, 1, 13, RED)
    px_set(p, 2, 11, RED)
    px_set(p, 0, 7, ORANGE); px_set(p, 0, 8, ORANGE)
    return p


def make_welder():
    """Welder - welding torch with sparks."""
    p = [T] * 256
    # Torch handle
    rect_fill(p, 8, 8, 13, 9, DKGRAY)
    rect_fill(p, 9, 8, 12, 9, GRAY)
    # Torch nozzle
    rect_fill(p, 5, 7, 7, 10, DKGRAY)
    rect_fill(p, 6, 8, 7, 9, LTGRAY)
    # Flame/arc
    rect_fill(p, 2, 6, 4, 11, ORANGE)
    rect_fill(p, 3, 7, 4, 10, YELLOW)
    px_set(p, 3, 8, WHITE); px_set(p, 3, 9, WHITE)
    # Sparks flying
    px_set(p, 1, 4, YELLOW); px_set(p, 3, 3, ORANGE)
    px_set(p, 0, 7, YELLOW); px_set(p, 1, 12, YELLOW)
    px_set(p, 4, 13, ORANGE); px_set(p, 2, 14, YELLOW)
    px_set(p, 5, 4, ORANGE); px_set(p, 0, 10, ORANGE)
    # Grip
    rect_fill(p, 11, 10, 12, 12, ORANGE)
    return p


def make_chainsaw():
    """Chainsaw - saw blade teeth on a bar."""
    p = [T] * 256
    # Chain bar
    rect_fill(p, 2, 7, 12, 8, DKGRAY)
    rect_fill(p, 3, 7, 11, 8, GRAY)
    # Teeth on top and bottom
    for x in range(2, 11, 2):
        px_set(p, x, 6, LTGRAY)    # top teeth
        px_set(p, x + 1, 9, LTGRAY)  # bottom teeth
    # Nose (rounded tip)
    px_set(p, 1, 7, LTGRAY); px_set(p, 1, 8, LTGRAY)
    px_set(p, 0, 7, DKGRAY); px_set(p, 0, 8, DKGRAY)
    # Engine body
    rect_fill(p, 10, 5, 14, 10, DKRED)
    rect_fill(p, 11, 6, 13, 9, RED)
    # Handle
    rect_fill(p, 12, 3, 14, 5, DKGRAY)
    rect_fill(p, 12, 10, 14, 12, DKGRAY)
    # Pull cord
    px_set(p, 15, 4, ORANGE); px_set(p, 15, 5, ORANGE)
    # Blood splatter hint
    px_set(p, 3, 5, RED); px_set(p, 6, 10, RED)
    return p


def make_laser():
    """Laser - precision beam with lens."""
    p = [T] * 256
    # Beam (thin, precise)
    hline(p, 0, 7, 7, CYAN)
    hline(p, 0, 7, 8, CYAN)
    # Bright core
    hline(p, 1, 5, 7, WHITE)
    hline(p, 1, 5, 8, WHITE)
    # Lens/emitter
    rect_fill(p, 8, 5, 10, 10, DKGRAY)
    rect_fill(p, 8, 6, 9, 9, (40, 80, 160, 255))  # blue lens
    px_set(p, 8, 7, CYAN); px_set(p, 8, 8, CYAN)  # emission point
    # Gun body
    rect_fill(p, 10, 6, 14, 9, DKGRAY)
    rect_fill(p, 11, 7, 13, 8, GRAY)
    # Target dot
    px_set(p, 0, 7, RED); px_set(p, 0, 8, RED)
    # Refraction sparkles
    px_set(p, 3, 5, (100, 220, 255, 255))
    px_set(p, 5, 10, (100, 220, 255, 255))
    return p


def make_deflector():
    """Deflector - energy shield bubble with reflection arrows."""
    p = [T] * 256
    # Shield bubble (dome shape)
    hline(p, 5, 10, 2, STEEL)
    hline(p, 3, 12, 3, STEEL)
    vline(p, 2, 4, 10, STEEL)
    vline(p, 13, 4, 10, STEEL)
    hline(p, 3, 12, 11, STEEL)
    hline(p, 5, 10, 12, STEEL)
    # Inner energy fill
    for y in range(4, 11):
        hline(p, 4, 11, y, DKBLUE)
    rect_fill(p, 5, 4, 10, 10, (30, 60, 120, 255))
    # Energy shimmer
    px_set(p, 5, 5, CYAN); px_set(p, 6, 4, CYAN)
    px_set(p, 9, 6, (80, 160, 200, 255))
    # Reflect arrows (bouncing off)
    px_set(p, 1, 6, RED); px_set(p, 1, 7, RED)  # incoming
    px_set(p, 0, 5, RED)  # incoming continued
    px_set(p, 1, 9, ORANGE); px_set(p, 0, 10, ORANGE)  # reflected back
    return p


def make_stun_gun():
    """Stun gun - taser/zapper with electricity arcs."""
    p = [T] * 256
    # Gun body
    rect_fill(p, 5, 6, 13, 9, DKGRAY)
    rect_fill(p, 6, 7, 12, 8, GRAY)
    # Prongs at front
    rect_fill(p, 2, 5, 4, 6, LTGRAY)
    rect_fill(p, 2, 9, 4, 10, LTGRAY)
    # Electric arc between prongs
    px_set(p, 2, 7, PURPLE); px_set(p, 3, 7, LIGHTNING)
    px_set(p, 1, 8, LIGHTNING); px_set(p, 3, 8, PURPLE)
    px_set(p, 2, 8, WHITE)
    # More arcs
    px_set(p, 1, 6, LIGHTNING); px_set(p, 1, 9, LIGHTNING)
    px_set(p, 0, 7, PURPLE); px_set(p, 0, 8, PURPLE)
    # Grip
    rect_fill(p, 9, 10, 11, 13, DKGRAY)
    rect_fill(p, 10, 10, 10, 12, GRAY)
    # Trigger
    px_set(p, 8, 10, LTGRAY)
    # Power indicator
    px_set(p, 11, 7, CYAN); px_set(p, 11, 8, CYAN)
    return p


if __name__ == '__main__':
    outdir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sprites')
    os.makedirs(outdir, exist_ok=True)

    cards = {
        'card_shield.png':      make_shield(),
        'card_shoot.png':       make_shoot(),
        'card_burst.png':       make_burst(),
        'card_move.png':        make_move(),
        'card_melee.png':       make_melee(),
        'card_overcharge.png':  make_overcharge(),
        'card_repair.png':      make_repair(),
        'card_stun.png':        make_stun(),
        'card_fortify.png':     make_fortify(),
        'card_double_shot.png': make_double_shot(),
        'card_dash.png':        make_dash(),
        'card_ice.png':         make_ice(),
        'card_acid.png':        make_acid(),
        'card_fire.png':        make_fire(),
        'card_lightning.png':   make_lightning(),
        'card_sniper.png':      make_sniper(),
        'card_shotgun.png':     make_shotgun(),
        'card_welder.png':      make_welder(),
        'card_chainsaw.png':    make_chainsaw(),
        'card_laser.png':       make_laser(),
        'card_deflector.png':   make_deflector(),
        'card_stun_gun.png':    make_stun_gun(),
    }

    for name, pixels in cards.items():
        path = os.path.join(outdir, name)
        write_png(path, pixels)
        print(f'Wrote {path}')
