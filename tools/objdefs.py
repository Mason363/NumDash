"""Object catalogue shared by the level compiler and the C runtime.

Hitboxes and draw layers follow Geometry Dash 2.x: hitbox sizes are in GD
units before rotation; layer ranks encode GD's z-layer / blending / z-order
sort so that decorations, glows, the player, portals and blocks overlap in
the same order as the original game.
"""

# Collision kinds
NONE, SOLID, HAZARD, SPECIAL = range(4)
# Special behaviours
SP = ['NONE', 'PAD_Y', 'PAD_P', 'PAD_B', 'ORB_Y', 'ORB_P', 'ORB_B', 'GRAV_N', 'GRAV_F',
      'PORTAL_CUBE', 'PORTAL_SHIP', 'COIN']
# Colour types for draw parts
CT = ['OBJ', 'BLACK', 'WHITE', 'P1ADD', 'P2ADD', 'GLOW', 'GLOW_Y', 'GLOW_B', 'GLOW_P']
# Layer ranks (drawing order); the player is drawn at LAYER_PLAYER.
LAYERS = ['DECO_BACK', 'RODS', 'ROD_BALLS', 'DETAIL', 'SPECIAL_GLOW', 'SPECIAL', 'PORTAL_BACK',
          'BLOCK_GLOW', 'PLAYER', 'COIN', 'PORTAL_FRONT', 'FILL', 'BLOCK']
# Part flags
F_PULSE, F_RANDOM3, F_COIN, F_ANIM = 1, 2, 4, 8

# name, gd ids, collision, hitbox w, h, special, editor y offset, parts
# part: (sprite, dx, dy, layer, colour type, flags)
OBJECTS = [
    ('BLOCK', [1], SOLID, 30, 30, 'NONE', 0, [('block', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_all', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_T', [2], SOLID, 30, 30, 'NONE', 0, [('grid_t', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_t', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_TL', [3], SOLID, 30, 30, 'NONE', 0, [('grid_tl', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_tl', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_C', [4], SOLID, 30, 30, 'NONE', 0, [('grid_c', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_c', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_LTR', [6], SOLID, 30, 30, 'NONE', 0, [('grid_ltr', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_ltr', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_LR', [7], SOLID, 30, 30, 'NONE', 0, [('grid_lr', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_lr', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('GRID_DECO', [5], NONE, 0, 0, 'NONE', 0, [('grid_none', 0, 0, 'DECO_BACK', 'OBJ', 0)]),
    ('DIAG_DECO', [73], NONE, 0, 0, 'NONE', 0, [('diag_block', 0, 0, 'DECO_BACK', 'OBJ', 0)]),
    ('PLANK', [40], SOLID, 30, 14, 'NONE', 8, [('plank', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_plank', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('SLAB0', [62], SOLID, 30, 16, 'NONE', 7, [('slab0', 0, 0, 'BLOCK', 'OBJ', 0)]),
    ('SLAB1', [65], SOLID, 30, 16, 'NONE', 7, [('slab1', 0, 0, 'BLOCK', 'OBJ', 0)]),
    ('SLAB2', [66], SOLID, 30, 16, 'NONE', 7, [('slab2', 0, 0, 'BLOCK', 'OBJ', 0)]),
    ('SLAB3', [68], SOLID, 30, 16, 'NONE', 7, [('slab3', 0, 0, 'BLOCK', 'OBJ', 0)]),
    ('SPIKE', [8], HAZARD, 6, 12, 'NONE', 0, [('spike', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_spike', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('SPIKE_SMALL', [39], HAZARD, 6, 5.6, 'NONE', -9, [('spike_small', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_spike_small', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('SPIKE_MED', [103], HAZARD, 4, 7.6, 'NONE', -6, [('spike_med', 0, 0, 'BLOCK', 'OBJ', 0), ('glow_spike_med', 0, 0, 'BLOCK_GLOW', 'GLOW', 0)]),
    ('PIT', [9], HAZARD, 9, 10.8, 'NONE', -13, [('pit0', 0, 0, 'BLOCK', 'BLACK', F_RANDOM3)]),
    ('ROD1', [15], NONE, 0, 0, 'NONE', 6, [('rod1', 0, 0, 'RODS', 'BLACK', 0), ('rod_ball', 0, 35, 'ROD_BALLS', 'P1ADD', F_PULSE)]),
    ('ROD2', [16], NONE, 0, 0, 'NONE', -1, [('rod2', 0, 0, 'RODS', 'BLACK', 0), ('rod_ball', 0, 27.5, 'ROD_BALLS', 'P1ADD', F_PULSE)]),
    ('ROD3', [17], NONE, 0, 0, 'NONE', -8, [('rod3', 0, 0, 'RODS', 'BLACK', 0), ('rod_ball', 0, 20, 'ROD_BALLS', 'P1ADD', F_PULSE)]),
    ('DSPIKES1', [18], NONE, 0, 0, 'NONE', 4, [('dspikes1', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('DSPIKES2', [19], NONE, 0, 0, 'NONE', 4, [('dspikes2', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('DSPIKES3', [20], NONE, 0, 0, 'NONE', -2, [('dspikes3', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('DSPIKES4', [21], NONE, 0, 0, 'NONE', -8, [('dspikes4', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('CHAIN', [41], NONE, 0, 0, 'NONE', -12, [('chain', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('CHAIN_SHORT', [110], NONE, 0, 0, 'NONE', -13, [('chain_short', 0, 0, 'DETAIL', 'P1ADD', 0)]),
    ('STAR', [54], NONE, 0, 0, 'NONE', 0, [('star', 0, 0, 'DETAIL', 'P2ADD', 0)]),
    ('PAD_Y', [35], SPECIAL, 25, 4, 'PAD_Y', -13, [('pad_yellow', 0, 0, 'SPECIAL', 'WHITE', 0), ('glow_pad_yellow', 0, 2, 'SPECIAL_GLOW', 'GLOW_Y', 0)]),
    ('PAD_B', [67], SPECIAL, 25, 6, 'PAD_B', -12, [('pad_blue', 0, 0, 'SPECIAL', 'WHITE', 0), ('glow_pad_blue', 0, 2, 'SPECIAL_GLOW', 'GLOW_B', 0)]),
    ('PAD_P', [140], SPECIAL, 25, 5, 'PAD_P', -13, [('pad_pink', 0, 0, 'SPECIAL', 'WHITE', 0), ('glow_pad_pink', 0, 2, 'SPECIAL_GLOW', 'GLOW_P', 0)]),
    ('ORB_Y', [36], SPECIAL, 36, 36, 'ORB_Y', 0, [('orb_yellow', 0, 0, 'SPECIAL', 'WHITE', F_PULSE), ('glow_orb_yellow', 0, 0, 'SPECIAL_GLOW', 'GLOW_Y', 0)]),
    ('ORB_P', [141], SPECIAL, 36, 36, 'ORB_P', 0, [('orb_pink', 0, 0, 'SPECIAL', 'WHITE', F_PULSE), ('glow_orb_pink', 0, 0, 'SPECIAL_GLOW', 'GLOW_P', 0)]),
    ('ORB_B', [84], SPECIAL, 36, 36, 'ORB_B', 0, [('orb_blue', 0, 0, 'SPECIAL', 'WHITE', F_PULSE), ('glow_orb_blue', 0, 0, 'SPECIAL_GLOW', 'GLOW_B', 0)]),
    ('GRAV_N', [10], SPECIAL, 25, 75, 'GRAV_N', 0, [('portal_grav_blue_front', 4, 0, 'PORTAL_FRONT', 'WHITE', 0), ('portal_grav_blue_back', -6, 0, 'PORTAL_BACK', 'WHITE', 0)]),
    ('GRAV_F', [11], SPECIAL, 25, 75, 'GRAV_F', 0, [('portal_grav_yellow_front', 4, 0, 'PORTAL_FRONT', 'WHITE', 0), ('portal_grav_yellow_back', -6, 0, 'PORTAL_BACK', 'WHITE', 0)]),
    ('PORTAL_CUBE', [12], SPECIAL, 34, 86, 'PORTAL_CUBE', 0, [('portal_cube_front', 5, 0, 'PORTAL_FRONT', 'WHITE', 0), ('portal_cube_back', -6, 0, 'PORTAL_BACK', 'WHITE', 0)]),
    ('PORTAL_SHIP', [13], SPECIAL, 34, 86, 'PORTAL_SHIP', 0, [('portal_ship_front', 5, 0, 'PORTAL_FRONT', 'WHITE', 0), ('portal_ship_back', -6, 0, 'PORTAL_BACK', 'WHITE', 0)]),
    ('COIN', [1329, 142], SPECIAL, 40, 40, 'COIN', 0, [('coin0', 0, 0, 'COIN', 'WHITE', F_COIN)]),
]

# Level trigger objects become events rather than objects.
EV_COLOR, EV_FADE, EV_TRAIL = 1, 2, 3
COLOR_TRIGGERS = {29: 0, 30: 1, 104: 2, 105: 3}     # BG, G1, LINE, OBJ
FADE_TRIGGERS = {22: 0, 23: 1, 24: 2, 25: 3, 26: 4, 27: 5, 28: 6, 55: 7, 56: 8, 57: 9, 58: 10, 59: 11}
TRAIL_TRIGGERS = {32: 1, 33: 0}

# Palette offered by the level editor, in GD's build-tab order.
EDITOR = ['BLOCK', 'GRID_T', 'GRID_TL', 'GRID_LTR', 'GRID_LR', 'GRID_C', 'PLANK',
          'SPIKE', 'SPIKE_SMALL', 'SPIKE_MED', 'PIT', 'PAD_Y', 'PAD_P', 'PAD_B', 'ORB_Y',
          'PORTAL_SHIP', 'PORTAL_CUBE', 'GRAV_F', 'GRAV_N', 'COIN', 'GRID_DECO', 'ROD1', 'ROD2',
          'ROD3', 'DSPIKES3', 'DSPIKES4', 'CHAIN', 'STAR']

INDEX = {o[0]: i + 1 for i, o in enumerate(OBJECTS)}     # type 0 = none
BY_ID = {}
for i, o in enumerate(OBJECTS):
    for gid in o[1]:
        BY_ID[gid] = i + 1
