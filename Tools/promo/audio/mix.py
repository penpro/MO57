"""MO57 promo audio: the final mix. Music (score.py's stems), the game's own sound under each shot, sound design on the cuts; mastered to about -14 LUFS, -1 dBTP for the web.

    python Tools/promo/audio/mix.py                     Saved/Promo/audio/mix_master.wav, with a loudness report
    python Tools/promo/audio/mix.py --music PATH.wav    the same mix over another music file (one of the game's own tracks, say) instead of the score

The game's sound is recorded from the game (Tools/promo/capture.py bed <name>: the master submix, real time) into Saved/Promo/audio/game/<name>.wav; one-shots exported from the game's bank are in
Saved/Promo/audio/game/oneshots/. A bed or one-shot not recorded yet falls back to a synthesised stand-in, and the report says which, so nothing silently ships as a fake.
"""
import argparse
import json
import os
import re
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import dsp  # noqa: E402
from dsp import RNG, SR, add_at, fade, fft_eq, highpass_curve, lowpass_curve, read_wav, resample, rms_db, spectral, stereo, write_wav  # noqa: E402
import film  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
AUD = os.path.join(ROOT, "Saved", "Promo", "audio")
GAME = os.path.join(AUD, "game")
TARGET_LUFS = -14.0
TRUE_PEAK = -1.0
BED_RMS_DB = -24.0     # every recorded bed is brought to this before its level in BEDS: the game's own beds range from -33 to -60 LUFS at the master
TOTAL = film.seconds(film.END_BAR)

# the game's bed under each shot, its level (dB against the music's full level), and how long it crossfades in. Shots not here have no bed.
BEDS = {
    "G01_pod": ("shore_dawn", -12.0), "G02_survivor": ("shore_dawn", -13.0), "G03_feet": ("meadow_dawn", -14.0),
    "G04_forest": ("forest_day", -17.0), "G05_sea": ("sea", -13.0), "G06_rainforest": ("rainforest", -15.0), "G07_coast": ("sea", -11.0), "G08_ravine": ("wind_high", -15.0), "G09_islands": ("wind_high", -20.0),
    "G10_trek": ("wind_high", -24.0), "G11_climb": ("wind_high", -24.0), "G12_swim": ("lake", -22.0), "G14_wall": ("forest_day", -26.0), "G15_fire": ("fire", -18.0), "G16_wolf": ("forest_day", -24.0),
    "G18_cave": ("cave", -14.0), "G19_survivor_meet": ("forest_day", -22.0), "G20_fire_group": ("fire_night", -16.0), "G22_coop": ("wind_high", -22.0),
}
# the music's level per bar range (dB), so the words and the world get room: (from bar, to bar, dB at start, dB at end)
MUSIC_RIDE = [(0, 5.5, 0, 0), (5.5, 10, -1, -1), (10, 16, -1, 0), (16, 25, 0, 0), (25, 26, 0, -2), (26, 31, -1, 0), (31, 34, 0, 0), (34, 35, 0, -60)]     # the score is 36 bars: it goes out with the picture's fade (Wes, Nov 5: "shorten the outro a bit")


def bar_t(b):
    return film.seconds(b)


# ---------- stand-ins (the report names every one used) ----------

def noise_bed(seconds, lo, hi, gain):
    n = int(seconds * SR)
    x = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1)
    x = fft_eq(x, dsp.combine(highpass_curve(lo, 1.5), lowpass_curve(hi, 1.5)))
    lfo = 0.65 + 0.35 * dsp.drift(n, 0.25)
    return (x.T * lfo).T * gain


STANDIN = {
    "shore_dawn": lambda s: noise_bed(s, 150, 2400, 0.12), "meadow_dawn": lambda s: noise_bed(s, 300, 5000, 0.05), "forest_day": lambda s: noise_bed(s, 400, 6000, 0.05),
    "wind_high": lambda s: noise_bed(s, 120, 1500, 0.12), "rainforest": lambda s: noise_bed(s, 800, 9000, 0.05), "sea": lambda s: noise_bed(s, 100, 3000, 0.16), "lake": lambda s: noise_bed(s, 400, 3000, 0.04),
    "fire": lambda s: noise_bed(s, 200, 5000, 0.06), "fire_night": lambda s: noise_bed(s, 200, 5000, 0.05), "cave": lambda s: noise_bed(s, 60, 500, 0.08),
}


def load_game(name, seconds):
    """A recorded bed, looped with a crossfade if it is short; or a stand-in. Returns (buffer, real?)."""
    path = os.path.join(GAME, name + ".wav")
    if os.path.isfile(path):
        x, rate = read_wav(path)
        x = resample(x, rate)
        x = x * 10.0 ** ((BED_RMS_DB - dsp.rms_db(x)) / 20.0)
        n = int(seconds * SR)
        if len(x) >= n:
            return x[:n], True
        xf = int(0.5 * SR)
        out = np.zeros((n, 2))
        i = 0
        while i < n:
            part = fade(x, 0.5, 0.5)
            k = min(len(part), n - i)
            out[i:i + k] += part[:k]
            i += len(x) - xf
        return out, True
    return STANDIN[name](seconds), False


def oneshot(name):
    """A sound exported from the game's bank, or None."""
    path = os.path.join(GAME, "oneshots", name + ".wav")
    if os.path.isfile(path):
        x, rate = read_wav(path)
        return resample(x, rate)
    return None


# ---------- sound design: synthesised on purpose (nothing in the game makes these) ----------

def entry_rumble(seconds):
    """The pod's fall: a roar that grows, crackle over the top."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1)

    def gain(f, times):
        p = np.clip(times / seconds, 0, 1)[:, None]
        centre = 90.0 + 700.0 * p ** 2
        return 1.0 / np.sqrt(1.0 + (f[None, :] / centre) ** 3.0)

    roar = spectral(x, gain, nfft=4096, hop=1024)
    swell = (t / seconds) ** 1.7
    flutter = 0.7 + 0.3 * dsp.drift(n, 9.0)
    roar = (roar.T * swell * flutter).T * 1.3
    crackle = np.zeros((n, 2))
    for i in range(int(seconds * 42)):
        at = int(RNG.uniform(0.2, 1.0) ** 0.5 * n * 0.999)
        k = int(RNG.uniform(0.002, 0.012) * SR)
        burst = RNG.standard_normal(k) * np.exp(-np.arange(k) / (k * 0.25))
        if at + k < n:
            crackle[at:at + k] += dsp.pan(burst, RNG.uniform(-0.7, 0.7)) * (0.15 + 0.3 * (at / n))
    crackle = fft_eq(crackle, highpass_curve(1200.0, 1.5))
    return roar + crackle * 0.4


def streak_pass(seconds):
    """Something very fast going past and into the ground: a falling whistle, a tearing noise crossing the stage, cut dead by the edit."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    p = t / seconds
    f = 2600.0 * (0.1 ** p)
    tone = np.sin(2 * np.pi * np.cumsum(f) / SR) * (p ** 2.0) * 0.05
    noise = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1)

    def gain(fr, times):
        q = np.clip(times / seconds, 0, 1)[:, None]
        centre = 5200.0 * (0.14 ** q)
        return np.exp(-0.5 * (np.log2(np.maximum(fr[None, :], 1.0) / centre) / 1.1) ** 2)

    tear = spectral(noise, gain, nfft=2048, hop=512) * 0.55
    env = p ** 1.6
    a = (0.9 * (p * 2 - 1) + 1) * np.pi / 4
    out = np.zeros((n, 2))
    out[:, 0] = (tear[:, 0] * env + tone) * np.cos(a)
    out[:, 1] = (tear[:, 1] * env + tone) * np.sin(a)
    return out


def whoosh(seconds=0.6, peak=0.65, gain_=0.25):
    """A soft air move for a cut."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1)
    y = fft_eq(x, dsp.combine(highpass_curve(280, 1.2), lowpass_curve(4200, 1.2)))
    env = np.minimum(t / (peak * seconds), (seconds - t) / ((1 - peak) * seconds)) ** 2
    return (y.T * env).T * gain_


def tick():
    n = int(0.09 * SR)
    t = np.arange(n) / SR
    y = np.sin(2 * np.pi * 1750 * t) * np.exp(-t / 0.008) * 0.4 + RNG.standard_normal(n) * np.exp(-t / 0.002) * 0.2
    return stereo(y)


def cue_list():
    """(time, what, gain dB): sound design on the film's timeline."""
    cues = []
    cues.append((bar_t(2.25), entry_rumble(bar_t(4.25) - bar_t(2.25)), -9.0))
    cues.append((bar_t(4.25), streak_pass(bar_t(5.0) - bar_t(4.25)), -4.0))
    for s in film.SHOTS:
        if s.kind == "game" and s.bar not in (5.5, 16.0, 18.0, 26.0, 31.0) and s.bars <= 1.0:
            cues.append((bar_t(s.bar) - 0.3, whoosh(0.6), -22.0))
    for s in ("G17_wound", "G21_possess"):
        shot = next(x for x in film.SHOTS if x.id == s)
        for k in (0.1, 0.45):
            cues.append((bar_t(shot.bar + k), tick(), -22.0))
    return cues


# ---------- one-shots from the game on the picture ----------

def footsteps(report):
    """Her steps under the walking shot: the game's own grass tread."""
    out = []
    shot = next(x for x in film.SHOTS if x.id == "G03_feet")
    a = [oneshot("SFX_Footsteps_GrassTread%d" % i) for i in (1, 2)]
    if a[0] is None:
        report["standin"].append("footsteps (none: waiting for the game's bank export)")
        return out
    report["real"].append("SFX_Footsteps_GrassTread (G03_feet)")
    stride = 0.5                # the walk's cadence, read off the take: a heel strike every 12 film frames, the first at frame 8
    t = bar_t(shot.bar) + 8.0 / 24.0
    i = 0
    while t < bar_t(shot.bar + shot.bars) - 0.2:
        out.append((t + RNG.normal(0, 0.008), a[i % 2], -18.0 + RNG.normal(0, 1.0)))
        t += stride
        i += 1
    return out


def on_picture(report):
    """The game's own object sounds on what the picture shows: rods going into the wall, a hand on rock."""
    out = []
    wall = next(x for x in film.SHOTS if x.id == "G14_wall")
    knocks = [oneshot("SFX_Object_WoodKnock%d" % i) for i in (1, 2)]
    if knocks[0] is not None:
        t = bar_t(wall.bar) + 0.1
        i = 0
        while t < bar_t(wall.bar + wall.bars / 2.0) - 0.1:     # the stakes and rods: the first half of the shot
            out.append((t, knocks[i % 2], -24.0 + RNG.normal(0, 2.0)))
            t += 0.24 + RNG.uniform(0, 0.12)
            i += 1
        report["real"].append("SFX_Object_WoodKnock (G14_wall)")
    climb = next(x for x in film.SHOTS if x.id == "G11_climb")
    hits = [oneshot("SFX_Object_StoneHit%d" % i) for i in (1, 2)]
    if hits[0] is not None:
        for k, at in enumerate((0.25, 0.95, 1.7, 2.6)):
            out.append((bar_t(climb.bar) + at, hits[k % 2], -30.0))
        report["real"].append("SFX_Object_StoneHit (G11_climb)")
    return out


def ride(n):
    """The music's level over the film."""
    g = np.ones(n)
    for a, b, d0, d1 in MUSIC_RIDE:
        i, j = int(bar_t(a) * SR), min(int(bar_t(b) * SR), n)
        if i < n:
            g[i:j] = 10.0 ** (np.linspace(d0, d1, j - i) / 20.0)
    return g


def measure(path):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-af", "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True)
    text = r.stderr
    i = re.findall(r"I:\s+(-?[\d.]+) LUFS", text)
    tp = re.findall(r"Peak:\s+(-?[\d.]+) dBFS", text)
    lra = re.findall(r"LRA:\s+([\d.]+) LU", text)
    return float(i[-1]), float(tp[-1]), float(lra[-1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--music")
    a = ap.parse_args()
    report = {"standin": [], "real": []}
    n = int(TOTAL * SR)
    if a.music:
        x, rate = read_wav(a.music)
        music = resample(x, rate)[:n]
        music = np.vstack((music, np.zeros((max(n - len(music), 0), 2))))
    else:
        music, rate = read_wav(os.path.join(AUD, "stems", "music_mix.wav"))
        music = music[:n]
    music = (music[:n].T * ride(n)[:len(music)]).T * 0.75
    bed_bus = np.zeros((n, 2))
    seen = {}
    for s in film.SHOTS:
        if s.id not in BEDS:
            continue
        name, db = BEDS[s.id]
        t0, t1 = bar_t(s.bar), bar_t(s.bar + s.bars)
        x, real = load_game(name, TOTAL + 2.0) if name not in seen else seen[name]
        seen[name] = (x, real)
        (report["real"] if real else report["standin"]).append("%s (%s)" % (name, s.id))
        i, j = int(t0 * SR), int(t1 * SR)
        part = fade(x[i:j].copy(), 0.04, 0.04)      # the same stretch of a bed stays continuous across shots that share it
        add_at(bed_bus, t0, part, 10.0 ** (db / 20.0))
    sfx = np.zeros((n, 2))
    for t, y, db in cue_list() + footsteps(report) + on_picture(report):
        add_at(sfx, t, y, 10.0 ** (db / 20.0))
    # nothing crosses the hard cut to black at bar 5 but what the score's impact is doing
    cut = int(bar_t(5.0) * SR)
    sfx[cut:cut + int(0.5 * SR)] *= 0.0
    mix = music + bed_bus + sfx
    os.makedirs(AUD, exist_ok=True)
    raw = os.path.join(AUD, "mix_raw.wav")
    pk = np.max(np.abs(mix))
    write_wav(raw, mix * (0.7 / pk))
    # master: a limiter takes the top off, then a static gain to the target: no dynamic loudnorm pumping the quiet opening up
    i0, tp0, lra0 = measure(raw)
    print("raw: %.1f LUFS, %.1f dBTP, LRA %.1f" % (i0, tp0, lra0), flush=True)
    final = os.path.join(AUD, "mix_master.wav")
    limit_in = 7.0
    for attempt in range(6):
        gain = TARGET_LUFS - i0
        chain = "volume=%.2fdB,alimiter=limit=%.4f:attack=2:release=60:level=disabled" % (gain, 10 ** (TRUE_PEAK - 0.6) / 10 ** 0 if False else 10 ** ((TRUE_PEAK - 0.6) / 20.0))
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af", chain, "-c:a", "pcm_s24le", final], check=True)
        i1, tp1, lra1 = measure(final)
        print("pass %d: gain %.2f dB -> %.2f LUFS, %.2f dBTP, LRA %.1f" % (attempt, gain, i1, tp1, lra1), flush=True)
        if abs(i1 - TARGET_LUFS) < 0.25 and tp1 <= TRUE_PEAK + 0.1:
            break
        i0 = i0 + (i1 - TARGET_LUFS)     # the limiter took some: ask for that much more
    print("REPORT game audio:", ", ".join(sorted(set(report["real"]))) or "none yet")
    print("REPORT stand-ins:", ", ".join(sorted(set(report["standin"]))) or "none")
    with open(os.path.join(AUD, "mix_report.json"), "w", encoding="utf-8") as f:
        json.dump(dict(report, lufs=i1, true_peak=tp1, lra=lra1), f, indent=1)


if __name__ == "__main__":
    main()
