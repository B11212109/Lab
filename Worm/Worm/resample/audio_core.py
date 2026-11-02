"""PCM-scale resampling and shared, physically calibrated analysis helpers."""
from pathlib import Path
import hashlib
import numpy as np
import soundfile as sf
from scipy import signal
from scipy.interpolate import CubicSpline
import soxr

FS_IN, FS_OUT = 44100, 16000
METHODS = ('Linear', 'FFT', 'Cubic', 'Polyphase', 'SoXR_HQ')
SETTINGS = {
    'Linear': ('float32', 'float32', 'numpy.interp; native float64 result cast to float32; no anti-alias filter'),
    'FFT': ('float32', 'float32', 'scipy.signal.resample; window=None; periodic extension'),
    'Cubic': ('float32', 'float32', 'CubicSpline; native float64 coefficients/result cast to float32; not-a-knot; no anti-alias filter'),
    'Polyphase': ('float32', 'float32', 'resample_poly; up=160 down=441; window=(kaiser,5.0); padtype=constant'),
    'SoXR_HQ': ('int16', 'int16', 'soxr.resample; quality=HQ; libsoxr internal precision not inferred from interface'),
}
SEGMENTS = [(0., .6, 1000), (.6, 1.2, 6000), (1.2, 1.8, 7400),
            (1.8, 2.4, 10000), (2.4, 3., 15000)]
AMPLITUDE = 18000
IMPULSE_TIME = 4.8
ROOT = Path(__file__).resolve().parent
RESULTS = ROOT / 'results' / 'five_methods'


def pcm16(x):
    x = np.asarray(x)
    if not np.all(np.isfinite(x)):
        raise ValueError('Non-finite samples')
    return np.clip(np.rint(x), -32768, 32767).astype(np.int16)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_input(path):
    info = sf.info(path)
    if (info.samplerate, info.channels, info.subtype) != (FS_IN, 1, 'PCM_16'):
        raise ValueError(f'{path}: expected 44100 Hz mono PCM_16, got {info}')
    x, fs = sf.read(path, dtype='int16')
    if len(x) < FS_IN // 10:
        raise ValueError('Input must be at least 0.1 seconds')
    return x, dict(path=str(path), samplerate=fs, channels=info.channels,
                   subtype=info.subtype, samples=len(x), duration=len(x)/fs,
                   sha256=sha256(path))


def raw_resample(x, method, *, native=False):
    """Return float32 PCM-scale samples (SoXR: int16), without trim/pad.

    native=True exposes library output before the float32 interface cast for auditing.
    NumPy interp and SciPy CubicSpline promote internally to float64. Time coordinates
    stay float64 to avoid degrading physical sample times; analysis also uses float64.
    """
    n = round(len(x) * FS_OUT / FS_IN)
    dtype = SETTINGS[method][0]
    a = x.astype(dtype, copy=False)
    if method == 'Linear':
        y = np.interp(np.arange(n)/FS_OUT, np.arange(len(a))/FS_IN, a)
    elif method == 'Cubic':
        y = CubicSpline(np.arange(len(a))/FS_IN, a)(np.arange(n)/FS_OUT)
    elif method == 'FFT':
        y = signal.resample(a, n)
    elif method == 'Polyphase':
        y = signal.resample_poly(a, 160, 441)
    elif method == 'SoXR_HQ':
        y = soxr.resample(a, FS_IN, FS_OUT, quality='HQ')
    else:
        raise ValueError(method)
    return y if native else y.astype(dtype, copy=False)


def convert(x, method):
    y = pcm16(raw_resample(x, method))
    expected = round(len(x)*FS_OUT/FS_IN)
    if len(y) != expected:
        raise ValueError(f'{method}: native length {len(y)} != rounded target {expected}; '
                         'not trimming or padding. Use an exact-duration input.')
    return y


def tone_amplitude(x, fs, frequency, start, end):
    i = np.arange(int(np.ceil(start*fs)), min(len(x), int(end*fs)))
    phase = 2*np.pi*frequency*i/fs
    # DC and Nyquist have only one identifiable sinusoidal basis component.
    if np.isclose(frequency, 0):
        return abs(float(np.mean(x[i])))
    if np.isclose(frequency, fs/2):
        return abs(float(np.mean(x[i]*np.cos(phase))))
    basis = np.column_stack((np.sin(phase), np.cos(phase), np.ones(len(i))))
    coef = np.linalg.lstsq(basis, x[i].astype(np.float64), rcond=None)[0]
    return float(np.hypot(coef[0], coef[1]))


def folded_frequency(f):
    return abs((f + FS_OUT/2) % FS_OUT - FS_OUT/2)


def measure(x, y, frequency, start, end):
    ai = tone_amplitude(x, FS_IN, frequency, start, end)
    ao = tone_amplitude(y, FS_OUT, folded_frequency(frequency), start, end)
    # A conservative reporting limit of one PCM count, NOT a filter stopband claim.
    floor = 20*np.log10(1/ai)
    return dict(input_amplitude=ai, output_amplitude=ao,
                relative_db=20*np.log10(max(ao, 1)/ai), floor_db=floor,
                below_floor=bool(ao < 1), output_frequency_hz=folded_frequency(frequency))


def psd(x, fs):
    n = min(len(x), round(.04*fs))
    return signal.welch(x.astype(np.float64), fs=fs, window='hann', nperseg=n,
                        noverlap=n//2, detrend=False, scaling='density')


def spectrogram(x, fs):
    n = min(len(x), round(.04*fs))
    hop = max(1, round(.01*fs))
    return signal.spectrogram(x.astype(np.float64), fs=fs, window='hann',
                              nperseg=n, noverlap=max(0, n-hop), detrend=False,
                              scaling='density', mode='psd')


def db_power(p):
    return 10*np.log10(np.maximum(p, 1e-12))
