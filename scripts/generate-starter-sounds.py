"""Rebuild Gate's original, synthesized starter effects (requires NumPy).

No recordings or third-party samples are used. Generated WAVs ship in the
repository; Python/NumPy are not needed to build or run Gate. MIT license.
"""
from pathlib import Path
import wave
import numpy as np

RATE = 48000
RNG = np.random.default_rng(7102026)
OUTPUT = Path(__file__).resolve().parents[1] / "resources" / "sounds"


def time(seconds):
    return np.arange(round(seconds * RATE)) / RATE


def envelope(t, duration, attack=.006, release=.035):
    return np.minimum(t / attack, 1) * np.minimum(np.maximum((duration - t) / release, 0), 1)


def noise(t, rng=None):
    return (RNG if rng is None else rng).normal(0, 1, len(t))


def band_noise(t, low, high, rng=None):
    frequencies = np.fft.rfftfreq(len(t), 1 / RATE)
    spectrum = np.fft.rfft(noise(t, rng))
    # Smooth band edges avoid ringing in the percussive sources.
    shape = (1 - np.exp(-(frequencies / low) ** 4)) * np.exp(-(frequencies / high) ** 4)
    result = np.fft.irfft(spectrum * shape, n=len(t))
    return result / max(np.std(result), 1e-6)


def place(track, sound, start, gain=1):
    offset = round(start * RATE)
    length = min(len(sound), len(track) - offset)
    if length > 0:
        track[offset:offset + length] += sound[:length] * gain


def room(sound, taps=((.029, .13), (.047, .08), (.081, .04))):
    result = sound.copy()
    for delay, gain in taps:
        offset = round(delay * RATE)
        result[offset:] += sound[:-offset] * gain
    return result


def save(name, sound):
    sound -= np.mean(sound)
    peak = np.max(np.abs(sound))
    active = sound[np.abs(sound) > peak * .025]
    rms = np.sqrt(np.mean(active ** 2))
    # Match active loudness across the pack, with soft limiting and headroom.
    sound *= .18 / max(rms, 1e-6)
    sound = .78 * np.tanh(sound / .78)
    fade = min(480, len(sound) // 2)
    sound[:fade] *= np.linspace(0, 1, fade)
    sound[-fade:] *= np.linspace(1, 0, fade)
    pcm = np.round(sound * 32767).astype("<i2")
    with wave.open(str(OUTPUT / name), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(pcm.tobytes())
    print(f"{name}: {len(sound) / RATE:.2f}s, peak {np.max(np.abs(sound)):.3f}")


def horn():
    result = np.zeros(round(1.75 * RATE))
    for start, duration in ((0, .16), (.24, .16), (.48, 1.08)):
        t = time(duration)
        sound = np.zeros(len(t))
        for fundamental in (392, 493.88):
            frequency = fundamental * (1 + .014 * np.exp(-t * 22) + .0015 * np.sin(2 * np.pi * 6 * t))
            phase = 2 * np.pi * np.cumsum(frequency) / RATE
            for harmonic in range(1, 11):
                sound += np.sin(harmonic * phase + .04 * harmonic) / harmonic ** 1.05
        sound = np.tanh(sound * .7) + band_noise(t, 1000, 5500) * .014
        place(result, sound * envelope(t, duration, .009, .06), start)
    return room(result)


def snare(duration=.16):
    t = time(duration)
    tone = np.sin(2 * np.pi * (185 * t + 2 * (1 - np.exp(-t * 35))))
    snap = band_noise(t, 1100, 9500)
    return (tone * np.exp(-t / .043) * .35 + snap * np.exp(-t / .033) * .48) * np.minimum(t / .001, 1)


def drum_roll():
    result = np.zeros(round(2.05 * RATE))
    start, hit = .02, 0
    while start < 1.7:
        place(result, snare(), start, (.3 + .7 * start / 1.7) * (1 if hit % 2 else .78))
        start += .065 - .036 * min(start / 1.7, 1)
        hit += 1
    place(result, snare(.24), 1.76, 1.45)
    return room(result)


def rimshot():
    result = np.zeros(round(1.9 * RATE))
    for start, pitch in ((0, 145), (.19, 112)):
        t = time(.22)
        phase = 2 * np.pi * (pitch * t + 1.5 * (1 - np.exp(-t * 35)))
        sound = (.7 * np.sin(phase) + .25 * np.sin(phase * 1.58)) * np.exp(-t / .063)
        place(result, sound, start)
    place(result, snare(.22), .43, .65)
    t = time(1.35)
    cymbal = band_noise(t, 4500, 15000)
    for frequency in (3721, 5413, 7129, 8971):
        cymbal += .12 * np.sin(2 * np.pi * frequency * t)
    place(result, cymbal * np.exp(-t / .25) * np.minimum(t / .004, 1), .43, .55)
    return room(result)


def buzzer():
    result = np.zeros(round(.9 * RATE))
    for start in (0, .43):
        t = time(.34)
        tone = sum(np.sin(2 * np.pi * 155 * h * t) / h for h in (1, 3, 5, 7, 9, 11))
        tone += .25 * np.sin(2 * np.pi * 164 * t)
        place(result, tone * envelope(t, .34) * (.9 + .1 * np.sin(2 * np.pi * 28 * t)), start)
    return result


def chime():
    result = np.zeros(round(2.5 * RATE))
    for start, frequency in ((0, 880), (.21, 1318.51)):
        t = time(2.1)
        sound = np.zeros(len(t))
        for ratio, gain, decay in ((1, 1, .65), (2.76, .36, .33), (5.4, .13, .16), (8.93, .05, .07)):
            sound += gain * np.sin(2 * np.pi * frequency * ratio * t) * np.exp(-t / decay)
        place(result, sound * np.minimum(t / .003, 1), start)
    return room(result)


def sad_trombone():
    result = np.zeros(round(2.65 * RATE))
    for index, frequency in enumerate((233.08, 220, 207.65, 196)):
        duration = .44 if index < 3 else .96
        t = time(duration)
        glide = np.maximum(t - .28, 0) * (62 if index == 3 else 2)
        frequencies = frequency - glide + 1.4 * np.sin(2 * np.pi * 5.4 * t) * np.minimum(t * 5, 1)
        phase = 2 * np.pi * np.cumsum(frequencies) / RATE
        tone = np.zeros(len(t))
        for harmonic in range(1, 10):
            tone += np.sin(harmonic * phase) * np.exp(-harmonic / 5) / harmonic ** .82
        wah = .65 + .35 * np.sin(np.pi * np.minimum(t / .35, 1))
        place(result, tone * wah * envelope(t, duration, .03, .085), index * .49)
    return room(result)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", help="Regenerate only this WAV filename")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for filename, create in (("air-horn.wav", horn),
                             ("drum-roll.wav", drum_roll), ("rimshot.wav", rimshot),
                             ("buzzer.wav", buzzer), ("chime.wav", chime),
                             ("sad-trombone.wav", sad_trombone)):
        if args.only is None or args.only == filename:
            save(filename, create())
