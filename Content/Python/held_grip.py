"""The hand's grip on a held item (Docs/Held_Grip.md; Nov 2 2026, Wes: "make sure it works on the other metahumans and make sure we have a
standard for how they grip other items"): the standard in Python -- the same numbers as FMOHandGrip (MOHandGrip.cpp), for measuring items,
writing their rows and checking any base's hand offline. Keep the two the same.

    python Tools/ue.py py -c "import held_grip as g; out(g.hold_all())"            every base, both hands, the standard handles: fingers on them?
    python Tools/ue.py py -c "import held_grip as g; out(g.derive_standard())"     the Power seat's constants, from Femme's seat of her bow
    python Tools/ue.py py --timeout 1800 -c "import held_grip as g; out(g.measure_all())"
                                                  every held item's handle measured off its mesh -> Saved/HeldGrip/rows.json (ue.py rows set)

The standard (FMOItemHeldVisual): an item gives GripStyle (Power: a handle across the palm; Sphere: something round in it), GripTransform (the
handle in its mesh: origin where the hand's centre closes on it, +Z toward the head -- the fist's index / thumb side --, +X the way it faces out
of the front of the fist: an axe's edge, a hammer's face, a bow's back), GripHalfSize and GripPose (the fist: GRIP_POSE). Each body's own hand
does the rest (the seat from its bones, the fingers closed on the handle), so every base -- and the next one -- holds it right.
"""
import json
import math
import os
import re

import unreal

GRIP_POSE = "/MOFramework/Characters/Bow/RMB_Loop_MH"   # the standard fist: Sparrow's bow hand at full draw, a power grip
GRIP_POSE_TIME = 0.33
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
# FMOHandGrip's constants (keep them the same)
RADII = {"thumb": (1.0, 0.92, 0.8), "index": (0.85, 0.78, 0.68), "middle": (0.86, 0.8, 0.7), "ring": (0.8, 0.74, 0.65), "pinky": (0.72, 0.66, 0.58)}
FLEX_LIMIT_DEG = {"thumb": (50.0, 60.0, 80.0), "index": (90.0, 110.0, 80.0), "middle": (90.0, 110.0, 80.0), "ring": (90.0, 110.0, 80.0),
                  "pinky": (90.0, 110.0, 80.0)}
TIP = 0.85
PALM_R = 1.2
REF_SPAN = 5.64
MAX_EXTENSION = 0.25
THUMB_SWING_MAX_DEG, THUMB_SWING_STEPS = 60.0, 24
SAMPLES, STEPS, ITERATIONS = 4, 16, 10
POWER_CENTRE = (0.0, 0.5619, 0.2897)
POWER_AXIS = (0.9998, 0.0037, -0.019)
POWER_FACE = (-0.0191, 0.374, -0.9272)
REFERENCE_BASE = "Femme"   # the hand the standard was measured on


# -- vectors and quaternions as tuples (UE's conventions: FTransform child * parent, Hamilton products) --
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(_dot(a, a))
def _norm(a):
    n = _len(a)
    return _mul(a, 1.0 / n) if n > 1e-9 else a
def _lerp(a, b, f): return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f)
def _perp(v, axis): return _sub(v, _mul(axis, _dot(v, axis)))


def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def _qinv(q): return (-q[0], -q[1], -q[2], q[3])


def _qrot(q, v):
    x, y, z, w = q
    tx, ty, tz = 2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx))


def _qaxis(axis, ang):
    s = math.sin(ang * 0.5)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(ang * 0.5))


def _qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


def _quat_from_axes(X, Y, Z):
    """The rotation taking +X, +Y, +Z to X, Y, Z (FMatrix(X, Y, Z, 0).ToQuat())."""
    m00, m01, m02 = X[0], Y[0], Z[0]
    m10, m11, m12 = X[1], Y[1], Z[1]
    m20, m21, m22 = X[2], Y[2], Z[2]
    tr = m00 + m11 + m22
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        q = ((m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s)
    elif m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2
        q = (0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s)
    elif m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2
        q = ((m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s)
    else:
        s = math.sqrt(1.0 + m22 - m00 - m11) * 2
        q = ((m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s)
    return _qnorm(q)


def _compose(local, parent):
    """child component space = local * parent."""
    (lq, lt), (pq, pt) = local, parent
    return (_qmul(pq, lq), _add(_qrot(pq, lt), pt))


def _inverse(t):
    qi = _qinv(t[0])
    return (qi, _mul(_qrot(qi, t[1]), -1.0))


def _tq(t):
    return ((t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w), (t.translation.x, t.translation.y, t.translation.z))


def bases():
    """{base id: body mesh path} from Config/DefaultGame.ini's survivor bases (the creator's Base picker: a new base comes in free)."""
    out = {}
    with open(os.path.join(unreal.Paths.project_dir(), "Config", "DefaultGame.ini"), encoding="utf-8") as f:
        for line in f:
            m = re.match(r'\+Bases=\(Id="([^"]+)".*?Body="([^"]+)"', line.strip())
            if m:
                out[m.group(1)] = m.group(2).split(".")[0]
    return out


# -- the hand (FMOHandGrip::ReadHand) --
CHAINS = {"thumb": ["thumb_01", "thumb_02", "thumb_03"]}
for _f in FINGERS[1:]:
    CHAINS[_f] = ["%s_metacarpal" % _f, "%s_01" % _f, "%s_02" % _f, "%s_03" % _f]


class Hand:
    """A body's hand in the fist, in the hand bone's space: the fist's (left hand's) local rotations on this body's own bones."""

    def __init__(self, body, side="l", fist=GRIP_POSE, fist_time=GRIP_POSE_TIME):
        anim = unreal.load_asset(fist)
        opts = unreal.AnimPoseEvaluationOptions()
        opts.set_editor_property("optional_skeletal_mesh", unreal.load_asset(body) if isinstance(body, str) else body)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, fist_time, opts)
        L = unreal.AnimPoseSpaces.LOCAL
        self.side = side

        def local(name):
            ref = _tq(unreal.AnimPoseExtensions.get_ref_bone_pose(pose, "%s_%s" % (name, side), L))
            fq = _tq(unreal.AnimPoseExtensions.get_bone_pose(pose, "%s_l" % name, L))[0]
            return (fq, ref[1]), ref

        self.parent, self.open, self.fist = {}, {}, {}
        self.knuckle, self.tip, self.metacarpal, self.thenar = [], [], [], []
        for f in FINGERS:
            cur = ((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0))
            chain = CHAINS[f]
            joints = chain[-3:]
            for name in chain[:-3]:
                cur = _compose(local(name)[0], cur)
            self.parent[f] = cur
            self.open[f] = [local(n)[1] for n in joints]
            self.fist[f] = [local(n)[0][0] for n in joints]
            if f == "thumb":
                base = _compose(local(joints[0])[0], cur)
                self.thenar = [base[1], _compose(local(joints[1])[0], base)[1]]
            else:
                self.metacarpal.append(cur[1])
                j = cur
                pts = []
                for n in joints:
                    j = _compose(local(n)[0], j)
                    pts.append(j[1])
                self.knuckle.append(pts[0])
                self.tip.append(pts[2])

    def frame(self):
        """The four knuckles' frame (FMOHandGrip::SeatInHand): o, e1 little -> index, e2 toward the curled tips, e3 (the same side of either hand), span."""
        k = self.knuckle
        o = _mul(_add(_add(k[0], k[1]), _add(k[2], k[3])), 0.25)
        e1 = _norm(_sub(k[0], k[3]))
        c = _sub(_mul(_add(_add(self.tip[0], self.tip[1]), _add(self.tip[2], self.tip[3])), 0.25), o)
        e2 = _norm(_perp(c, e1))
        e3 = _cross(e1, e2) if self.side == "l" else _cross(e2, e1)
        return o, e1, e2, e3, _len(_sub(k[0], k[3]))


# -- the handle (FMOHandGrip::Clearance) --
class Shape:
    def __init__(self, frame, half, style):
        self.q, self.t = frame
        self.qi = _qinv(self.q)
        self.half, self.style = half, style

    def local(self, p):
        return _qrot(self.qi, _sub(p, self.t))

    def clear(self, p, r):
        x, y, z = self.local(p)
        a, b, h = self.half
        if self.style == "Sphere":
            rn = math.sqrt((x / a) ** 2 + (y / b) ** 2 + (z / h) ** 2)
            return (_len((x, y, z)) * (1.0 - 1.0 / rn) if rn > 1e-6 else -min(self.half)) - r
        if abs(z) > h + 2.0:
            return 99.0
        rn = math.sqrt((x / a) ** 2 + (y / b) ** 2)
        radial = math.sqrt(x * x + y * y)
        d = radial * (1.0 - 1.0 / rn) if rn > 1e-6 else -min(a, b)
        if abs(z) > h:
            d = max(d, abs(z) - h)
        return d - r


# -- the fingers (FMOHandGrip::MakeFinger / JointRotation / Wrap / FingerClearance) --
class Finger:
    def __init__(self, opened, fist, f, scale):
        self.open, self.axis, self.limit = opened, [], []
        self.thumb = f == "thumb"
        self.swing = (0.0, 0.0, 0.0, 1.0)   # a thumb swung out from a handle too thick to go over (wrap sets it)
        self.radii = [r * scale for r in RADII[f]]
        for k in range(3):
            d = _qnorm(_qmul(fist[k], _qinv(opened[k][0])))
            if d[3] < 0:
                d = tuple(-c for c in d)
            s = math.sqrt(max(0.0, 1.0 - d[3] * d[3]))
            ang = 2.0 * math.acos(max(-1.0, min(1.0, d[3])))
            flexes = ang > 1e-3
            self.axis.append((d[0] / s, d[1] / s, d[2] / s) if flexes and s > 1e-9 else (0.0, 1.0, 0.0))
            self.limit.append(max(ang, math.radians(FLEX_LIMIT_DEG[f][k])) if flexes else 0.0)

    def rotation(self, k, bend):
        flexed = _qmul(_qaxis(self.axis[k], self.limit[k] * bend), self.open[k][0])
        return _qnorm(_qmul(self.swing, flexed) if k == 0 else flexed)

    def joints(self, parent, bend):
        out, cur = [], parent
        for k in range(3):
            cur = _compose((self.rotation(k, bend[k]), self.open[k][1]), cur)
            out.append(cur[1])
        out.append(_add(out[2], _mul(_sub(out[2], out[1]), TIP)))
        return out

    def phalanx(self, shape, j, k):
        r0, r1 = self.radii[k], (self.radii[k + 1] if k < 2 else self.radii[2] * 0.9)
        return min(shape.clear(_lerp(j[k], j[k + 1], i / SAMPLES), r0 + (r1 - r0) * i / SAMPLES) for i in range(1, SAMPLES + 1))

    def clear_from(self, parent, shape, bend, first=0):
        j = self.joints(parent, bend)
        return min(self.phalanx(shape, j, k) for k in range(first, 3))

    def wrap(self, parent, shape):
        bend = [0.0, 0.0, 0.0]
        free = 0
        if self.clear_from(parent, shape, bend) < 0.0:
            # a thumb goes over the handle (closes on from its far side); a finger straightens off it, its base on the handle and the joints
            # past that phalanx closing round it; neither: where it is least in it, the joints past its deepest phalanx still closing
            over = next((s / STEPS for s in range(1, STEPS + 1) if self.thumb and self.clear_from(parent, shape, [s / STEPS] * 3) >= 0.0), None)
            if over is None and self.thumb:
                # too thick to go over: the thumb's base swings out from the handle until the whole thumb is clear (FMOHandGrip::Wrap)
                j = self.joints(parent, bend)
                loc = shape.local(j[1])
                radial = loc if shape.style == "Sphere" else (loc[0], loc[1], 0.0)
                out_dir = _norm(_qrot(shape.q, radial))
                axis = _norm(_cross(_sub(j[3], j[0]), out_dir))
                if _len(axis) > 0.5:
                    axis_p = _norm(_qrot(_qinv(parent[0]), axis))
                    for s in range(1, THUMB_SWING_STEPS + 1):
                        self.swing = _qaxis(axis_p, math.radians(THUMB_SWING_MAX_DEG) * s / THUMB_SWING_STEPS)
                        if self.clear_from(parent, shape, bend) >= 0.0:
                            over = 0.0
                            break
                    else:
                        self.swing = (0.0, 0.0, 0.0, 1.0)
            if over is None:
                if self.clear_from(parent, shape, [-MAX_EXTENSION] * 3) >= 0.0:
                    lo, hi = -MAX_EXTENSION, 0.0
                    for _ in range(ITERATIONS):
                        mid = 0.5 * (lo + hi)
                        if self.clear_from(parent, shape, [mid] * 3) >= 0.0:
                            lo = mid
                        else:
                            hi = mid
                    bend = [lo] * 3
                else:
                    xs = [-MAX_EXTENSION + (1.0 + MAX_EXTENSION) * s / (2 * STEPS) for s in range(2 * STEPS + 1)]
                    bend = [max(xs, key=lambda x: self.clear_from(parent, shape, [x] * 3))] * 3   # first of the best, as the C++ keeps it
                j = self.joints(parent, bend)
                pinned = min(range(3), key=lambda k: self.phalanx(shape, j, k))   # the first of the lowest, as the C++ keeps it
                free = pinned + 1
            else:
                bend = [over] * 3
        while free < 3:
            at = lambda x: [max(bend[i], x) if i >= free else bend[i] for i in range(3)]
            if self.clear_from(parent, shape, at(bend[free]), free) < 0.0:
                break
            lo, hi = bend[free], None
            for step in range(1, STEPS + 1):
                x = bend[free] + (1.0 - bend[free]) * step / STEPS
                if self.clear_from(parent, shape, at(x), free) < 0.0:
                    hi = x
                    break
                lo = x
            if hi is None:
                bend = at(1.0)
                break
            for _ in range(ITERATIONS):
                mid = 0.5 * (lo + hi)
                if self.clear_from(parent, shape, at(mid), free) >= 0.0:
                    lo = mid
                else:
                    hi = mid
            j = self.joints(parent, at(hi))
            touching = next((k for k in range(free, 3) if self.phalanx(shape, j, k) < 0.0), 2)
            bend = at(lo)
            free = touching + 1
        return bend


# -- the seat (FMOHandGrip::SeatInHand) --
def seat(hand, style, half, offset=0.0):
    """The handle's frame in the hand bone's space ((q, t)) and the hand's scale; offset: the item's GripHandOffset."""
    o, e1, e2, e3, span = hand.frame()
    fk = lambda v: _add(_add(_mul(e1, v[0]), _mul(e2, v[1])), _mul(e3, v[2]))
    scale = span / REF_SPAN
    Z = _norm(fk(POWER_AXIS))
    X = _norm(_perp(fk(POWER_FACE), Z))
    rot = _quat_from_axes(X, _cross(Z, X), Z)
    power_at = _add(o, _mul(fk(POWER_CENTRE), span))
    palm = (0.0, 0.0, 0.0)
    for m, k in zip(hand.metacarpal, hand.knuckle):
        palm = _add(palm, _mul(_add(m, k), 0.125))
    to_palm = _norm(_perp(_sub(palm, power_at), Z))
    palm_r = PALM_R * scale
    to_handle = _mul(Z, -offset)

    def palm_clear(at):
        sh = Shape((rot, _add(at, to_handle)), half, style)
        palm_min = min(sh.clear(_lerp(m, k, s / 4.0), palm_r) for m, k in zip(hand.metacarpal, hand.knuckle) for s in range(5))
        return min(palm_min, min(sh.clear(_lerp(hand.thenar[0], hand.thenar[1], s / 4.0), RADII["thumb"][0] * scale) for s in range(5)))

    sphere = style == "Sphere"
    start, along = (palm, _mul(to_palm, -1.0)) if sphere else (power_at, to_palm)
    reach = 10.0 * scale
    deepest = 0.0 if sphere else _dot(_sub(palm, power_at), to_palm)
    clear_s, in_s = (reach if sphere else deepest - reach), deepest
    if palm_clear(_add(start, _mul(along, clear_s))) < 0.0 or palm_clear(_add(start, _mul(along, in_s))) >= 0.0:
        raise ValueError("the handle %s never meets the palm" % (half,))
    for _ in range(24):
        mid = 0.5 * (clear_s + in_s)
        if palm_clear(_add(start, _mul(along, mid))) >= 0.0:
            clear_s = mid
        else:
            in_s = mid
    return (rot, _add(_add(start, _mul(along, clear_s)), to_handle)), scale


def hold(body, side, style, half, offset=0.0, fist=GRIP_POSE, fist_time=GRIP_POSE_TIME):
    """FMOHandGrip::Hold: the seat, then each finger wrapped in the fist's palm: {finger: (clearance cm, bends)}, the handle's frame."""
    hand = Hand(body, side, fist, fist_time)
    frame, scale = seat(hand, style, half, offset)
    sh = Shape(frame, half, style)
    res = {}
    for f in FINGERS:
        fg = Finger(hand.open[f], hand.fist[f], f, scale)
        bend = fg.wrap(hand.parent[f], sh)
        res[f] = (fg.clear_from(hand.parent[f], sh, bend), bend)
    return res, frame, scale


STANDARD_HANDLES = {"bow grip": ("Power", (2.15, 1.85, 5.0), 1.61), "axe haft": ("Power", (1.5, 1.8, 6.0), 0.0),
                    "stone": ("Sphere", (3.5, 2.5, 4.0), 0.0)}


def hold_all():
    """Every base, both hands, the standard handles: each finger's clearance (cm) and the hand's scale."""
    lines = []
    for base, body in bases().items():
        for name, (style, half, offset) in STANDARD_HANDLES.items():
            for side in "lr":
                res, frame, scale = hold(body, side, style, half, offset)
                lines.append("%-7s %s %-9s x%.2f | %s" % (base, side, name, scale, " ".join("%s %.2f" % (f[:2], res[f][0]) for f in FINGERS)))
    return "HELDGRIP\n" + "\n".join(lines)


def derive_standard(row_patch="Docs/RowPatches/2026-11-02-blender-items-DT_Items.json", bow="CompositeBow01"):
    """POWER_CENTRE / AXIS / FACE: the reference hand's (Femme's) seat of a bow, fitted to Sparrow's bow in her fist (bow_hand_fit, Nov 2), in the
    knuckles' frame per cm of span, its centre moved along the axis to the knuckles' middle. Paste the result into MOHandGrip.cpp and here."""
    with open(os.path.join(unreal.Paths.project_dir(), row_patch)) as f:
        tr = json.load(f)[bow]["heldVisual"]["LeftHandTransform"]
    q = (tr["Rotation"]["X"], tr["Rotation"]["Y"], tr["Rotation"]["Z"], tr["Rotation"]["W"])
    R = (tr["Translation"]["X"], tr["Translation"]["Y"], tr["Translation"]["Z"])
    hand = Hand(bases()[REFERENCE_BASE], "l")
    o, e1, e2, e3, span = hand.frame()
    U, D = _qrot(q, (0.0, 0.0, 1.0)), _qrot(q, (0.0, 1.0, 0.0))
    t = -_dot(_sub(R, o), e1) / _dot(U, e1)
    C = _add(R, _mul(U, t))
    ek = lambda v: (_dot(v, e1), _dot(v, e2), _dot(v, e3))
    centre = tuple(c / span for c in ek(_sub(C, o)))
    return json.dumps({"POWER_CENTRE": [round(c, 4) for c in centre], "POWER_AXIS": [round(c, 4) for c in ek(U)],
                       "POWER_FACE": [round(c, 4) for c in ek(_mul(D, -1.0))], "span": round(span, 3), "GripHandOffset": round(t, 3)})


# -- measuring an item's handle off its mesh --
# How each held item is held (Docs/Held_Grip.md, the table): the mode picks where the hand closes and which way the item faces.
#   end     a hafted tool: the hand near the butt, the head (the thicker end) toward the thumb, the face the way the head reaches out
#   sword   the grip between the pommel and the guard, the edge out of the front of the fist
#   shaft   a spear: the rear hand a third of the way up from the butt, the point toward the thumb
#   middle  a stick held at its middle (a fire drill)
#   tongs   both handles in one fist, squeezed together across the palm
#   shield  the grip behind the boss, the face out of the front of the fist
#   sphere  something round in the palm (a stone, an apple): its thinnest axis against the palm
ITEMS = {
    "HaftedAxe01": "end", "PolishedAxe01": "end", "IronAxe01": "end", "Adze01": "end", "StoneHoe01": "end", "StoneSickle01": "end",
    "SmithHammer01": "end", "StoneMaul01": "end", "CrudeClub01": "end", "Torch01": "end", "Atlatl01": "end", "DropSpindle01": "end",
    "BowDrill01": "end", "FireDrill01": "middle", "SmithTongs01": "tongs",
    "BronzeSword01": "sword", "IronSword01": "sword", "SteelSword01": "sword",
    "WoodenSpear01": "shaft", "FireHardenedSpear01": "shaft", "FlintSpear01": "shaft", "BronzeSpear01": "shaft", "IronSpear01": "shaft",
    "FishSpear01": "shaft",
    "WoodenShield01": "shield", "BronzeShield01": "shield", "IronShield01": "shield",
    "Hammerstone01": "sphere", "CrudeChopper01": "sphere", "StoneScraper01": "sphere", "Apple01": "sphere", "CookedMeat01": "sphere",
}
HAND_WIDTH = 8.5      # cm across the four fingers of a closed hand (an adult's): the window a handle is measured in
BUTT_MARGIN = 1.0     # the little finger's edge this far in from a butt
SHAFT_FRACTION = 0.3  # a spear's rear hand, from the butt
HANDLE_REACH = 4.0    # a handle's section: what lies this close to its line (a power grip closes on ~8 cm at most)


def _eig3(c):
    """A symmetric 3x3's eigenpairs (Jacobi), the largest first: [(value, unit vector)]."""
    a = [list(r) for r in c]
    v = [[1.0 if i == j else 0.0 for j in range(3)] for i in range(3)]
    for _ in range(60):
        p, q = max(((0, 1), (0, 2), (1, 2)), key=lambda pq: abs(a[pq[0]][pq[1]]))
        if abs(a[p][q]) < 1e-12:
            break
        theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q])
        t = (1.0 if theta >= 0 else -1.0) / (abs(theta) + math.sqrt(theta * theta + 1.0))
        c_ = 1.0 / math.sqrt(t * t + 1.0)
        s_ = t * c_
        apq = a[p][q]
        a[p][p] -= t * apq
        a[q][q] += t * apq
        a[p][q] = a[q][p] = 0.0
        for r in range(3):
            if r not in (p, q):
                arp, arq = a[r][p], a[r][q]
                a[r][p] = a[p][r] = c_ * arp - s_ * arq
                a[r][q] = a[q][r] = s_ * arp + c_ * arq
        for r in range(3):
            vrp, vrq = v[r][p], v[r][q]
            v[r][p] = c_ * vrp - s_ * vrq
            v[r][q] = s_ * vrp + c_ * vrq
    return sorted(((a[i][i], (v[0][i], v[1][i], v[2][i])) for i in range(3)), key=lambda e: -e[0])


def _pca(points):
    n = float(len(points))
    m = _mul((sum(p[0] for p in points), sum(p[1] for p in points), sum(p[2] for p in points)), 1.0 / n)
    c = [[0.0] * 3 for _ in range(3)]
    for p in points:
        d = _sub(p, m)
        for i in range(3):
            for j in range(3):
                c[i][j] += d[i] * d[j] / n
    return m, [vec for _, vec in _eig3(c)]


def _mesh(mesh, scale):
    """LOD0's vertices (cm as held) and triangles, every section."""
    verts, tris = [], []
    for s in range(mesh.get_num_sections(0)):
        sec = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)
        base = len(verts)
        verts += [(v.x * scale, v.y * scale, v.z * scale) for v in sec[0]]
        tris += [base + i for i in sec[1]]
    return verts, [(tris[i], tris[i + 1], tris[i + 2]) for i in range(0, len(tris) - 2, 3)]


def _sections(verts, tris, axis, origin, step=0.5):
    """Cross-sections along axis every step cm: [(s from the lowest vertex, centroid, points)], and the extent."""
    along = [_dot(_sub(v, origin), axis) for v in verts]
    lo, hi = min(along), max(along)
    n = int((hi - lo) / step)
    bins = [[] for _ in range(n + 1)]
    for t in tris:
        a = [along[i] for i in t]
        for k in range(max(0, int((min(a) - lo) / step)), min(n, int((max(a) - lo) / step)) + 1):
            bins[k].append(t)
    out = []
    for k in range(n + 1):
        s = lo + k * step
        pts = []
        for t in bins[k]:
            for i, j in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
                if (along[i] - s) * (along[j] - s) < 0.0:
                    pts.append(_lerp(verts[i], verts[j], (s - along[i]) / (along[j] - along[i])))
        if pts:
            c3 = _mul((sum(p[0] for p in pts), sum(p[1] for p in pts), sum(p[2] for p in pts)), 1.0 / len(pts))
            out.append((s - lo, c3, pts))
    return out, lo, hi


def _radius(sec):
    return max(_len(_sub(p, sec[1])) for p in sec[2])


def _flatness(sec):
    """A section's thin extent over its wide one (1 round, ~0.2 a blade)."""
    pts = [_sub(p, sec[1]) for p in sec[2]]
    if len(pts) < 3:
        return 1.0
    cov = [[sum(p[i] * p[j] for p in pts) / len(pts) for j in range(3)] for i in range(3)]
    e = _eig3(cov)
    return math.sqrt(max(e[1][0], 0.0) / max(e[0][0], 1e-9))


def _handle(verts, tris, mode):
    """GripTransform (rotation, location), GripHalfSize and a note for a long item (mode end / sword / shaft / middle / tongs)."""
    m, (A, B, C) = _pca(verts)
    secs, lo, hi = _sections(verts, tris, A, m)
    radii = [_radius(s) for s in secs]
    q = max(1, len(secs) // 4)
    e = max(1, len(secs) // 20)
    if mode == "sword":
        flat = [_flatness(s) for s in secs]                              # the blade is the long flat part: +A runs up it
        half = len(secs) // 2
        flip = sum(flat[:half]) / max(1, half) < sum(flat[half:]) / max(1, len(secs) - half)
    elif mode == "shaft":
        flip = min(radii[:e]) < min(radii[-e:])                          # the point tapers to nothing, the butt is blunt: +A to the point
    else:
        flip = max(radii[-q:]) < max(radii[:q])                          # the head is the thicker end: +A to it
    if flip:
        A = _mul(A, -1.0)
        secs, lo, hi = _sections(verts, tris, A, m)
        radii = [_radius(s) for s in secs]
    length = hi - lo
    s_of = [s[0] for s in secs]
    radii = [sorted(radii[max(0, k - 1):k + 2])[len(radii[max(0, k - 1):k + 2]) // 2] for k in range(len(radii))]   # a lone bump smoothed out
    thin = sorted(radii)[len(radii) // 3]   # a handle's radius: the thinner third of the item
    is_thin = [r <= max(thin * 1.6, thin + 0.6) for r in radii]
    if mode == "sword":
        # the grip: the round, narrow sections nearest the butt (past the pommel, before the guard and the flat blade); the hand just under
        # the guard on a long grip
        hilt = [k for k in range(len(secs)) if s_of[k] <= 0.4 * length]
        rounded = set(k for k in hilt if _flatness(secs[k]) >= 0.55 and s_of[k] > 1.5)   # not the pommel's very tip
        rr = sorted(radii[k] for k in rounded)
        narrow = rr[len(rr) // 4] if rr else thin
        ok = [k in rounded and radii[k] <= narrow * 1.5 + 0.4 for k in range(len(secs))]
        runs, k = [], 0
        while k < len(hilt):
            if ok[hilt[k]]:
                j = k
                while j + 1 < len(hilt) and ok[hilt[j + 1]]:
                    j += 1
                runs.append((hilt[k], hilt[j]))
                k = j + 1
            else:
                k += 1
        k, j = max(runs, key=lambda r: s_of[r[1]] - s_of[r[0]]) if runs else (hilt[0], hilt[0])   # the grip: the longest such run
        run, thin = (s_of[k], s_of[j]), narrow
        centre_s = 0.5 * (run[0] + run[1]) if run[1] - run[0] < HAND_WIDTH + 2 * BUTT_MARGIN else run[1] - 0.5 - 0.5 * HAND_WIDTH
    elif mode == "shaft":
        centre_s, run = SHAFT_FRACTION * length, (0.0, length)
    elif mode == "middle":
        centre_s, run = 0.5 * length, (0.0, length)
    else:   # end, tongs: the hand from the butt in, the little finger BUTT_MARGIN in from it
        i = 0
        while i < len(secs) and not is_thin[i] and s_of[i] < 0.25 * length:
            i += 1   # a knob or cap at the butt
        j = i
        while j < len(secs) and is_thin[j]:
            j += 1
        run = (s_of[min(i, len(secs) - 1)], s_of[max(i, j - 1)])
        if run[1] - run[0] < HAND_WIDTH + 2 * BUTT_MARGIN:
            centre_s = 0.5 * (run[0] + run[1])   # a short handle: the hand fills it
        else:
            centre_s = run[0] + BUTT_MARGIN + 0.5 * HAND_WIDTH
    window = [s for s in secs if abs(s[0] - centre_s) <= 0.5 * HAND_WIDTH] or sorted(secs, key=lambda s: abs(s[0] - centre_s))[:3]
    # the handle's own line through the window (a curved haft, a sickle), its centre at the hand's
    w0 = [s for s in window if s[0] <= centre_s] or window[:1]
    w1 = [s for s in window if s[0] >= centre_s] or window[-1:]
    p0 = _mul(tuple(sum(s[1][i] for s in w0) for i in range(3)), 1.0 / len(w0))
    p1 = _mul(tuple(sum(s[1][i] for s in w1) for i in range(3)), 1.0 / len(w1))
    Z = _norm(_sub(p1, p0)) if _len(_sub(p1, p0)) > 1.0 and _dot(_sub(p1, p0), A) > 0 else A
    centre = _mul(tuple(sum(s[1][i] for s in window) for i in range(3)), 1.0 / len(window))
    # cut again square to that line: the handle's true section, anything more than HANDLE_REACH off its line left out (a bow drill's cord)
    near = []
    zsecs, zlo, _zhi = _sections(verts, tris, Z, centre)
    for s, _c, pts in zsecs:
        if abs(s + zlo) <= 0.5 * HAND_WIDTH:
            near += [p for p in pts if _len(_perp(_sub(p, centre), Z)) <= HANDLE_REACH]
    if near:
        centre = _add(centre, _mul(tuple(sum(_perp(_sub(p, centre), Z)[i] for p in near) for i in range(3)), 1.0 / len(near)))
    # the face: across the handle's line, the head's main direction, toward where its mass is
    if mode == "tongs":
        head = near   # the two handles side by side: their spread is Y, so X is across it
    elif mode == "sword":
        head = [v for v in verts if _dot(_sub(v, centre), Z) > run[1] - centre_s + 2.0]   # the blade
    else:
        head = [v for v in verts if _dot(_sub(v, centre), Z) > 0.5 * (length - centre_s)]
    across = [_perp(_sub(v, centre), Z) for v in head] or [B]
    cm = _mul(tuple(sum(a[i] for a in across) for i in range(3)), 1.0 / len(across))
    cov = [[sum(a[i] * a[j] for a in across) / len(across) for j in range(3)] for i in range(3)]
    X = _norm(_perp(_eig3(cov)[0][1], Z))
    if mode == "tongs":
        X = _norm(_cross(Z, X))
    elif _dot(cm, X) < 0.0:
        X = _mul(X, -1.0)
    Y = _cross(Z, X)
    # its size: half-width along X, half-depth along Y (round the line's centre); half-length to the run's nearer end, a hand's half at least
    pts = [_perp(_sub(p, centre), Z) for p in near] or [_sub(p, centre) for s in window for p in s[2]]
    hx = max(abs(_dot(p, X)) for p in pts)
    hy = max(abs(_dot(p, Y)) for p in pts)
    hz = max(0.5 * HAND_WIDTH, min(centre_s - run[0], run[1] - centre_s, 40.0))
    note = "%s: %.1f cm long, handle %.1f x %.1f cm, hand %.1f cm from the butt (handle %.1f-%.1f)" % (
        mode, length, 2 * hx, 2 * hy, centre_s, run[0], run[1])
    return (_quat_from_axes(X, Y, Z), centre), (max(hx, 0.6), max(hy, 0.6), hz), note


def _sphere(verts):
    m, (A, B, C) = _pca(verts)
    ext = [(min(_dot(_sub(v, m), ax) for v in verts), max(_dot(_sub(v, m), ax) for v in verts)) for ax in (B, C, A)]
    centre = _add(m, _add(_add(_mul(B, 0.5 * sum(ext[0])), _mul(C, 0.5 * sum(ext[1]))), _mul(A, 0.5 * sum(ext[2]))))
    X, Z = B, A
    Y = _cross(Z, X)   # the thinnest axis (C or its opposite): against the palm
    half = tuple(0.5 * (e[1] - e[0]) for e in ext)
    return (_quat_from_axes(X, Y, Z), centre), half, "sphere: %.1f x %.1f x %.1f cm" % tuple(2 * h for h in half)


def _shield(verts):
    """The grip behind the boss: across the shield (its longer side), standing 2.5 cm off the back of the rim; the face out of the fist."""
    m, (A, B, N) = _pca(verts)
    rel = [_sub(v, m) for v in verts]
    r = [math.sqrt(_dot(p, A) ** 2 + _dot(p, B) ** 2) for p in rel]
    rmax = max(r)
    inner = [_dot(p, N) for p, ri in zip(rel, r) if ri < 0.25 * rmax]
    outer = [_dot(p, N) for p, ri in zip(rel, r) if ri > 0.8 * rmax]
    if inner and outer and sum(inner) / len(inner) < sum(outer) / len(outer):
        N = _mul(N, -1.0)   # the face is the side the middle (the boss) stands out on
    back = min(_dot(p, N) for p, ri in zip(rel, r) if ri > 0.8 * rmax)   # the back of the rim
    centre = _add(m, _mul(N, back - 2.5))
    X, Z = N, A
    return (_quat_from_axes(X, _cross(Z, X), Z), centre), (1.4, 1.4, 5.5), "shield: %.0f cm across" % (2 * rmax)


def measure(row):
    """({heldVisual patch}, note) for a DT_Items row (the table's JSON), measured off its held (else world) mesh at its world scale."""
    hv, wv = row.get("HeldVisual") or {}, row.get("WorldVisual") or {}
    mode = ITEMS[row["Name"]]
    path = hv.get("StaticMesh") if hv.get("StaticMesh") not in (None, "", "None") else wv.get("StaticMesh")
    mesh = unreal.load_asset(path.split(".")[0])
    scale = (wv.get("RelativeTransform") or {}).get("Scale3D", {}).get("X", 1.0) or 1.0
    verts, tris = _mesh(mesh, scale)
    if mode == "sphere":
        (q, t), half, note = _sphere(verts)
        style = "Sphere"
    elif mode == "shield":
        (q, t), half, note = _shield(verts)
        style = "Power"
    else:
        (q, t), half, note = _handle(verts, tris, mode)
        style = "Power"
    return {"GripStyle": style,
            "GripTransform": {"Rotation": {"X": q[0], "Y": q[1], "Z": q[2], "W": q[3]}, "Translation": {"X": t[0], "Y": t[1], "Z": t[2]},
                              "Scale3D": {"X": 1, "Y": 1, "Z": 1}},
            "GripHalfSize": {"X": round(half[0], 2), "Y": round(half[1], 2), "Z": round(half[2], 2)}, "GripHandOffset": 0.0,
            "GripPose": GRIP_POSE + "." + GRIP_POSE.split("/")[-1], "GripPoseTime": GRIP_POSE_TIME}, note


def measure_all(table="/MOFramework/Data/DT_Items", only=None):
    """Every ITEMS row measured: Saved/HeldGrip/rows.json ({row: {"heldVisual": patch}}) for `ue.py rows set`, and a line per item."""
    rows = {r["Name"]: r for r in json.loads(unreal.load_asset(table).export_to_json_string())}
    folder = os.path.join(unreal.Paths.project_saved_dir(), "HeldGrip")
    path = os.path.join(folder, "rows.json")
    patch = json.load(open(path)) if only and os.path.isfile(path) else {}   # a few items: into the patch beside the rest
    lines = []
    for name in (only or ITEMS):
        if name not in rows:
            lines.append("%s: not in %s" % (name, table))
            continue
        try:
            hv, note = measure(rows[name])
        except Exception as e:   # one bad mesh must not stop the rest
            lines.append("%s: FAILED %r" % (name, e))
            continue
        patch.setdefault(name, {})["heldVisual"] = hv
        h = hv["GripHalfSize"]
        lines.append("%-20s %-6s half %.2f x %.2f x %.2f | %s" % (name, hv["GripStyle"], h["X"], h["Y"], h["Z"], note))
    os.makedirs(folder, exist_ok=True)
    with open(path, "w") as f:
        json.dump(patch, f, indent=1)
    return "HELDGRIP measured %d -> Saved/HeldGrip/rows.json\n%s" % (len(lines), "\n".join(lines))


# -- a bow (Tools/blender_items.py: the bow's mesh has its grip's centre at the root, the handle along +Z, the draw along +Y) --
BOW_GRIP_ROTATION = (0.0, 0.0, -math.sqrt(0.5), math.sqrt(0.5))   # GripTransform: +X (out of the fist) the bow's back, -Y


def bow_grip(draw_path, grip_cm, base=REFERENCE_BASE, clip=GRIP_POSE, clip_time=GRIP_POSE_TIME):
    """A bow's GripTransform / GripHandOffset: the standard Power seat in the reference body's hand, the hand along the grip until the nocking
    point is level with the drawing fingers (index_02_r / middle_02_r, UMOBowMeshComponent's) at Sparrow's full draw. grip_cm: the Blender
    build's (<Id>.json: half-width across the bow, half-depth along the draw, half-length); the row's GripHalfSize is in the grip's axes (X out
    of the fist = along the draw). Returns the row fields and how far the string must go there (draw_cm against the bow's full_cm; the bow is
    rebuilt when they differ by more than a centimetre)."""
    half = (grip_cm[1], grip_cm[0], grip_cm[2])
    body = bases()[base]
    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property("optional_skeletal_mesh", unreal.load_asset(body))
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(unreal.load_asset(clip), clip_time, opts)
    W = unreal.AnimPoseSpaces.WORLD
    at = lambda b: _tq(unreal.AnimPoseExtensions.get_bone_pose(pose, b, W))
    hq, ht = at("hand_l")
    F = _mul(_add(at("index_02_r")[1], at("middle_02_r")[1]), 0.5)
    F = _qrot(_qinv(hq), _sub(F, ht))   # the drawing fingers in the bow hand's space
    draw = unreal.load_asset(draw_path)
    last = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(
        draw, draw.get_editor_property("sequence_length"), unreal.AnimPoseEvaluationOptions()), "string_nock", W).translation
    full, nock_up = last.y, last.z
    hand = Hand(body, "l")
    offset = 2.0
    for _ in range(5):
        (q, R), scale = seat(hand, "Power", half, offset)
        U, Dr = _qrot(q, (0.0, 0.0, 1.0)), _mul(_qrot(q, (1.0, 0.0, 0.0)), -1.0)   # the bow's up and draw (its -X: toward the archer)
        nock = _add(R, _mul(U, nock_up))
        offset -= _dot(_sub(F, nock), U)
    w = _sub(F, nock)
    along = _dot(w, Dr)
    return {"GripStyle": "Power",
            "GripTransform": {"Rotation": dict(zip("XYZW", BOW_GRIP_ROTATION)), "Translation": {"X": 0, "Y": 0, "Z": 0}, "Scale3D": {"X": 1, "Y": 1, "Z": 1}},
            "GripHandOffset": round(offset, 3), "GripHalfSize": {"X": half[0], "Y": half[1], "Z": half[2]},
            "GripPose": GRIP_POSE + "." + GRIP_POSE.split("/")[-1], "GripPoseTime": GRIP_POSE_TIME}, {
        "draw_cm": round(along, 2), "full_cm": round(full, 2), "draw_miss_cm": round(along - full, 2),
        "lateral_cm": round(_len(_perp(w, Dr)), 2), "base": base}


def bow_rows(names=("SimpleBow01", "SinewBackedBow01", "CompositeBow01"), table="/MOFramework/Data/DT_Items"):
    """The bows' grip rows (their draw clips as they are in the table, their grips from the Blender build), into Saved/HeldGrip/rows.json."""
    rows = {r["Name"]: r for r in json.loads(unreal.load_asset(table).export_to_json_string())}
    path = os.path.join(unreal.Paths.project_saved_dir(), "HeldGrip", "rows.json")
    patch = json.load(open(path)) if os.path.isfile(path) else {}
    lines = []
    for n in names:
        hv = rows[n]["HeldVisual"]
        with open(os.path.join(unreal.Paths.project_saved_dir(), "BlenderItems", n, n + ".json")) as f:
            grip_cm = json.load(f)["grip_cm"]   # the build's own numbers (Tools/blender_items.py), never the row's (axes differ)
        fields, check = bow_grip(hv["DrawAnimation"].split(".")[0], grip_cm)
        patch[n] = {"heldVisual": fields}
        lines.append("%s: hand %.2f cm up the grip, %s" % (n, fields["GripHandOffset"], check))
    with open(path, "w") as f:
        json.dump(patch, f, indent=1)
    return "HELDGRIP bows\n" + "\n".join(lines)
