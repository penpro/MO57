"""MO57 promo audio: the score, written against film.py's bar grid (72 BPM, 4/4; bar b is at b * 10 / 3 seconds). D minor; a falling cell (A G F) and a rising one (A C F); it ends in D major.

    python Tools/promo/audio/score.py [--from BAR --to BAR]   renders Saved/Promo/audio/stems/music_<bus>.wav and music_mix.wav, prints levels per act
    python Tools/promo/audio/score.py --sheet out.png          also a spectrogram of the mix to look at

No samples and no model: every note is synthesised by Tools/promo/audio/instruments.py (Docs/Promo_Video.md section 6 on why). To re-cut, move the bars in film.py and the numbers here.
"""
import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dsp  # noqa: E402
from dsp import SR, add_at, combine, eq_bells, fft_eq, highpass_curve, impulse, lowpass_curve, reverb, rms_db, peak_db, write_wav  # noqa: E402
import instruments as ins  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, "Saved", "Promo", "audio", "stems")
BAR = 10.0 / 3.0
BEAT = BAR / 4.0
TOTAL_BARS = 36.0

CHORD = {
    "Dm": [50, 57, 62, 65, 69],
    "Bb": [46, 53, 58, 62, 65],
    "F": [41, 48, 53, 57, 60, 65],
    "C": [48, 55, 60, 64, 67],
    "Gm": [43, 50, 55, 58, 62],
    "A": [45, 52, 57, 61, 64],
    "D": [50, 57, 62, 66, 69],
    "D5": [50, 57, 62, 69],
}
ROOTNOTE = {"Dm": 38, "Bb": 34, "F": 41, "C": 36, "Gm": 31, "A": 33, "D": 38, "D5": 38}


def T(bar, beat=0.0):
    return bar * BAR + beat * BEAT


class Score:
    def __init__(self, start=0.0, stop=TOTAL_BARS):
        self.n = int((TOTAL_BARS * BAR + 8.0) * SR)
        self.start, self.stop = start, stop
        self.bus = {k: np.zeros((self.n, 2)) for k in ("piano", "strings", "ostinato", "choir", "brass", "drums", "fx", "impacts")}

    def on(self, bar):
        return self.start - 1.0 <= bar <= self.stop

    def put(self, bus, t, y, gain=1.0):
        add_at(self.bus[bus], t, y, gain)


def human(sigma=0.006):
    return dsp.RNG.normal(0.0, sigma)


def play_piano(s, bar, beat, midi, beats, vel=0.5, felt=0.75):
    if s.on(bar):
        s.put("piano", T(bar, beat) + human(), ins.piano(midi, beats * BEAT, vel, felt))


def pad(s, bar, bars, chord, vel=0.5, attack=0.8, bright=0.8, release=1.0, voices=7, octave=0, gain=1.0):
    """Sustained strings on a chord for this many bars."""
    if not s.on(bar):
        return
    for m in CHORD[chord]:
        s.put("strings", T(bar), ins.strings(m + octave, bars * BAR, attack=attack, release=release, voices=voices, vel=vel, brightness=bright), 0.40 * gain)


def bass(s, bar, bars, chord, vel=0.6, attack=0.25):
    if not s.on(bar):
        return
    r = ROOTNOTE[chord]
    s.put("strings", T(bar), ins.strings(r, bars * BAR, attack=attack, release=0.8, voices=5, vel=vel, brightness=0.5, vibrato=3.0), 0.6)


def choir_chord(s, bar, bars, chord, vowel="ah", vel=0.6, attack=1.0, gain=1.0, octave=12):
    if not s.on(bar):
        return
    for m in CHORD[chord][1:]:
        s.put("choir", T(bar), ins.choir(m + octave, bars * BAR, vowel=vowel, attack=attack, vel=vel), 0.42 * gain)


def line(s, bus, notes, fn, gain=1.0):
    """A melody: (bar, beat, midi, beats, vel)."""
    for bar, beat, midi, beats, vel in notes:
        if s.on(bar):
            s.put(bus, T(bar, beat) + human(0.004), fn(midi, beats * BEAT, vel), gain)


_spic = {}


def spic(midi, vel, tone=1.0):
    key = (midi, round(vel * 4), int(dsp.RNG.integers(0, 3)))
    if key not in _spic:
        _spic[key] = ins.spiccato(midi, vel=max(vel, 0.25), tone=tone)
    return _spic[key]


def ostinato(s, bar, bars, pattern, step=0.25, vel=0.6, ramp=0.0, tone=1.0):
    """pattern: per step, a MIDI note or a list, with velocity from an accent grid of 3+3+2 at sixteenths."""
    accents = [1.0, 0.45, 0.6, 1.0, 0.45, 0.6, 1.0, 0.55]
    steps = int(bars * 4 / step)
    for i in range(steps):
        t_bar = bar + i * step / 4.0
        if not s.on(t_bar):
            continue
        p = pattern[i % len(pattern)]
        if p is None:
            continue
        v = vel * accents[i % 8] * (1.0 + ramp * i / steps)
        for m in (p if isinstance(p, (list, tuple)) else [p]):
            s.put("ostinato", T(bar) + i * step * BEAT + human(0.003), spic(m, min(v, 1.0), tone), 0.5)


_drum = {}


def drum(s, bar, beat, kind, vel=1.0, gain=1.0):
    if not s.on(bar):
        return
    key = (kind, round(vel * 5), int(dsp.RNG.integers(0, 4)))
    if key not in _drum:
        _drum[key] = {"low": lambda: ins.taiko(52.0, vel, size=1.25), "mid": lambda: ins.taiko(82.0, vel, size=0.8), "hi": lambda: ins.frame_drum(vel, 180.0)}[kind]()
    s.put("drums", T(bar, beat) + human(0.002), _drum[key], gain)


def drums_full(s, bar, bars, vel=1.0):
    """Half-time weight with a running hand-drum line: the low drum on 1 and 3-and, a mid answer, sixteenths under it."""
    for b in range(int(bars)):
        B = bar + b
        drum(s, B, 0.0, "low", vel, 1.0)
        drum(s, B, 0.75, "mid", 0.45 * vel, 0.5)
        drum(s, B, 1.5, "mid", 0.7 * vel, 0.7)
        drum(s, B, 2.0, "low", 0.8 * vel, 0.8)
        drum(s, B, 2.75, "low", 0.75 * vel, 0.7)
        drum(s, B, 3.5, "mid", 0.85 * vel, 0.8)
        for i in range(16):
            accent = (1.0, 0.35, 0.55, 0.4)[i % 4]
            drum(s, B, i / 4.0, "hi", accent * 0.55 * vel, 0.32)


def compose(s):
    # ---------- The seed (bars 0-5): nothing, then a low mass and glass ----------
    if s.on(0):
        y = ins.drone([26, 33, 38], 5 * BAR + 0.5, cut=240.0, vel=0.65)
        y = dsp.fade(y, 3.0, 0.02)
        s.put("fx", 0.0, y, 0.8)
        s.put("fx", 0.0, dsp.fade(ins.air(5 * BAR + 0.5, 3200.0, 1.5, 0.05), 4.0, 0.02), 1.0)
    for bar, midi, v in ((0.5, 74, 0.5), (1.5, 69, 0.4), (2.5, 77, 0.55), (3.25, 76, 0.45), (3.75, 74, 0.5)):
        if s.on(bar):
            s.put("fx", T(bar), ins.glass(midi, 6.0, v), 1.0)
    if s.on(3):
        low = ins.strings(38, 2 * BAR, attack=2 * BAR * 0.8, release=0.02, voices=7, vel=0.85, brightness=0.7, vibrato=2.0)
        s.put("strings", T(3), dsp.fade(low, 0.0, 0.02), 0.7)
        high = ins.strings(62, 2 * BAR, attack=2 * BAR * 0.9, release=0.02, voices=9, vel=0.7, brightness=1.1, vibrato=4.0)
        s.put("strings", T(3), dsp.fade(high, 0.0, 0.02), 0.34)
        s.put("fx", T(3.0), dsp.fade(ins.riser(2 * BAR, 150.0, 9000.0, 0.7, tonal_midi=50), 0.0, 0.015), 1.0)
    # the hit in the dark: black, then the ground
    if s.on(5):
        s.put("impacts", T(5), ins.impact(size=1.1, vel=1.0, sub_hz=42.0), 1.0)

    # ---------- Wake (bars 5.5-10): the piano alone ----------
    left = [(6, 0, 38, 8, 0.28), (6, 0, 45, 8, 0.24), (7, 0, 34, 8, 0.28), (7, 0, 41, 8, 0.24), (8, 0, 41, 8, 0.28), (8, 0, 48, 8, 0.24), (9, 0, 36, 8, 0.28), (9, 0, 43, 8, 0.24)]
    right = [
        (6, 0, 69, 1.5, 0.42), (6, 1.5, 67, 0.5, 0.34), (6, 2, 65, 2.2, 0.38),
        (7, 0, 65, 1.5, 0.36), (7, 1.5, 62, 0.5, 0.3), (7, 2, 58, 2.2, 0.34),
        (8, 0, 57, 1, 0.34), (8, 1, 60, 1, 0.38), (8, 2, 65, 2.2, 0.44),
        (9, 0, 64, 1, 0.38), (9, 1, 67, 1, 0.42), (9, 2, 67, 2.4, 0.36),
    ]
    for n in left + right:
        play_piano(s, n[0], n[1], n[2], n[3], n[4], felt=0.9)
    # a first touch at the end of the black, one note
    play_piano(s, 5.5, 2, 69, 4, 0.28, felt=0.95)

    # ---------- One seed, a whole world (bars 10-18) ----------
    bass(s, 10, 2, "Dm", 0.45, attack=1.0)
    pad(s, 10, 2, "Dm", vel=0.34, attack=1.4, bright=0.55)
    bass(s, 12, 2, "Bb", 0.5, attack=0.5)
    pad(s, 12, 2, "Bb", vel=0.4, attack=0.7, bright=0.6)
    bass(s, 14, 1, "Gm", 0.58)
    pad(s, 14, 1, "Gm", vel=0.5, bright=0.75)
    bass(s, 15, 1, "A", 0.68)
    pad(s, 15, 1, "A", vel=0.62, bright=0.9)
    D2, A2, D3, F3, A3, Bb2, Bb1, F2, G2, E3 = 38, 45, 50, 53, 57, 46, 34, 41, 43, 52
    ostinato(s, 10, 2, [D2, None, D3, None, A2, None, D3, None], step=0.5, vel=0.42)
    ostinato(s, 12, 2, [Bb1, None, Bb2, None, F2, None, Bb2, None], step=0.5, vel=0.5)
    ostinato(s, 14, 1, [G2, None, D3, G2, None, D3, G2, D3], step=0.5, vel=0.62)
    ostinato(s, 15, 1, [A2, A2, E3, A2, A2, E3, A2, E3], step=0.5, vel=0.74)
    high = [
        (10, 0, 81, 1.5, 0.36), (10, 1.5, 79, 0.5, 0.3), (10, 2, 77, 2, 0.36),
        (11, 0, 74, 1, 0.3), (11, 1, 77, 1, 0.34), (11, 2, 81, 2, 0.4),
        (12, 0, 82, 1.5, 0.38), (12, 1.5, 81, 0.5, 0.32), (12, 2, 77, 2, 0.38),
        (13, 0, 74, 1, 0.34), (13, 1, 77, 1, 0.38), (13, 2, 82, 2, 0.44),
        (14, 0, 79, 2, 0.46), (14, 2, 82, 2, 0.5),
        (15, 0, 85, 2, 0.52), (15, 2, 88, 2, 0.58),
    ]
    for n in high:
        play_piano(s, n[0], n[1], n[2], n[3], n[4], felt=0.55)
    if s.on(14):
        s.put("fx", T(14), dsp.fade(ins.riser(2 * BAR, 240.0, 11000.0, 0.8, tonal_midi=45), 0.0, 0.01), 1.0)
        s.put("fx", T(15), ins.reverse_swell(BAR, 50, 0.9), 0.5)

    # the world breaks open on the islands (bar 16)
    if s.on(16):
        s.put("impacts", T(16), ins.impact(size=1.2, vel=1.0, sub_hz=44.0), 1.0)
        s.put("brass", T(16), ins.brass([29, 41, 48, 53], 2 * BAR * 0.9, attack=0.12, release=1.4, vel=1.0, open_to=3800.0), 0.95)
    bass(s, 16, 1, "F", 0.85, attack=0.05)
    pad(s, 16, 1, "F", vel=0.78, attack=0.12, bright=1.05, gain=1.1)
    bass(s, 17, 1, "C", 0.85, attack=0.1)
    pad(s, 17, 1, "C", vel=0.8, attack=0.12, bright=1.1, gain=1.1)
    choir_chord(s, 16, 1, "F", "ah", 0.75, attack=0.25, gain=1.1)
    choir_chord(s, 17, 1, "C", "ah", 0.8, attack=0.25, gain=1.1)
    theme_hi = [(16, 0, 72, 1.5, 0.85), (16, 1.5, 69, 0.5, 0.75), (16, 2, 77, 2.0, 0.9), (17, 0, 76, 1.0, 0.85), (17, 1, 79, 1.0, 0.9), (17, 2, 79, 2.0, 0.95)]
    line(s, "strings", theme_hi, lambda m, d, v: ins.strings(m + 12, d, attack=0.09, release=0.5, voices=9, detune=9.0, vibrato=14.0, vel=v, brightness=1.15), 0.42)
    ostinato(s, 16, 2, [[D2 + 3, 41], None, [48, 53], 41, None, [48, 53], 41, [48, 53]], step=0.5, vel=0.8)
    drum(s, 16, 0.0, "low", 1.0, 1.1)
    drum(s, 16, 2.0, "low", 0.8, 0.8)
    drum(s, 17, 0.0, "low", 0.9, 0.9)
    drum(s, 17, 2.0, "low", 0.85, 0.85)
    for b, v in ((17.75, 0.6), (17.8125, 0.7), (17.875, 0.8), (17.9375, 0.95)):
        drum(s, 17, (b - 17) * 4, "mid", v, 0.8)

    # ---------- Nothing is given (bars 18-26): the machine starts ----------
    drums_full(s, 18, 7, 0.95)
    sixteen = [D3, D2, D3, D3, D2, D3, D3, D2]
    ostinato(s, 18, 2, sixteen, step=0.25, vel=0.78, tone=0.9)
    ostinato(s, 20, 2, [46, 34, 46, 46, 34, 46, 46, 34], step=0.25, vel=0.82, tone=0.95)
    ostinato(s, 22, 2, [43, 31, 43, 43, 31, 43, 43, 31], step=0.25, vel=0.86, tone=1.0)
    ostinato(s, 24, 1, [45, 33, 45, 45, 33, 45, 45, 33], step=0.25, vel=0.92, tone=1.1)
    for bar, ch, n in ((18, "Dm", 2), (20, "Bb", 2), (22, "Gm", 2), (24, "A", 1)):
        bass(s, bar, n, ch, 0.8, attack=0.08)
        pad(s, bar, n, ch, vel=0.62, attack=0.2, bright=0.95)
    # a high line over the drums: the falling cell, thrown up an octave, then held
    over = [(18, 0, 81, 1.5, 0.7), (18, 1.5, 79, 0.5, 0.62), (18, 2, 77, 2, 0.72), (19, 0, 74, 4, 0.65),
            (20, 0, 77, 1.5, 0.72), (20, 1.5, 74, 0.5, 0.65), (20, 2, 70, 2, 0.72), (21, 0, 74, 4, 0.7),
            (22, 0, 79, 1.5, 0.78), (22, 1.5, 77, 0.5, 0.7), (22, 2, 74, 2, 0.78), (23, 0, 70, 4, 0.74),
            (24, 0, 76, 2, 0.84), (24, 2, 81, 2, 0.9)]
    line(s, "strings", over, lambda m, d, v: ins.strings(m, d, attack=0.1, release=0.5, voices=9, detune=9.0, vibrato=14.0, vel=v, brightness=1.15), 0.4)
    choir_chord(s, 22, 2, "Gm", "oo", 0.5, attack=0.6, gain=0.9)
    choir_chord(s, 24, 1, "A", "ah", 0.7, attack=0.3, gain=1.0)
    # stabs under the words
    for bar, midis, v in ((18, [26, 38, 45, 50], 1.0), (20.5, [22, 34, 41, 46], 1.0), (23, [19, 31, 38, 43], 1.0), (25, [21, 33, 40, 45], 1.0)):
        if s.on(bar):
            s.put("brass", T(bar), ins.brass(midis, 0.75 * BAR, attack=0.04, release=0.8, vel=v, open_to=4200.0), 0.85)
            s.put("impacts", T(bar), ins.impact(size=0.55, vel=0.85, sub_hz=50.0), 0.85)
    # bar 25: the floor goes: only a held line and the words
    if s.on(25):
        s.put("strings", T(25), ins.strings(81, BAR * 0.9, attack=0.3, release=0.7, voices=9, vel=0.55, brightness=1.2, vibrato=6.0), 0.3)
        s.put("strings", T(25), ins.strings(88, BAR * 0.9, attack=0.5, release=0.7, voices=9, vel=0.45, brightness=1.2, vibrato=6.0), 0.2)
        s.put("fx", T(25.25), dsp.fade(ins.riser(0.75 * BAR, 400.0, 6000.0, 0.28), 0.0, 0.2), 1.0)

    # ---------- You will need help (bars 26-31): the theme in full ----------
    prog = [(26, "Bb"), (27, "F"), (28, "C"), (29, "Dm"), (30, "Bb")]
    for i, (bar, ch) in enumerate(prog):
        bass(s, bar, 1, ch, 0.55 + 0.08 * i, attack=0.3)
        pad(s, bar, 1, ch, vel=0.5 + 0.07 * i, attack=0.5, bright=0.8 + 0.06 * i, voices=9)
        choir_chord(s, bar, 1, ch, "oh" if i < 3 else "ah", 0.5 + 0.08 * i, attack=0.5, gain=1.0)
    theme = [
        (26, 0, 65, 1.5, 0.5), (26, 1.5, 62, 0.5, 0.42), (26, 2, 58, 2.2, 0.48),
        (27, 0, 57, 1, 0.46), (27, 1, 60, 1, 0.5), (27, 2, 65, 2.2, 0.56),
        (28, 0, 64, 1, 0.54), (28, 1, 67, 1, 0.58), (28, 2, 72, 2.2, 0.62),
        (29, 0, 69, 1.5, 0.62), (29, 1.5, 67, 0.5, 0.56), (29, 2, 65, 2.2, 0.62),
        (30, 0, 70, 1, 0.66), (30, 1, 74, 1, 0.7), (30, 2, 72, 1, 0.72), (30, 3, 76, 1, 0.78),
    ]
    for n in theme:
        play_piano(s, n[0], n[1], n[2] + 12, n[3], n[4], felt=0.5)
    line(s, "strings", theme, lambda m, d, v: ins.strings(m + 12, d, attack=0.14, release=0.6, voices=9, detune=9.0, vibrato=14.0, vel=v * 1.1, brightness=1.0), 0.34)
    # the drums come back slowly under it
    for bar, v in ((27, 0.5), (28, 0.65), (29, 0.8), (30, 0.95)):
        drum(s, bar, 0.0, "low", v, 0.9)
        drum(s, bar, 2.0, "low", v * 0.8, 0.7)
        drum(s, bar, 3.5, "mid", v * 0.7, 0.6)
    ostinato(s, 29, 2, [D3, None, A2, D3, None, A2, D3, A2], step=0.5, vel=0.62, ramp=0.5)
    # bar 30, beat 3: the last chord turns up, everything climbs
    pad(s, 30.5, 0.5, "C", vel=0.85, attack=0.12, bright=1.2, gain=0.8)
    if s.on(30):
        s.put("fx", T(30), dsp.fade(ins.riser(BAR, 300.0, 12000.0, 1.0, tonal_midi=50), 0.0, 0.008), 1.0)
        s.put("fx", T(30), ins.reverse_swell(BAR, 38, 1.0), 0.55)
    for i, b in enumerate((0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75)):
        drum(s, 30, 2 + b, "mid", 0.5 + 0.06 * i, 0.8)

    # ---------- Title (bars 31-36): the hit, the sun, D major ----------
    if s.on(31):
        s.put("impacts", T(31), ins.impact(size=1.6, vel=1.0, sub_hz=40.0), 1.0)
        s.put("brass", T(31), ins.brass([26, 38, 45, 50, 57], 1.5 * BAR, attack=0.06, release=2.2, vel=1.0, open_to=4600.0), 1.0)
        s.put("drums", T(31), ins.taiko(46.0, 1.0, size=1.6), 1.2)
    bass(s, 31, 2, "D", 0.8, attack=0.05)
    pad(s, 31, 2, "D5", vel=0.72, attack=0.1, bright=1.0, release=3.0, gain=1.0)
    choir_chord(s, 31, 2, "D5", "ah", 0.8, attack=0.15, gain=1.1)
    # the falling cell resolved: A G F-sharp, in D major at last, over the held sound
    last = [(32, 0, 81, 1.5, 0.5), (32, 1.5, 79, 0.5, 0.44), (32, 2, 78, 2.2, 0.5), (33, 0, 74, 6, 0.46), (33, 0, 69, 6, 0.36), (33, 0, 62, 6, 0.36), (33, 0, 50, 8, 0.34), (33, 0, 38, 8, 0.3)]
    for n in last:
        play_piano(s, n[0], n[1], n[2], n[3], n[4], felt=0.7)
    pad(s, 33, 3, "D", vel=0.3, attack=1.2, bright=0.55, release=3.0, gain=0.9)
    choir_chord(s, 33, 3, "D", "oo", 0.32, attack=1.5, gain=0.7)
    if s.on(33):
        s.put("fx", T(33), dsp.fade(ins.air(3 * BAR, 2400.0, 1.6, 0.03), 1.5, 2.0), 1.0)


def duck_curve(n, hits, depth_db=7.0, recover=0.42):
    """A gain over the whole film: dipped at each hit, coming back with the recover time, so the hits have room."""
    g = np.ones(n)
    for bar, depth in hits:
        i = int(T(bar) * SR)
        k = min(int(recover * 4 * SR), n - i)
        if k <= 0:
            continue
        t = np.arange(k) / SR
        dip = 10.0 ** (-depth * depth_db / 20.0 * np.exp(-t / recover))
        g[i:i + k] = np.minimum(g[i:i + k], dip)
    return g


def finish(s):
    os.makedirs(OUT, exist_ok=True)
    print("reverbs", flush=True)
    hall = impulse(3.2, 0.028, seed=11)
    big = impulse(5.2, 0.042, bright=0.42, seed=12)
    room = impulse(1.2, 0.010, bright=0.6, seed=13)
    hits = [(16, 1.0), (18, 0.8), (20.5, 0.8), (23, 0.8), (25, 0.8), (31, 1.2)] + [(b, 0.25) for b in range(18, 25)]
    duck = duck_curve(s.n, hits)
    n = s.n
    shape = {
        "piano": (combine(highpass_curve(55.0), eq_bells((2600.0, 0.9, 1.5), (240.0, 0.7, -1.5))), [(hall, 0.33)], 1.3),
        "strings": (combine(highpass_curve(38.0), eq_bells((250.0, 0.8, -1.5), (3200.0, 1.0, 1.0))), [(hall, 0.30), (big, 0.07)], 0.58),
        "ostinato": (combine(highpass_curve(45.0), eq_bells((190.0, 0.7, 1.0), (800.0, 0.6, -1.5))), [(room, 0.20), (hall, 0.10)], 1.9),
        "choir": (combine(highpass_curve(120.0), eq_bells((2800.0, 0.8, 1.0))), [(big, 0.38)], 1.25),
        "brass": (combine(highpass_curve(28.0), eq_bells((300.0, 0.8, -1.0))), [(big, 0.26)], 0.72),
        "drums": (combine(highpass_curve(30.0), eq_bells((85.0, 0.6, 2.0), (420.0, 0.6, -2.5))), [(room, 0.20), (big, 0.08)], 0.92),
        "fx": (highpass_curve(22.0), [(big, 0.34)], 0.8),
        "impacts": (highpass_curve(24.0), [(big, 0.22)], 0.95),
    }
    mix = np.zeros((n, 2))
    for name, (curve, sends, level) in shape.items():
        t0 = time.time()
        dry = fft_eq(s.bus[name], curve)
        if name in ("strings", "ostinato", "choir", "fx"):
            dry = (dry.T * duck).T
        wet = np.zeros((n + int(8 * SR), 2))
        wet[:n] += dry
        for ir, amount in sends:
            w = dsp.fft_convolve(dry, ir)
            k = min(len(w), len(wet))
            wet[:k] += w[:k] * amount
        wet = wet[:n] * level
        # nothing rings over the hard cut to black at bar 5 but the impact's own bus
        if name != "impacts":
            i = int(T(5) * SR)
            k = int(0.012 * SR)
            ramp = np.ones(n)
            ramp[i:i + k] = np.linspace(1, 0, k)
            hold = int(T(5.5) * SR)
            ramp[i + k:hold] = 0.0
            wet = (wet.T * ramp).T
        write_wav(os.path.join(OUT, "music_%s.wav" % name), wet)
        mix += wet
        print("  %-9s rms %6.1f dB peak %6.1f dB (%.1fs)" % (name, rms_db(wet), peak_db(wet), time.time() - t0), flush=True)
    total = int(TOTAL_BARS * BAR * SR)
    end_fade = int(1.5 * SR)
    mix = mix[:total]
    mix[-end_fade:] = (mix[-end_fade:].T * np.linspace(1, 0, end_fade)).T
    pk = np.max(np.abs(mix))
    mix = mix * (0.89 / pk)
    write_wav(os.path.join(OUT, "music_mix.wav"), mix)
    print("mix peak normalised from %.2f; per section:" % pk)
    for name, a, b in (("seed", 0, 5), ("wake", 5.5, 10), ("world", 10, 16), ("islands", 16, 18), ("nothing", 18, 25), ("help", 26, 31), ("title", 31, 33), ("tail", 33, 36)):
        seg = mix[int(T(a) * SR):int(T(b) * SR)]
        print("  %-8s rms %6.1f dB peak %6.1f dB" % (name, rms_db(seg), peak_db(seg)))
    return mix


def sheet(mix, path):
    """A log-frequency spectrogram, bars marked: a way of looking at what cannot be heard here."""
    from PIL import Image, ImageDraw
    mono = mix.mean(axis=1)
    nfft, hop = 8192, 4096
    win = np.hanning(nfft)
    frames = (len(mono) - nfft) // hop
    W, H = 1800, 500
    bins_f = np.fft.rfftfreq(nfft, 1 / SR)
    y_edges = np.geomspace(25, 18000, H + 1)
    img = np.zeros((H, min(frames, 100000)))
    columns = []
    for i in range(frames):
        s = np.abs(np.fft.rfft(mono[i * hop:i * hop + nfft] * win))
        idx = np.searchsorted(bins_f, y_edges[:-1])
        columns.append(s[np.minimum(idx, len(s) - 1)])
    a = 20 * np.log10(np.array(columns).T + 1e-5)
    a = np.clip((a + 30) / 90, 0, 1)
    pix = (a[::-1] * 255).astype(np.uint8)
    im = Image.fromarray(pix).resize((W, H)).convert("RGB")
    d = ImageDraw.Draw(im)
    for b in range(0, 37):
        x = int(b / 36.0 * W)
        d.line([(x, 0), (x, 8 if b % 5 else 16)], fill=(0, 255, 180))
    im.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="start", type=float, default=0.0)
    ap.add_argument("--to", dest="stop", type=float, default=TOTAL_BARS)
    ap.add_argument("--sheet")
    a = ap.parse_args()
    t0 = time.time()
    s = Score(a.start, a.stop)
    compose(s)
    print("composed in %.1fs" % (time.time() - t0), flush=True)
    mix = finish(s)
    if a.sheet:
        sheet(mix, a.sheet)
    print("done in %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
