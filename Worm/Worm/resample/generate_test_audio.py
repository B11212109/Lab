"""Create the documented five-second signal; never overwrite an existing input."""
import numpy as np
import soundfile as sf
from scipy.signal import chirp
from audio_core import ROOT, FS_IN, SEGMENTS, AMPLITUDE, IMPULSE_TIME, pcm16


def fade(x, fs=FS_IN):
    n = min(round(.01*fs), len(x)//2)
    ramp = np.sin(np.linspace(0, np.pi/2, n))**2
    x[:n] *= ramp
    x[-n:] *= ramp[::-1]
    return x


def generate(path=ROOT / 'synthetic_test.wav'):
    if path.exists():
        return path
    x = np.zeros(5*FS_IN, dtype=np.float64)
    for start, end, f in SEGMENTS:
        a, b = round(start*FS_IN), round(end*FS_IN)
        x[a:b] = fade(AMPLITUDE*np.sin(2*np.pi*f*np.arange(b-a)/FS_IN))
    a, b = round(3*FS_IN), round(4.6*FS_IN)
    x[a:b] = fade(AMPLITUDE*chirp(np.arange(b-a)/FS_IN, 200, 1.6, 20000, method='linear'))
    x[round(IMPULSE_TIME*FS_IN)] = 24000
    sf.write(path, pcm16(x), FS_IN, subtype='PCM_16')
    return path


if __name__ == '__main__':
    print(generate())
