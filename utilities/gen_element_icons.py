#!/usr/bin/env python3
"""Generate 16x16 pixel-art element icons for the weakness system."""

from PIL import Image

W, H = 16, 16
T = (0, 0, 0, 0)  # transparent

def save_icon(name, pixels):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for y in range(H):
        for x in range(W):
            img.putpixel((x, y), pixels[y][x])
    img.save(f"../assets/sprites/{name}.png")
    print(f"Generated {name}.png")

# Color palettes
ICE_LIGHT  = (170, 220, 255, 255)
ICE_MID    = (100, 180, 240, 255)
ICE_DARK   = (50, 120, 200, 255)
ICE_WHITE  = (220, 240, 255, 255)

ACID_LIGHT = (140, 230, 80, 255)
ACID_MID   = (80, 200, 40, 255)
ACID_DARK  = (40, 140, 20, 255)
ACID_DROP  = (180, 255, 100, 255)

FIRE_LIGHT = (255, 240, 100, 255)
FIRE_MID   = (255, 160, 40, 255)
FIRE_DARK  = (220, 60, 20, 255)
FIRE_HOT   = (255, 255, 200, 255)

LGHT_LIGHT = (255, 255, 140, 255)
LGHT_MID   = (255, 230, 60, 255)
LGHT_DARK  = (200, 180, 30, 255)
LGHT_WHITE = (255, 255, 220, 255)

# --- ICE: snowflake / crystal shape ---
ice = [[T]*W for _ in range(H)]
# Draw a simple snowflake
def set_px(grid, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        grid[y][x] = c

# Vertical and horizontal lines through center
for i in range(3, 13):
    set_px(ice, 8, i, ICE_MID)
    set_px(ice, i, 8, ICE_MID)
# Diagonals
for i in range(-4, 5):
    set_px(ice, 8+i, 8+i, ICE_LIGHT)
    set_px(ice, 8+i, 8-i, ICE_LIGHT)
# Center bright
set_px(ice, 8, 8, ICE_WHITE)
set_px(ice, 7, 7, ICE_WHITE)
set_px(ice, 9, 9, ICE_WHITE)
set_px(ice, 7, 9, ICE_WHITE)
set_px(ice, 9, 7, ICE_WHITE)
# Branch tips
for dx, dy in [(-4, -1), (-4, 1), (4, -1), (4, 1), (-1, -4), (1, -4), (-1, 4), (1, 4)]:
    set_px(ice, 8+dx, 8+dy, ICE_DARK)

save_icon("icon_ice", ice)

# --- ACID: bubbling droplet ---
acid = [[T]*W for _ in range(H)]

# Main droplet shape
drop_coords = [
    (7, 3), (8, 3),
    (6, 4), (7, 4), (8, 4), (9, 4),
    (5, 5), (6, 5), (7, 5), (8, 5), (9, 5), (10, 5),
    (5, 6), (6, 6), (7, 6), (8, 6), (9, 6), (10, 6),
    (4, 7), (5, 7), (6, 7), (7, 7), (8, 7), (9, 7), (10, 7), (11, 7),
    (4, 8), (5, 8), (6, 8), (7, 8), (8, 8), (9, 8), (10, 8), (11, 8),
    (4, 9), (5, 9), (6, 9), (7, 9), (8, 9), (9, 9), (10, 9), (11, 9),
    (5, 10), (6, 10), (7, 10), (8, 10), (9, 10), (10, 10),
    (5, 11), (6, 11), (7, 11), (8, 11), (9, 11), (10, 11),
    (6, 12), (7, 12), (8, 12), (9, 12),
    (7, 13), (8, 13),
]
for x, y in drop_coords:
    set_px(acid, x, y, ACID_MID)
# Highlight
for x, y in [(6, 5), (6, 6), (7, 5), (7, 6)]:
    set_px(acid, x, y, ACID_LIGHT)
# Dark edge
for x, y in [(9, 9), (10, 9), (10, 10), (9, 11), (8, 12)]:
    set_px(acid, x, y, ACID_DARK)
# Bubbles
for x, y in [(3, 11), (12, 6), (2, 9)]:
    set_px(acid, x, y, ACID_DROP)

save_icon("icon_acid", acid)

# --- FIRE: flame shape ---
fire = [[T]*W for _ in range(H)]

flame_coords_dark = [
    (7, 2), (8, 2),
    (6, 3), (7, 3), (8, 3), (9, 3),
    (5, 4), (6, 4), (9, 4), (10, 4),
    (5, 5), (6, 5), (9, 5), (10, 5),
    (4, 6), (5, 6), (10, 6), (11, 6),
    (4, 7), (5, 7), (10, 7), (11, 7),
    (4, 8), (5, 8), (10, 8), (11, 8),
    (5, 9), (6, 9), (9, 9), (10, 9),
    (5, 10), (6, 10), (9, 10), (10, 10),
    (6, 11), (7, 11), (8, 11), (9, 11),
    (7, 12), (8, 12),
]
flame_coords_mid = [
    (7, 4), (8, 4),
    (7, 5), (8, 5),
    (6, 6), (7, 6), (8, 6), (9, 6),
    (6, 7), (7, 7), (8, 7), (9, 7),
    (6, 8), (7, 8), (8, 8), (9, 8),
    (7, 9), (8, 9),
    (7, 10), (8, 10),
]
flame_coords_hot = [
    (7, 6), (8, 6),
    (7, 7), (8, 7),
    (7, 8), (8, 8),
]

for x, y in flame_coords_dark:
    set_px(fire, x, y, FIRE_DARK)
for x, y in flame_coords_mid:
    set_px(fire, x, y, FIRE_MID)
for x, y in flame_coords_hot:
    set_px(fire, x, y, FIRE_LIGHT)
# Bright center
set_px(fire, 7, 7, FIRE_HOT)
set_px(fire, 8, 7, FIRE_HOT)
# Top flicker
set_px(fire, 8, 1, FIRE_MID)
set_px(fire, 7, 1, FIRE_DARK)

save_icon("icon_fire", fire)

# --- LIGHTNING: bolt shape ---
lght = [[T]*W for _ in range(H)]

bolt = [
    # Top part of bolt
    (8, 1), (9, 1),
    (7, 2), (8, 2), (9, 2),
    (7, 3), (8, 3),
    (6, 4), (7, 4),
    (6, 5), (7, 5),
    # Middle horizontal bar
    (5, 6), (6, 6), (7, 6), (8, 6), (9, 6), (10, 6),
    (5, 7), (6, 7), (7, 7), (8, 7), (9, 7), (10, 7),
    # Bottom part of bolt
    (8, 8), (9, 8),
    (8, 9), (9, 9),
    (7, 10), (8, 10),
    (7, 11), (8, 11),
    (6, 12), (7, 12),
    (6, 13), (7, 13),
]

for x, y in bolt:
    set_px(lght, x, y, LGHT_MID)
# Bright center
for x, y in [(7, 6), (8, 6), (7, 7), (8, 7)]:
    set_px(lght, x, y, LGHT_WHITE)
# Dark edges
for x, y in [(5, 6), (10, 6), (5, 7), (10, 7), (9, 1), (6, 13)]:
    set_px(lght, x, y, LGHT_DARK)
# Bright tip
set_px(lght, 6, 12, LGHT_LIGHT)

save_icon("icon_lightning", lght)

print("All element icons generated!")
