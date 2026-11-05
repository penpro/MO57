"""MO57 promo audio: the DSP the score and the sound design are built from. numpy only (no scipy here; Blender's numpy works too). 48 kHz float stereo: arrays of shape (n, 2).

Everything is an offline, whole-buffer operation: spectral (STFT) filters instead of recursive ones, convolution reverb from a synthesised impulse response.
"""
import wave

import numpy as np

SR = 48000
RNG = np.random.default_rng(57)


def midi_hz(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=np.float64) - 69.0) / 12.0)


def stereo(x):
    return np.stack((x, x), axis=1) if x.ndim == 1 else x


def pan(x, pos):
    """Equal-power pan of a mono buffer, -1 left .. 1 right."""
    a = (pos + 1.0) * np.pi / 4.0
    return np.stack((x * np.cos(a), x * np.sin(a)), axis=1)


def fade(x, in_s=0.0, out_s=0.0):
    x = x.copy()
    n = len(x)
    if in_s > 0:
        k = min(int(in_s * SR), n)
        g = 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / max(k, 1))
        x[:k] = (x[:k].T * g).T
    if out_s > 0:
        k = min(int(out_s * SR), n)
        g = 0.5 + 0.5 * np.cos(np.pi * np.arange(k) / max(k, 1))
        x[n - k:] = (x[n - k:].T * g).T
    return x


def adsr(n, a, d, s, r, curve=1.0):
    """An envelope over n samples: attack, decay to sustain, release at the end. Seconds; sustain 0..1."""
    na, nd, nr = int(a * SR), int(d * SR), int(r * SR)
    env = np.full(n, float(s))
    na = min(na, n)
    if na:
        env[:na] = (0.5 - 0.5 * np.cos(np.pi * np.arange(na) / na)) ** curve
    nd = min(nd, n - na)
    if nd > 0:
        env[na:na + nd] = 1.0 + (s - 1.0) * (np.arange(nd) / nd)
    nr = min(nr, n)
    if nr:
        env[n - nr:] *= (0.5 + 0.5 * np.cos(np.pi * np.arange(nr) / nr))
    return env


def saw(freq, n, phase=0.0):
    """Band-limited sawtooth (PolyBLEP). freq: Hz, scalar or per-sample."""
    dt = np.broadcast_to(np.asarray(freq, dtype=np.float64) / SR, (n,))
    ph = (phase + np.cumsum(dt)) % 1.0
    y = 2.0 * ph - 1.0
    m = ph < dt
    t = ph[m] / dt[m]
    y[m] -= t + t - t * t - 1.0
    m = ph > 1.0 - dt
    t = (ph[m] - 1.0) / dt[m]
    y[m] -= t * t + t + t + 1.0
    return y


def drift(n, rate_hz, seed_rng=RNG):
    """A slow random curve about -1..1 (interpolated random points)."""
    pts = max(int(n / SR * rate_hz) + 3, 4)
    p = seed_rng.uniform(-1.0, 1.0, pts)
    x = np.linspace(0, pts - 1.001, n)
    i = x.astype(np.int64)
    f = x - i
    f = f * f * (3 - 2 * f)
    return p[i] * (1 - f) + p[i + 1] * f


def supersaw(midi, dur, voices=7, detune_cents=14.0, vibrato_cents=0.0, vibrato_hz=5.2, width=0.9, scoop_cents=0.0, scoop_s=0.08, pulse=0.0):
    """A section: several voices of a saw, each its own detune, drift and side of the stage. Stereo."""
    n = int(dur * SR)
    f0 = midi_hz(midi)
    t = np.arange(n) / SR
    out = np.zeros((n, 2))
    for v in range(voices):
        spread = (v - (voices - 1) / 2) / max((voices - 1) / 2, 1)
        cents = spread * detune_cents + RNG.uniform(-2.5, 2.5)
        cents_t = cents + drift(n, 0.7) * 3.0
        if vibrato_cents:
            onset = np.clip(t / 0.7, 0, 1)
            cents_t = cents_t + vibrato_cents * onset * np.sin(2 * np.pi * (vibrato_hz + RNG.uniform(-0.5, 0.5)) * t + RNG.uniform(0, 6.28))
        if scoop_cents:
            cents_t = cents_t - scoop_cents * np.exp(-t / scoop_s)
        f = f0 * 2.0 ** (cents_t / 1200.0)
        y = saw(f, n, RNG.uniform())
        if pulse:
            y = y + pulse * (saw(f, n, 0.5) - y) * 0.0 + pulse * np.sign(np.sin(2 * np.pi * np.cumsum(f) / SR + RNG.uniform(0, 6.28))) * 0.4
        pos = spread * width
        a = (pos + 1.0) * np.pi / 4.0
        out[:, 0] += y * np.cos(a)
        out[:, 1] += y * np.sin(a)
    return out / np.sqrt(voices)


# ---------- spectral filters ----------

def _stft_frames(n, nfft, hop):
    return max(int(np.ceil((n + nfft) / hop)), 1)


def spectral(x, gain, nfft=4096, hop=1024):
    """Filter by a magnitude curve: gain(freqs_hz, frame_times_s) -> array (frames, bins), or (bins,) for a static curve. Zero phase, stereo-aware."""
    x = stereo(x)
    n = len(x)
    freqs = np.fft.rfftfreq(nfft, 1.0 / SR)
    win = np.hanning(nfft + 1)[:-1]
    frames = _stft_frames(n, nfft, hop)
    times = (np.arange(frames) * hop - nfft / 2) / SR
    g = gain(freqs, times)
    if g.ndim == 1:
        g = np.broadcast_to(g, (frames, len(freqs)))
    pad = np.zeros((nfft + frames * hop, 2))
    pad[nfft // 2: nfft // 2 + n] = x
    out = np.zeros_like(pad)
    norm = np.zeros(len(pad))
    for ch in range(2):
        idx = np.arange(frames)[:, None] * hop + np.arange(nfft)[None, :]
        chunks = []
        for start in range(0, frames, 256):
            sel = idx[start:start + 256]
            spec = np.fft.rfft(pad[sel, ch] * win, axis=1) * g[start:start + 256]
            y = np.fft.irfft(spec, n=nfft, axis=1) * win
            np.add.at(out[:, ch], sel.ravel(), y.ravel())
    idx = np.arange(frames)[:, None] * hop + np.arange(nfft)[None, :]
    np.add.at(norm, idx.ravel(), np.tile(win * win, frames))
    norm[norm < 1e-6] = 1.0
    res = out / norm[:, None]
    return res[nfft // 2: nfft // 2 + n]


def fft_eq(x, curve):
    """A static EQ over a whole buffer: curve(freqs) -> gain, zero phase. Fast for long stems."""
    x = stereo(x)
    n = len(x)
    size = n + 8192
    f = np.fft.rfftfreq(size, 1.0 / SR)
    g = curve(f)
    out = np.zeros_like(x)
    for ch in range(2):
        s = np.fft.rfft(x[:, ch], n=size) * g
        out[:, ch] = np.fft.irfft(s, n=size)[:n]
    return out


def lowpass_curve(fc, order=2.0):
    return lambda f: 1.0 / np.sqrt(1.0 + (f / max(fc, 1.0)) ** (2.0 * order))


def highpass_curve(fc, order=2.0):
    return lambda f: 1.0 / np.sqrt(1.0 + (max(fc, 1.0) / np.maximum(f, 1.0)) ** (2.0 * order))


def peak(f, centre, width_oct, gain_db):
    """One bell in dB, a Gaussian in octaves."""
    return gain_db * np.exp(-0.5 * (np.log2(np.maximum(f, 1.0) / centre) / width_oct) ** 2)


def eq_bells(*bells):
    """curve(f) from (centre, width in octaves, dB) bells."""
    def curve(f):
        db = np.zeros_like(f)
        for c, w, g in bells:
            db = db + peak(f, c, w, g)
        return 10.0 ** (db / 20.0)
    return curve


def combine(*curves):
    def curve(f):
        g = np.ones_like(f)
        for c in curves:
            g = g * c(f)
        return g
    return curve


def fft_convolve(x, ir):
    x = stereo(x)
    ir = stereo(ir)
    size = len(x) + len(ir)
    out = np.zeros((size, 2))
    for ch in range(2):
        out[:, ch] = np.fft.irfft(np.fft.rfft(x[:, ch], n=size) * np.fft.rfft(ir[:, ch], n=size), n=size)
    return out


def impulse(seconds=3.2, predelay=0.024, early=14, bright=0.55, seed=1):
    """A hall: sparse early reflections, a diffuse stereo tail whose highs die first. Unit energy; low end thinned so a reverb never muddies the bass."""
    rng = np.random.default_rng(seed)
    n = int(seconds * 1.4 * SR)
    t = np.arange(n) / SR
    size = n
    f = np.fft.rfftfreq(size, 1.0 / SR)
    bands = [(0, 180, 0.75), (180, 700, 1.0), (700, 2500, 0.8), (2500, 7000, 0.5 * (0.5 + bright)), (7000, 24000, 0.28 * (0.3 + bright))]
    ir = np.zeros((n, 2))
    for ch in range(2):
        noise = np.fft.rfft(rng.standard_normal(n))
        tail = np.zeros(n)
        for lo, hi, scale in bands:
            band = np.fft.irfft(noise * ((f >= lo) & (f < hi)), n=size)
            tail += band * np.exp(-6.91 * t / max(seconds * scale, 0.05))
        env = 1.0 - np.exp(-t / 0.035)
        ir[:, ch] = tail * env
    head = int(predelay * SR)
    ir = np.vstack((np.zeros((head, 2)), ir))
    for _ in range(early):
        at = int(rng.uniform(0.006, 0.075) * SR) + head // 2
        ir[at, rng.integers(0, 2)] += rng.uniform(-1, 1) * 3.0 / (1 + at / 2000)
    ir = fft_eq(ir, highpass_curve(150.0, 1.0))
    ir /= np.sqrt(np.sum(ir ** 2, axis=0, keepdims=True) + 1e-9)
    return ir


def reverb(x, ir, wet=0.3, dry=1.0):
    """Convolve and blend; the result is as long as x plus the tail."""
    w = fft_convolve(x, ir)
    out = w * wet
    out[:len(x)] += stereo(x) * dry
    return out


def soft_clip(x, drive=1.0):
    return np.tanh(x * drive) / np.tanh(drive)


def add_at(buf, t, y, gain=1.0):
    """Mix y into buf at t seconds (cut to fit)."""
    y = stereo(y)
    i = int(round(t * SR))
    if i < 0:
        y = y[-i:]
        i = 0
    if i >= len(buf):
        return
    k = min(len(y), len(buf) - i)
    buf[i:i + k] += y[:k] * gain


def rms_db(x):
    return 20.0 * np.log10(np.sqrt(np.mean(np.asarray(x) ** 2)) + 1e-12)


def peak_db(x):
    return 20.0 * np.log10(np.max(np.abs(x)) + 1e-12)


def write_wav(path, x, bits=24):
    x = np.clip(stereo(x), -1.0, 1.0)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setframerate(SR)
        if bits == 24:
            w.setsampwidth(3)
            v = (x * 8388607.0).astype(np.int32)
            b = v.astype("<i4").tobytes()
            w.writeframes(bytes(np.frombuffer(b, dtype=np.uint8).reshape(-1, 4)[:, :3].ravel()))
        else:
            w.setsampwidth(2)
            w.writeframes((x * 32767.0).astype("<i2").tobytes())


def read_wav(path):
    """Any 16 / 24 bit PCM WAV -> float stereo at this file's rate; returns (x, rate). Resample with resample()."""
    with wave.open(path, "rb") as w:
        n, ch, width, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    if width == 3:
        a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        v = (a[:, 0].astype(np.int32) | (a[:, 1].astype(np.int32) << 8) | (a[:, 2].astype(np.int32) << 16))
        v = (v << 8) >> 8
        x = v / 8388608.0
    elif width == 2:
        x = np.frombuffer(raw, dtype="<i2") / 32768.0
    else:
        raise ValueError("unsupported width %d" % width)
    x = x.reshape(-1, ch)
    return (x if ch == 2 else np.repeat(x[:, :1], 2, axis=1)), rate


def resample(x, rate, target=SR):
    if rate == target:
        return x
    n = int(len(x) * target / rate)
    pos = np.linspace(0, len(x) - 1.0001, n)
    i = pos.astype(np.int64)
    f = (pos - i)[:, None]
    return x[i] * (1 - f) + x[i + 1] * f
