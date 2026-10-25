"""The stone-ring campfire's 20 fuel sticks: where each lies (shared by make_stone_ring_campfire.py, which builds the Blueprint, and
apply_campfire_fuel_layout.py, which re-lays the sticks of the existing one in place).

A log-cabin base of four, then a teepee of sixteen, laid alternately round so any amount stands balanced. Oct 24 2026, Wes: "drop the wood in the
fireplace a little so it's more 'in' the ground and give it some randomness so it doesn't look so cartoonishly perfect" (BR25-8): every stick sits
4 cm lower (the base half in the ash, the teepee's feet dug in), and each is its own length (x0.85-1.15), leans and turns by its own amount, and the
teepee's feet wander in and out of their circle.

Positions are relative to the campfire's actor (the caller subtracts the Mesh component's offset). Seeded: the same layout every time.
"""
import math
import random

FUEL_SCALE = 1.35      # 27 cm firewood -> 36 cm sticks
TEEPEE_FOOT = 21.0
TEEPEE_PITCH = 58.0
SINK_CM = 4.0
HALF_LENGTH = 13.35    # the firewood mesh's half length at scale 1 (cm)
ORDER = [0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15]


def layout(seed=2410):
    """[(name, (x, y, z), (yaw, pitch, roll), scale)] for FuelStick_01..20, in fill order."""
    rng = random.Random(seed)
    out = []
    base = [((0, -9, 3.5), 0), ((0, 9, 3.5), 0), ((-9, 0, 9.5), 90), ((9, 0, 9.5), 90)]
    for i, (p, yaw) in enumerate(base):
        scale = FUEL_SCALE * rng.uniform(0.85, 1.15)
        loc = (p[0] + rng.uniform(-3, 3), p[1] + rng.uniform(-3, 3), p[2] - SINK_CM + rng.uniform(-1.5, 0.5))
        out.append(("FuelStick_%02d" % (i + 1), loc, (yaw + rng.uniform(-18, 18), rng.uniform(-6, 6), rng.uniform(0, 360)), scale))
    for n, k in enumerate(ORDER):
        a = 2 * math.pi * k / 16 + rng.uniform(-0.18, 0.18)
        r = TEEPEE_FOOT + rng.uniform(-4, 4)
        foot = (r * math.cos(a), r * math.sin(a), 1.5 - SINK_CM + rng.uniform(-1.5, 1.0))
        yaw = math.degrees(a) + 180 + rng.uniform(-8, 8)      # pointing in, toward the centre, not quite at it
        pitch = TEEPEE_PITCH + rng.uniform(-9, 9)
        scale = FUEL_SCALE * rng.uniform(0.85, 1.15)
        half = HALF_LENGTH * scale
        d = (math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch)))
        centre = (foot[0] + d[0] * half, foot[1] + d[1] * half, foot[2] + d[2] * half)
        out.append(("FuelStick_%02d" % (len(base) + n + 1), centre, (yaw, pitch, rng.uniform(0, 360)), scale))
    return out
