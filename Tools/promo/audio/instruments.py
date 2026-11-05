"""MO57 promo audio: the score's instruments, synthesised (no samples, no downloads). Each call returns a stereo buffer starting at the note's onset; the tail is in the buffer.
Everything takes MIDI numbers and seconds. Tools/promo/audio/score.py plays them.
"""
import numpy as np

from dsp import (RNG, SR, adsr, combine, drift, eq_bells, fade, highpass_curve, lowpass_curve, midi_hz, pan, soft_clip, spectral, stereo, supersaw)


def strings(midi, dur, attack=0.35, release=0.6, voices=7, detune=11.0, vibrato=9.0, brightness=1.0, vel=0.8):
    """A section of bowed strings: detuned saws through a body resonance, the bow's noise under them, brighter the harder it is played."""
    total = dur + release
    n = int(total * SR)
    y = supersaw(midi, total, voices=voices, detune_cents=detune, vibrato_cents=vibrato, width=0.85)
    f0 = float(midi_hz(midi))
    t = np.arange(n) / SR
    env = adsr(n, attack, 0.0, 1.0, release)
    breath = RNG.standard_normal(n) * 0.018 * (0.5 + vel)
    y = y + np.stack((breath, np.roll(breath, 311)), axis=1)
    cut0 = max(f0 * (3.0 + 5.0 * vel) * brightness, 500.0)
    cut1 = max(f0 * (7.0 + 13.0 * vel) * brightness, 1200.0)

    def gain(f, times):
        opening = np.clip(times / max(attack * 1.6, 0.05), 0.0, 1.0)[:, None]
        fc = cut0 + (cut1 - cut0) * opening
        lp = 1.0 / np.sqrt(1.0 + (f[None, :] / fc) ** 4.0)
        body = eq_bells((270.0, 0.55, 3.0), (610.0, 0.5, 2.0), (1300.0, 0.6, 2.5), (3100.0, 0.6, 2.0), (5500.0, 0.8, -3.0))(f)
        hp = highpass_curve(max(f0 * 0.6, 42.0), 2.0)(f)
        return lp * (body * hp)[None, :]

    y = spectral(y, gain, nfft=4096, hop=1024)
    return (y.T * env).T * vel


def spiccato(midi, length=0.13, vel=0.8, tone=1.0):
    """One short bow stroke of a low-string section: the attack is most of it."""
    total = length + 0.09
    n = int(total * SR)
    y = supersaw(midi, total, voices=5, detune_cents=9.0, width=0.5)
    f0 = float(midi_hz(midi))
    t = np.arange(n) / SR
    env = np.exp(-t / (length * 0.55)) * (1 - np.exp(-t / 0.004))
    chiff = RNG.standard_normal(n) * np.exp(-t / 0.012) * 0.25 * vel
    y = (y.T * env).T + np.stack((chiff, chiff), axis=1)
    fc = f0 * (5.0 + 10.0 * vel) * tone
    y = spectral(y, lambda f, times: (1.0 / np.sqrt(1.0 + (f / fc) ** 4.0)) * eq_bells((240.0, 0.5, 3.5), (1100.0, 0.7, 2.0))(f) * highpass_curve(max(f0 * 0.55, 35.0), 2.0)(f), nfft=1024, hop=256)
    return y * vel


def choir(midi, dur, vowel="ah", attack=0.8, release=1.2, voices=9, vel=0.7):
    """Massed voices: many unsteady saws through a vowel's formants, some breath on top."""
    total = dur + release
    n = int(total * SR)
    y = supersaw(midi, total, voices=voices, detune_cents=9.0, vibrato_cents=16.0, vibrato_hz=4.8, width=1.0)
    forms = {"ah": (760.0, 1180.0, 2650.0), "oh": (480.0, 860.0, 2500.0), "oo": (330.0, 800.0, 2300.0), "eh": (560.0, 1800.0, 2600.0)}[vowel]
    bells = eq_bells((forms[0], 0.32, 15.0), (forms[1], 0.32, 12.0), (forms[2], 0.28, 9.0), (3400.0, 0.3, 4.0))
    f0 = float(midi_hz(midi))
    env = adsr(n, attack, 0.0, 1.0, release)
    breath = RNG.standard_normal(n) * 0.05
    y = y + np.stack((breath, np.roll(breath, 977)), axis=1)
    y = spectral(y, lambda f, times: bells(f) * lowpass_curve(4600.0, 2.0)(f) * highpass_curve(max(f0 * 0.75, 90.0), 2.0)(f) * 0.08, nfft=4096, hop=1024)
    return (y.T * env).T * vel


def brass(midis, dur, attack=0.18, release=0.9, vel=1.0, open_to=3200.0):
    """The braam: low brass and detuned saws together, the filter opening like a section leaning in, a little grit."""
    total = dur + release
    n = int(total * SR)
    out = np.zeros((n, 2))
    for m in midis:
        out += supersaw(m, total, voices=7, detune_cents=16.0, width=0.75, scoop_cents=45.0, scoop_s=0.07, pulse=0.35)
    out /= np.sqrt(len(midis))
    env = adsr(n, attack, dur * 0.8, 0.55, release)
    closed = 180.0 + 160.0 * vel

    def gain(f, times):
        op = np.clip(times / (attack * 2.2), 0.0, 1.0)[:, None] ** 1.4
        decay = np.clip((times - attack * 2.2) / (dur * 0.9), 0.0, 1.0)[:, None]
        fc = closed + (open_to - closed) * op * (1.0 - 0.72 * decay)
        lp = 1.0 / np.sqrt(1.0 + (f[None, :] / fc) ** 3.0)
        return lp * eq_bells((130.0, 0.6, 4.0), (520.0, 0.7, 2.5), (1500.0, 0.8, 3.0))(f)[None, :] * highpass_curve(30.0, 2.0)(f)[None, :]

    out = spectral(out, gain, nfft=4096, hop=512)
    out = soft_clip((out.T * env).T * 0.42, 1.4)
    return out * vel


def piano(midi, dur, vel=0.5, felt=0.75):
    """A felted upright: inharmonic partials, two strings to a note beating slowly, highs dying first, a soft thump of the hammer. Keeps ringing until it is damped at dur."""
    f0 = float(midi_hz(midi))
    ring = 2.0 + 5.0 * np.clip((96.0 - midi) / 60.0, 0.1, 1.0)
    total = min(dur + 0.45, ring * 1.3) + 0.2
    n = int(total * SR)
    t = np.arange(n) / SR
    B = 0.00032 * (1.0 + max(midi - 60, 0) / 24.0)
    y = np.zeros(n)
    y2 = np.zeros(n)
    tilt = (1200.0 + 3000.0 * vel) * (1.25 - felt * 0.7)
    count = min(int(11000.0 / f0), 34)
    for k in range(1, count + 1):
        fk = k * f0 * np.sqrt(1.0 + B * k * k)
        if fk > 17000.0:
            break
        amp = (1.0 / k ** (1.05 + (1.0 - vel) * 0.5)) * np.exp(-fk / tilt) * (1.0 + 0.5 * np.sin(k * 1.9))   # a hammer's strike point hollows some partials
        t60 = ring / (1.0 + 0.16 * (k - 1) + fk / 5000.0)
        decay = np.exp(-6.91 * t / t60) * 0.86 + 0.14 * np.exp(-6.91 * t / (t60 * 2.8))
        beat = RNG.uniform(0.1, 0.45)
        p1, p2 = RNG.uniform(0, 6.28), RNG.uniform(0, 6.28)
        y += amp * decay * np.sin(2 * np.pi * (fk + beat) * t + p1)
        y2 += amp * decay * np.sin(2 * np.pi * (fk - beat) * t + p2)
    y = y * 0.62 + y2 * 0.38
    y2 = y2 * 0.62 + y * 0.38
    a = int(0.0065 * (1.0 + felt) * SR)
    env = np.ones(n)
    env[:a] = np.sin(0.5 * np.pi * np.arange(a) / a)
    rel = max(int(dur * SR), a + 1)
    if rel < n:
        env[rel:] = np.exp(-(t[rel:] - t[rel]) / 0.14)
    thump = RNG.standard_normal(n) * np.exp(-t / 0.009) * 0.09
    wood = np.sin(2 * np.pi * 95.0 * t) * np.exp(-t / 0.035) * 0.07
    left = (y * env + thump + wood)
    right = (y2 * env + thump + wood)
    place = np.clip((midi - 60.0) / 40.0, -0.6, 0.6)
    a0 = (place + 1) * np.pi / 4
    out = np.stack((left * np.cos(a0), right * np.sin(a0)), axis=1)
    return out * (0.20 + 0.65 * vel)


def glass(midi, dur=5.0, vel=0.5):
    """A struck, glassy tone for the void: few inharmonic partials, a very long ring."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    f0 = float(midi_hz(midi))
    y = np.zeros(n)
    for ratio, amp, life in ((1.0, 1.0, 1.0), (2.76, 0.42, 0.55), (5.4, 0.2, 0.3), (8.93, 0.08, 0.16)):
        y += amp * np.exp(-6.91 * t / (dur * life)) * np.sin(2 * np.pi * f0 * ratio * t + RNG.uniform(0, 6.28))
    y *= (1 - np.exp(-t / 0.002))
    return pan(y, RNG.uniform(-0.35, 0.35)) * vel * 0.4


def taiko(pitch=62.0, vel=1.0, room=0.0, size=1.0):
    """A big drum: the skin's pitch falling fast, a hit of stick noise, the shell's low ring."""
    total = 1.4 * size
    n = int(total * SR)
    t = np.arange(n) / SR
    f = pitch * (1.0 + 1.1 * np.exp(-t / 0.026))
    phase = 2 * np.pi * np.cumsum(f) / SR
    tone = np.sin(phase) * np.exp(-t / (0.26 * size)) + 0.3 * np.sin(phase * 1.59) * np.exp(-t / 0.09)
    stick = RNG.standard_normal(n) * np.exp(-t / 0.011)
    stick = spectral(stick[:, None].repeat(2, axis=1), lambda f, times: eq_bells((380.0, 0.9, 6.0), (1400.0, 0.8, 0.0))(f) * lowpass_curve(3800.0, 2.0)(f), nfft=512, hop=128)[:, 0]
    y = tone * 0.95 + stick * 0.22 * vel
    return pan(soft_clip(y * (0.6 + 0.6 * vel), 1.2), RNG.uniform(-0.12, 0.12)) * vel


def frame_drum(vel=0.7, pitch=170.0):
    """A smaller hand drum for the sixteenth notes."""
    n = int(0.42 * SR)
    t = np.arange(n) / SR
    f = pitch * (1.0 + 0.35 * np.exp(-t / 0.018))
    tone = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.075)
    slap = RNG.standard_normal(n) * np.exp(-t / 0.008)
    slap = spectral(slap[:, None].repeat(2, axis=1), lambda f, times: lowpass_curve(5200.0, 2.0)(f) * highpass_curve(420.0, 2.0)(f), nfft=512, hop=128)[:, 0]
    return pan(tone * 0.6 + slap * 0.45, RNG.uniform(-0.45, 0.45)) * vel


def impact(size=1.0, vel=1.0, sub_hz=46.0):
    """A hit you feel: a sub dropping, a big drum, a crack of noise, a dark metal ring under it."""
    total = 5.0 * size
    n = int(total * SR)
    t = np.arange(n) / SR
    f = sub_hz * (0.55 + 0.45 * np.exp(-t / 1.1)) * (1.0 + 1.2 * np.exp(-t / 0.03))
    sub = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (1.3 * size))
    crack = RNG.standard_normal(n) * np.exp(-t / 0.035)
    crack = spectral(crack[:, None].repeat(2, axis=1), lambda f, times: lowpass_curve(5200.0, 1.5)(f) * highpass_curve(260.0, 1.5)(f), nfft=1024, hop=256)
    metal = np.zeros(n)
    for ratio, amp, life in ((1.0, 1.0, 1.1), (1.47, 0.7, 0.9), (2.09, 0.55, 0.7), (2.83, 0.38, 0.5), (3.67, 0.3, 0.4), (5.11, 0.2, 0.25)):
        metal += amp * np.exp(-t / (life * size)) * np.sin(2 * np.pi * 68.0 * ratio * t + RNG.uniform(0, 6.28))
    out = stereo(sub * 1.0) + crack * 0.34 + pan(metal * 0.13, 0.0)
    return soft_clip(out * 0.9, 1.25) * vel


def riser(dur, start_hz=200.0, end_hz=7000.0, vel=0.5, tonal_midi=None):
    """Air and pitch climbing to a point: noise through a rising band, and if asked a rising tone inside it."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    noise = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1) * 0.25

    def gain(f, times):
        p = np.clip(times / dur, 0.0, 1.0)[:, None]
        centre = start_hz * (end_hz / start_hz) ** (p ** 1.6)
        return np.exp(-0.5 * (np.log2(np.maximum(f[None, :], 1.0) / centre) / 1.15) ** 2)

    y = spectral(noise, gain, nfft=2048, hop=512)
    env = (t / dur) ** 2.4
    out = (y.T * env).T
    if tonal_midi is not None:
        f = midi_hz(tonal_midi) * 2.0 ** ((t / dur) ** 2.0 * 1.0)
        tone = np.sin(2 * np.pi * np.cumsum(f) / SR) + 0.5 * np.sin(4 * np.pi * np.cumsum(f) / SR)
        out = out + pan(tone * env * 0.07, 0.0)
    return out * vel


def reverse_swell(dur, midi=50, vel=0.5):
    """A note played backwards into a hit: the ring of a low string swelling up to nothing."""
    y = strings(midi, dur, attack=0.02, release=0.1, vel=0.8)
    n = len(y)
    t = np.arange(n) / SR
    y = (y.T * np.exp(-6.0 * t / dur)).T[::-1]
    return y * vel


def drone(midis, dur, cut=320.0, vel=0.4, beat_depth=0.2):
    """A low held mass: a few saws almost closed, moving slowly."""
    n = int(dur * SR)
    out = np.zeros((n, 2))
    for m in midis:
        out += supersaw(m, dur, voices=5, detune_cents=7.0, width=0.8)
    out /= np.sqrt(len(midis))

    def gain(f, times):
        wob = (1.0 + 0.35 * np.sin(times * 2 * np.pi * 0.07))[:, None]
        return (1.0 / np.sqrt(1.0 + (f[None, :] / (cut * wob)) ** 4.0)) * highpass_curve(24.0, 2.0)(f)[None, :]

    out = spectral(out, gain, nfft=8192, hop=2048)
    lfo = 1.0 + beat_depth * (drift(n, 0.25) * 0.5)
    return (out.T * lfo).T * vel


def air(dur, centre=2600.0, width=1.6, vel=0.12, rate=0.1):
    """Slow breath of filtered noise: the sound of nothing, moving."""
    n = int(dur * SR)
    noise = np.stack((RNG.standard_normal(n), RNG.standard_normal(n)), axis=1)

    def gain(f, times):
        c = centre * 2.0 ** (0.8 * np.interp(times, np.linspace(times[0], times[-1], max(int(dur * rate) + 3, 4)), RNG.uniform(-1, 1, max(int(dur * rate) + 3, 4))))[:, None]
        return np.exp(-0.5 * (np.log2(np.maximum(f[None, :], 1.0) / c) / width) ** 2)

    return spectral(noise, gain, nfft=4096, hop=1024) * vel
