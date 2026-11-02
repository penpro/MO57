"""Where a held bow sits in the survivor's left hand (Nov 2 2026), from her full draw: Sparrow's RMB_Loop on the MetaHuman.

    python Tools/ue.py py -c "import bow_hand_fit as f; out(f.fit('<draw clip>', grip=(1.85, 2.15, 4.7)))"

The bow (Tools/blender_items_blender.py) has its grip centre at the root, its handle along +Z, its draw along +Y (the clip's nocking-point
travel) and its nocking point NOCK_Z above the grip. `grip` is the wrapped handle's outer half-width, half-depth (along the draw) and
half-length, from the bow's <Id>.json ("grip_cm"); it is also the item row's HeldVisual.GripHalfSize.

The seat (Nov 2, Wes: "her fingers are back to being all fucked up"; the first seat put the grip at the centroid of her finger joints -- inside
her fingers -- and ran it through them):
  - Sparrow's own bow lies along her knuckle line, in the hollow just behind the knuckles (SPARROW_GRIP: measured in the source clip in a frame
    of her knuckles; the retarget gives the MetaHuman the same fist since the retarget pose's arms and fingers were aligned to hers).
  - Our handles are thicker than Paragon's stylized one, so the fist opens round them: in game UMOBodyRetargetAnimInstance closes each finger
    from the open hand toward this fist until a phalanx touches the handle. `wrap` below is the same closing (FMOBodyRetargetAnimInstanceProxy::
    WrapFinger), so the fit sees the hand the game will show.
  - The handle's axis is searched round Sparrow's (offset across it, tilt of it) for: the palm against the handle and not in it, the open
    fingers clear of it (or nothing can close on it), each closing finger reaching it, the axis near hers, and the string at full draw on the
    drawing fingers F (between index_02_r / middle_02_r, where UMOBowMeshComponent holds it).
The bow's draw points from the axis at F; its root slides along the axis until the nocking point is level with F. Returns the transform
relative to hand_l (the socket the bow hangs from), each finger's closing and clearance, and how far the string misses F (draw_miss_cm: the
bow's own full draw against the distance from the seat to F -- the bow is rebuilt for it when it is off by more than a centimetre).
"""
import json
import math

import unreal

CLIP = "/MOFramework/Characters/Bow/RMB_Loop_MH"
CLIP_T = 0.33
DRAW_START = "/MOFramework/Characters/Bow/RMB_Drawback_MH"   # brace: the string where the draw begins
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
# FMOBodyRetargetAnimInstanceProxy::GripFingerRadii / GripTipFraction (keep them the same)
RADII = {"thumb": (1.0, 0.92, 0.8), "index": (0.85, 0.78, 0.68), "middle": (0.86, 0.8, 0.7), "ring": (0.8, 0.74, 0.65),
         "pinky": (0.72, 0.66, 0.58)}
TIP = 0.85
SAMPLES = 4
STEPS = 16        # FMOBodyRetargetAnimInstanceProxy::WrapFinger: the sweep walked for the first touch, then bisected
ITERATIONS = 10
PALM_R = 1.2      # the palm's soft side off the metacarpals (cm)
# Sparrow's bow in her fist (RMB_Loop at 0.33, Paragon: Sparrow): bow_base from the centroid of her four 01 knuckles, in the knuckle frame
# (e1 little finger -> index, e2 toward the curled fingertips square to e1, e3 = e1 x e2), per cm of knuckle span (index_01 -> pinky_01:
# hers 7.4 cm), and the shaft's direction there. Measured Nov 2.
SPARROW_GRIP = (1.1 / 7.4, 2.2 / 7.4, 3.0 / 7.4)
SPARROW_AXIS = (1.0, 0.02, 0.09)


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


def _rotate(v, axis, ang):
    c, s = math.cos(ang), math.sin(ang)
    return _add(_add(_mul(v, c), _mul(_cross(axis, v), s)), _mul(axis, _dot(axis, v) * (1.0 - c)))


def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def _qrot(q, v):
    x, y, z, w = q
    tx, ty, tz = 2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx))


def _qslerp(a, b, t):
    d = sum(a[i] * b[i] for i in range(4))
    if d < 0:
        b, d = tuple(-c for c in b), -d
    if d > 0.9995:
        r = tuple(a[i] + (b[i] - a[i]) * t for i in range(4))
    else:
        th = math.acos(d)
        s0, s1 = math.sin((1 - t) * th) / math.sin(th), math.sin(t * th) / math.sin(th)
        r = tuple(a[i] * s0 + b[i] * s1 for i in range(4))
    n = math.sqrt(sum(c * c for c in r))
    return tuple(c / n for c in r)


def _compose(local, parent):
    """child component space = local * parent."""
    (lq, lt), (pq, pt) = local, parent
    return (_qmul(pq, lq), _add(_qrot(pq, lt), pt))


def _tq(t):
    return ((t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w), (t.translation.x, t.translation.y, t.translation.z))


class Hand:
    """Her left hand at the full draw: the fingers' parents as animated, each finger's open (reference) and fist (the clip) local joints."""

    def __init__(self, clip=CLIP, t=CLIP_T, side="l"):
        anim = unreal.load_asset(clip)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())
        ref = unreal.AnimPoseExtensions.get_reference_pose(anim.get_editor_property("skeleton"))
        W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
        get = lambda p, b, sp: _tq(unreal.AnimPoseExtensions.get_bone_pose(p, b, sp))
        self.pose = pose
        self.at = lambda b: get(pose, b, W)[1]
        self.chains = {}
        for f in FINGERS:
            names = ["%s_%s_%s" % (f, j, side) for j in ("01", "02", "03")]
            j01, l01 = get(pose, names[0], W), get(pose, names[0], L)
            pq = _qmul(j01[0], (-l01[0][0], -l01[0][1], -l01[0][2], l01[0][3]))   # the parent: the 01's world over its local
            parent = (pq, _sub(j01[1], _qrot(pq, l01[1])))
            self.chains[f] = (parent, [get(ref, n, L) for n in names], [get(pose, n, L)[0] for n in names])
        self.palm = [(self.at("%s_metacarpal_%s" % (f, side)), self.at("%s_01_%s" % (f, side))) for f in FINGERS[1:]]
        self.hand = get(pose, "hand_" + side, W)

    def joints(self, f, bend):
        parent, opened, fist = self.chains[f]
        out, cur = [], parent
        for k in range(3):
            cur = _compose((_qslerp(opened[k][0], fist[k], bend[k]), opened[k][1]), cur)
            out.append(cur[1])
        out.append(_add(out[2], _mul(_sub(out[2], out[1]), TIP)))
        return out


class Handle:
    """FMOBodyRetargetAnimInstanceProxy::HandleClearance: an elliptic cylinder at R along U, Y along D (the draw)."""

    def __init__(self, R, U, D, half):
        self.R, self.U, self.D, self.X = R, U, D, _cross(D, U)
        self.a, self.b, self.h = half

    def clear(self, p, r):
        q = _sub(p, self.R)
        x, y, z = _dot(q, self.X), _dot(q, self.D), _dot(q, self.U)
        if abs(z) > self.h + 2.0:
            return 99.0
        rn = math.sqrt((x / self.a) ** 2 + (y / self.b) ** 2)
        radial = math.sqrt(x * x + y * y)
        d = radial * (1.0 - 1.0 / rn) if rn > 1e-6 else -min(self.a, self.b)
        if abs(z) > self.h:
            d = max(d, abs(z) - self.h)
        return d - r


def _phalanx(handle, f, j, k):
    r = RADII[f]
    r0, r1 = r[k], (r[k + 1] if k < 2 else r[2] * 0.9)
    return min(handle.clear(_lerp(j[k], j[k + 1], i / SAMPLES), r0 + (r1 - r0) * i / SAMPLES) for i in range(1, SAMPLES + 1))


def _clear_from(hand, handle, f, bend, first):
    j = hand.joints(f, bend)
    return min(_phalanx(handle, f, j, k) for k in range(first, 3))


def wrap(hand, handle, f):
    """FMOBodyRetargetAnimInstanceProxy::WrapFinger: the free joints bend together until a phalanx touches; it and the joints before it stop."""
    bend, free = [0.0, 0.0, 0.0], 0
    while free < 3:
        at = lambda x: [max(bend[i], x) if i >= free else bend[i] for i in range(3)]
        if _clear_from(hand, handle, f, at(bend[free]), free) < 0.0:
            break
        # walk the sweep for the first touch (testing only its end misses a handle the finger passes through and out of)
        lo, hi = bend[free], None
        for step in range(1, STEPS + 1):
            x = bend[free] + (1.0 - bend[free]) * step / STEPS
            if _clear_from(hand, handle, f, at(x), free) < 0.0:
                hi = x
                break
            lo = x
        if hi is None:
            bend = at(1.0)
            break
        for _ in range(ITERATIONS):
            mid = 0.5 * (lo + hi)
            if _clear_from(hand, handle, f, at(mid), free) >= 0.0:
                lo = mid
            else:
                hi = mid
        j = hand.joints(f, at(hi))
        touching = next((k for k in range(free, 3) if _phalanx(handle, f, j, k) < 0.0), 2)
        bend = at(lo)
        free = touching + 1
    return bend, _clear_from(hand, handle, f, bend, 0)


def knuckle_frame(at):
    k = [at("%s_01_l" % f) for f in FINGERS[1:]]
    t = [at("%s_03_l" % f) for f in FINGERS[1:]]
    o = _mul(_add(_add(k[0], k[1]), _add(k[2], k[3])), 0.25)
    e1 = _norm(_sub(k[0], k[3]))
    c = _sub(_mul(_add(_add(t[0], t[1]), _add(t[2], t[3])), 0.25), o)
    e2 = _norm(_sub(c, _mul(e1, _dot(c, e1))))
    return o, e1, e2, _cross(e1, e2), _len(_sub(k[0], k[3]))


def seat(hand, F, full, nock_up, grip):
    """The handle's axis in her fist (see the header). Returns R (the bow's root), U (up the handle), D (the draw), the miss, the score."""
    o, e1, e2, e3, span = knuckle_frame(hand.at)
    P0 = _add(o, _add(_add(_mul(e1, SPARROW_GRIP[0] * span), _mul(e2, SPARROW_GRIP[1] * span)), _mul(e3, SPARROW_GRIP[2] * span)))
    U0 = _norm(_add(_add(_mul(e1, SPARROW_AXIS[0]), _mul(e2, SPARROW_AXIS[1])), _mul(e3, SPARROW_AXIS[2])))
    E1 = _norm(_cross(U0, _sub(F, P0)))
    E2 = _cross(E1, U0)

    def frame(P, U):
        s = _dot(_sub(F, P), U) - nock_up
        R = _add(P, _mul(U, s))
        w = _sub(_sub(F, R), _mul(U, nock_up))
        return R, _norm(w), _len(w) - full

    def quick(P, U):
        R, D, miss = frame(P, U)
        hd = Handle(R, U, D, grip)
        palm = min(min(hd.clear(_lerp(a, b, i / SAMPLES), PALM_R) for i in range(SAMPLES + 1)) for a, b in hand.palm)
        dev = math.degrees(math.acos(max(-1.0, min(1.0, _dot(U, U0)))))
        cost = 25 * min(0.0, palm) ** 2 + max(0.0, palm - 0.2) ** 2 + 0.05 * miss * miss + (max(0.0, dev - 6.0) / 8.0) ** 2
        cost += 0.005 * _dot(_sub(P, P0), _sub(P, P0))   # near where Sparrow's bow sits, other things equal
        for f in FINGERS:
            w8 = 1.0 if f != "thumb" else 0.3
            cost += 25 * w8 * min(0.0, _clear_from(hand, hd, f, [0.0, 0.0, 0.0], 0)) ** 2
            cost += w8 * max(0.0, _clear_from(hand, hd, f, [1.0, 1.0, 1.0], 0)) ** 2
        return cost, R, D, miss, palm, hd

    def search(c1, c2, a1, a2, span_cm, step, tilt, tstep):
        n, m = int(round(span_cm / step)), int(round(tilt / tstep))
        res = []
        for i in range(-n, n + 1):
            for j in range(-n, n + 1):
                P = _add(_add(P0, _mul(E1, c1 + i * step)), _mul(E2, c2 + j * step))
                for k in range(-m, m + 1):
                    for l in range(-m, m + 1):
                        t1, t2 = a1 + k * tstep, a2 + l * tstep
                        U = _norm(_rotate(_rotate(U0, E1, math.radians(t1)), E2, math.radians(t2)))
                        res.append((quick(P, U)[0], c1 + i * step, c2 + j * step, t1, t2))
        res.sort()
        return res

    coarse = search(0.0, 0.0, 0.0, 0.0, 3.0, 0.5, 20.0, 5.0)
    fine = search(*coarse[0][1:], 0.5, 0.1, 5.0, 1.0)
    best = None
    for cand in fine[:30]:
        _, o1, o2, t1, t2 = cand
        P = _add(_add(P0, _mul(E1, o1)), _mul(E2, o2))
        U = _norm(_rotate(_rotate(U0, E1, math.radians(t1)), E2, math.radians(t2)))
        cost, R, D, miss, palm, hd = quick(P, U)
        fingers = {f: wrap(hand, hd, f) for f in FINGERS}
        for f, (bend, cl) in fingers.items():
            w8 = 1.0 if f != "thumb" else 0.3
            cost += w8 * (4 * max(0.0, cl - 0.1) ** 2 + 4 * max(0.0, 0.45 - bend[0]) ** 2)
        if best is None or cost < best[0]:
            best = (cost, R, U, D, miss, palm, fingers, (o1, o2, t1, t2))
    return best


def fit(draw_path, grip, t=CLIP_T):
    hand = Hand(CLIP, t)
    F = _mul(_add(hand.at("index_02_r"), hand.at("middle_02_r")), 0.5)

    # the bow's own frame, from its draw clip: the nocking point at full draw (the last frame)
    draw = unreal.load_asset(draw_path)
    L = draw.get_editor_property("sequence_length")
    last = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(draw, L, unreal.AnimPoseEvaluationOptions()),
                                                   "string_nock", unreal.AnimPoseSpaces.WORLD).translation
    full, nock_up = last.y, last.z

    cost, R, U, D, miss, palm, fingers, params = seat(hand, F, full, nock_up, grip)
    rot = unreal.MathLibrary.make_rot_from_yz(unreal.Vector(*D), unreal.Vector(*U))
    bow_world = unreal.Transform(unreal.Vector(*R), rot, unreal.Vector(1, 1, 1))
    hq, ht = hand.hand
    hand_t = unreal.Transform(unreal.Vector(*ht), unreal.Quat(*hq).rotator(), unreal.Vector(1, 1, 1))
    rel = bow_world.make_relative(hand_t)

    # brace: where the drawing fingers stand from this seat as the draw begins (the string is taken there)
    start = Hand(DRAW_START, 0.0)
    Fs = _mul(_add(start.at("index_02_r"), start.at("middle_02_r")), 0.5)
    hs = start.hand
    bow_s = _compose((_tq(rel)[0], _tq(rel)[1]), hs)
    Rs, Ds = bow_s[1], _qrot(bow_s[0], (0.0, 1.0, 0.0))
    brace = _dot(_sub(Fs, Rs), Ds)
    return json.dumps({
        "string_F": [round(v, 2) for v in F], "bow_full_cm": round(full, 2), "nock_up_cm": round(nock_up, 2), "grip": list(grip),
        "seat_to_F_cm": round(full + miss, 2), "draw_miss_cm": round(miss, 2), "brace_from_seat_cm": round(brace, 2),
        "palm_clearance_cm": round(palm, 2), "axis_offset_cm": [round(params[0], 2), round(params[1], 2)],
        "axis_tilt_deg": [round(params[2], 1), round(params[3], 1)], "score": round(cost, 4),
        "fingers": {f: {"bend": [round(b, 2) for b in bend], "clearance_cm": round(cl, 2)} for f, (bend, cl) in fingers.items()},
        "relative_to_hand_l": {"loc": [round(rel.translation.x, 3), round(rel.translation.y, 3), round(rel.translation.z, 3)],
                               "rot": [round(rel.rotation.x, 6), round(rel.rotation.y, 6), round(rel.rotation.z, 6), round(rel.rotation.w, 6)]}})
