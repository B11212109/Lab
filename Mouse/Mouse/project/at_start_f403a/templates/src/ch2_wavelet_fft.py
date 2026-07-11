#!/usr/bin/env python3
"""
CH2 wavelet + FFT analyzer.

Input CSV format:
  Time_Rel,CH1_Raw,CH2_Raw

This script reads CH2 only, estimates the sampling rate from Time_Rel,
then exports FFT and Morlet-CWT energy results.
"""

import argparse
import os
import sys
from datetime import datetime
import tkinter as tk
from tkinter import filedialog

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams.update({
    "font.size": 13,
    "axes.titlesize": 16,
    "axes.labelsize": 14,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "legend.fontsize": 12,
    "figure.titlesize": 17,
})


DEFAULT_MODE = "both"
HEART_FMIN_HZ = 3.0
HEART_FMAX_HZ = 15.0
RESP_FMIN_HZ = 0.3
RESP_FMAX_HZ = 3.0
DEFAULT_FREQ_BINS = 80
DEFAULT_MORLET_CYCLES = 6.0
DEFAULT_TOP_N = 8
DEFAULT_START_SEC = 30.0
DEFAULT_END_SEC = 60.0
DEFAULT_HEART_START_SEC = None
DEFAULT_HEART_END_SEC = None
DEFAULT_RESP_START_SEC = None
DEFAULT_RESP_END_SEC = None
DEFAULT_RESP_LOW_HZ = RESP_FMIN_HZ
DEFAULT_RESP_HIGH_HZ = RESP_FMAX_HZ
DEFAULT_RESP_FREQ_BINS = 48
FFT_MIN_DISPLAY_HZ = 0.2
FFT_Y_MAX_PERCENTILE = 99.5
SEGMENT_PLOT_SECONDS = 5.0


def select_csv_file():
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    csv_path = filedialog.askopenfilename(
        title="Select CSV file",
        filetypes=[("CSV Files", "*.csv"), ("All Files", "*.*")]
    )
    root.destroy()
    return csv_path


def format_segment_name(start_s=None, end_s=None):
    if start_s is None and end_s is None:
        return "full"

    start_text = "start" if start_s is None else f"{start_s:.1f}"
    end_text = "end" if end_s is None else f"{end_s:.1f}"
    return f"segment_{start_text}-{end_text}s"


def ensure_combined_output_dir(csv_path, output_root, start_s=None, end_s=None):
    base_name = os.path.splitext(os.path.basename(csv_path))[0]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    segment_name = format_segment_name(start_s, end_s)
    output_path = os.path.join(output_root, f"{base_name}_CH2_both_{segment_name}_{timestamp}")
    os.makedirs(output_path, exist_ok=True)
    return output_path


def ensure_output_dir(csv_path, output_root, mode, start_s=None, end_s=None, combined_output_dir=None):
    if combined_output_dir:
        output_path = os.path.join(combined_output_dir, mode)
        os.makedirs(output_path, exist_ok=True)
        return output_path

    base_name = os.path.splitext(os.path.basename(csv_path))[0]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    segment_name = format_segment_name(start_s, end_s)
    output_path = os.path.join(output_root, f"{base_name}_CH2_{mode}_{segment_name}_{timestamp}")
    os.makedirs(output_path, exist_ok=True)
    return output_path


def load_ch2_csv(csv_path):
    with open(csv_path, "r", encoding="utf-8-sig") as f:
        header = f.readline().strip().split(",")

    data = np.genfromtxt(csv_path, delimiter=",", skip_header=1)
    if data.ndim == 1:
        data = np.expand_dims(data, axis=0)

    if data.shape[1] < 3:
        raise ValueError("CSV must contain Time_Rel, CH1_Raw, and CH2_Raw")

    header_map = {name.strip(): idx for idx, name in enumerate(header)}
    time_idx = header_map.get("Time_Rel", 0)
    ch2_idx = header_map.get("CH2_Raw", 2)

    valid_mask = np.isfinite(data[:, time_idx]) & np.isfinite(data[:, ch2_idx])
    data = data[valid_mask]
    if len(data) < 8:
        raise ValueError("Not enough valid CH2 samples")

    t = data[:, time_idx].astype(float)
    ch2 = data[:, ch2_idx].astype(float)

    order = np.argsort(t)
    t = t[order]
    ch2 = ch2[order]

    unique_mask = np.concatenate(([True], np.diff(t) > 0))
    t = t[unique_mask]
    ch2 = ch2[unique_mask]
    if len(t) < 8:
        raise ValueError("Not enough unique time samples")

    return t, ch2


def estimate_sampling_rate(t):
    dt = np.diff(t)
    dt = dt[np.isfinite(dt) & (dt > 0)]
    if len(dt) == 0:
        raise ValueError("Invalid Time_Rel column")
    return 1.0 / np.median(dt)


def extract_time_range(t, signal, start_s=None, end_s=None):
    if start_s is None and end_s is None:
        return t, signal

    if start_s is None:
        start_s = float(t[0])
    if end_s is None:
        end_s = float(t[-1])
    if end_s <= start_s:
        raise ValueError("--end must be greater than --start")

    mask = (t >= start_s) & (t <= end_s)
    if not np.any(mask):
        raise ValueError(f"No CH2 data in selected range: {start_s:.3f}~{end_s:.3f}s")

    return t[mask], signal[mask]


def detrend_dc(signal):
    signal = np.asarray(signal, dtype=float)
    return signal - np.mean(signal)


def compute_fft(signal, fs):
    signal = detrend_dc(signal)
    n = len(signal)
    window = np.hanning(n)
    spectrum = np.fft.rfft(signal * window)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    amps = np.abs(spectrum) * 2.0 / np.sum(window)
    power = amps * amps
    return freqs, amps, power


def build_wavelet_frequency_grid(fmin, fmax, freq_bins, resp_low, resp_high, resp_bins):
    base_freqs = np.linspace(fmin, fmax, freq_bins)
    resp_high = min(resp_high, fmax)
    if resp_high > resp_low:
        resp_freqs = np.linspace(resp_low, resp_high, resp_bins)
        return np.unique(np.concatenate((base_freqs, resp_freqs)))
    return base_freqs


def morlet_cwt_power(signal, fs, freqs, cycles):
    signal = detrend_dc(signal)
    dt = 1.0 / fs
    power = np.zeros((len(freqs), len(signal)), dtype=float)

    for i, freq in enumerate(freqs):
        sigma_t = cycles / (2.0 * np.pi * freq)
        half_width = max(3.5 * sigma_t, 2.0 / fs)
        tw = np.arange(-half_width, half_width + dt, dt)
        wavelet = np.exp(2j * np.pi * freq * tw) * np.exp(-(tw * tw) / (2.0 * sigma_t * sigma_t))
        norm = np.sqrt(np.sum(np.abs(wavelet) ** 2))
        if norm > 0:
            wavelet = wavelet / norm

        conv = np.convolve(signal, np.conj(wavelet[::-1]), mode="same")
        if len(conv) != len(signal):
            start = (len(conv) - len(signal)) // 2
            conv = conv[start:start + len(signal)]
        power[i, :] = np.abs(conv) ** 2

    return power


def find_top_frequency_peaks(freqs, values, top_n, min_hz=0.0, max_hz=None):
    freqs = np.asarray(freqs)
    values = np.asarray(values)
    mask = np.isfinite(freqs) & np.isfinite(values) & (freqs >= min_hz)
    if max_hz is not None:
        mask &= freqs <= max_hz
    freqs = freqs[mask]
    values = values[mask]

    if len(freqs) == 0:
        return []

    peak_indices = []
    for i in range(1, len(values) - 1):
        if values[i] >= values[i - 1] and values[i] >= values[i + 1]:
            peak_indices.append(i)

    if not peak_indices:
        peak_indices = list(range(len(values)))

    peak_indices = sorted(peak_indices, key=lambda idx: values[idx], reverse=True)
    result = []
    for idx in peak_indices[:top_n]:
        result.append((float(freqs[idx]), float(values[idx])))
    return result


def find_band_energy_peaks(freqs, values, low_hz, high_hz, top_n):
    freqs = np.asarray(freqs)
    values = np.asarray(values)
    mask = np.isfinite(freqs) & np.isfinite(values) & (freqs >= low_hz) & (freqs <= high_hz)
    return find_top_frequency_peaks(freqs[mask], values[mask], top_n, min_hz=low_hz, max_hz=high_hz)


def save_fft_csv(freqs, amps, power, output_path, mode):
    file_path = os.path.join(output_path, f"CH2_fft_{mode}.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("Frequency_Hz,Amplitude,Power\n")
        for freq, amp, pwr in zip(freqs, amps, power):
            f.write(f"{freq:.6f},{amp:.8f},{pwr:.8f}\n")
    return file_path


def save_wavelet_csv(freqs, mean_power, output_path):
    file_path = os.path.join(output_path, "CH2_wavelet_mean_power.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("Frequency_Hz,Mean_Wavelet_Power\n")
        for freq, pwr in zip(freqs, mean_power):
            f.write(f"{freq:.6f},{pwr:.8f}\n")
    return file_path


def save_wavelet_time_frequency_csv(t, freqs, power, output_path):
    file_path = os.path.join(output_path, "CH2_wavelet_time_frequency_power.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        time_headers = ",".join(f"{ti:.6f}" for ti in t)
        f.write(f"Frequency_Hz,{time_headers}\n")
        for freq, row in zip(freqs, power):
            values = ",".join(f"{pwr:.8f}" for pwr in row)
            f.write(f"{freq:.6f},{values}\n")
    return file_path


def save_respiration_peaks(output_path, fft_resp_peaks, wavelet_resp_peaks, low_hz, high_hz):
    csv_path = os.path.join(output_path, "CH2_respiration_peaks.csv")
    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("Method,Rank,Frequency_Hz,Energy,Band_Low_Hz,Band_High_Hz\n")
        for rank, (freq, value) in enumerate(fft_resp_peaks, 1):
            f.write(f"FFT_Respiration,{rank},{freq:.6f},{value:.8f},{low_hz:.6f},{high_hz:.6f}\n")
        for rank, (freq, value) in enumerate(wavelet_resp_peaks, 1):
            f.write(f"Wavelet_Respiration,{rank},{freq:.6f},{value:.8f},{low_hz:.6f},{high_hz:.6f}\n")
    return csv_path


def save_summary(output_path, fs, duration, mode, fft_peaks, wavelet_peaks, fft_resp_peaks, wavelet_resp_peaks, resp_low, resp_high):
    txt_path = os.path.join(output_path, "CH2_energy_summary.txt")
    csv_path = os.path.join(output_path, "CH2_energy_peaks.csv")

    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("Method,Rank,Frequency_Hz,Energy\n")
        for rank, (freq, value) in enumerate(fft_peaks, 1):
            f.write(f"FFT,{rank},{freq:.6f},{value:.8f}\n")
        for rank, (freq, value) in enumerate(wavelet_peaks, 1):
            f.write(f"Wavelet,{rank},{freq:.6f},{value:.8f}\n")

    with open(txt_path, "w", encoding="utf-8") as f:
        f.write("CH2 energy concentration summary\n")
        f.write(f"Mode: {mode}\n")
        f.write(f"Estimated sampling rate: {fs:.3f} Hz\n")
        f.write(f"Duration: {duration:.3f} s\n\n")
        f.write("Top FFT energy peaks:\n")
        for rank, (freq, value) in enumerate(fft_peaks, 1):
            f.write(f"  {rank}. {freq:.3f} Hz, power={value:.8f}\n")
        f.write("\nTop wavelet mean-energy peaks:\n")
        for rank, (freq, value) in enumerate(wavelet_peaks, 1):
            f.write(f"  {rank}. {freq:.3f} Hz, mean_power={value:.8f}\n")
        if mode == "respiration":
            f.write(f"\nRespiration-band energy peaks ({resp_low:.3f}~{resp_high:.3f} Hz):\n")
            f.write("  FFT:\n")
            for rank, (freq, value) in enumerate(fft_resp_peaks, 1):
                f.write(f"    {rank}. {freq:.3f} Hz, power={value:.8f}\n")
            f.write("  Wavelet:\n")
            for rank, (freq, value) in enumerate(wavelet_resp_peaks, 1):
                f.write(f"    {rank}. {freq:.3f} Hz, mean_power={value:.8f}\n")
            if duration < (3.0 / max(resp_low, 1e-6)):
                f.write("\nNote: selected duration is short for the lowest respiration frequency. Use a longer window for reliable respiration analysis.\n")

    return txt_path, csv_path


def plot_fft(freqs, amps, output_path, fs, low_hz, high_hz, mode, peaks=None):
    nyquist = fs * 0.5
    x_min = max(low_hz, 0.0)
    x_max = min(high_hz, nyquist)
    mask = (freqs >= x_min) & (freqs <= x_max)
    y_max = np.percentile(amps[mask], FFT_Y_MAX_PERCENTILE) * 1.2 if np.any(mask) else np.max(amps)
    y_max = max(float(y_max), 1.0)

    fig, ax = plt.subplots(figsize=(13, 5))
    ax.plot(freqs, amps, "b-", lw=1.0)
    ax.set_title(f"CH2 FFT ({x_min:.1f}-{x_max:.1f} Hz)", fontweight="bold")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Amplitude")
    ax.set_xlim(x_min, x_max)
    ax.set_ylim(0, y_max)
    ax.grid(True, alpha=0.3)

    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_fft_{mode}.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_raw_waveform(t, signal, output_path):
    fig, ax = plt.subplots(figsize=(14, 5))
    wave_color = "#0072B2"
    ax.plot(t, signal, color=wave_color, lw=1.0)
    ax.fill_between(t, signal, np.min(signal), color=wave_color, alpha=0.12)
    ax.set_title("CH2 Raw Waveform", fontweight="bold")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("CH2 Raw Value")
    ax.set_xlim(float(t[0]), float(t[-1]))

    sig_min = float(np.min(signal))
    sig_max = float(np.max(signal))
    span = sig_max - sig_min
    if span <= 0:
        span = 1.0
    pad = span * 0.15
    ax.set_ylim(sig_min - pad, sig_max + pad)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()

    file_path = os.path.join(output_path, "CH2_raw_waveform.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_wavelet(t, freqs, power, output_path, low_hz, high_hz):
    fig, ax = plt.subplots(figsize=(14, 7))
    freq_mask = (freqs >= low_hz) & (freqs <= high_hz)
    if np.any(freq_mask):
        freqs_plot = freqs[freq_mask]
        power_plot = power[freq_mask, :]
    else:
        freqs_plot = freqs
        power_plot = power
    extent = [float(t[0]), float(t[-1]), float(freqs_plot[0]), float(freqs_plot[-1])]

    vmax = np.percentile(power_plot[np.isfinite(power_plot)], 99.0) if np.any(np.isfinite(power_plot)) else 1.0
    image = ax.imshow(
        power_plot,
        aspect="auto",
        origin="lower",
        extent=extent,
        cmap="magma",
        vmin=0,
        vmax=max(vmax, 1e-12)
    )
    ax.set_title(f"CH2 Morlet CWT Time-Frequency Energy ({extent[2]:.1f}-{extent[3]:.1f} Hz)", fontweight="bold")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Frequency (Hz)")
    ax.grid(False)
    fig.colorbar(image, ax=ax, label="Wavelet Power")

    fig.tight_layout()
    file_path = os.path.join(output_path, "CH2_wavelet_energy.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_5s_wavelet_segments(t, signal, freqs, power, output_path, mode, low_hz, high_hz):
    segment_dir = os.path.join(output_path, "segments_5s")
    os.makedirs(segment_dir, exist_ok=True)

    freq_mask = (freqs >= low_hz) & (freqs <= high_hz)
    if np.any(freq_mask):
        freqs_plot = freqs[freq_mask]
        power_plot = power[freq_mask, :]
    else:
        freqs_plot = freqs
        power_plot = power

    finite_power = power_plot[np.isfinite(power_plot)]
    vmax = np.percentile(finite_power, 99.0) if len(finite_power) else 1.0
    vmax = max(float(vmax), 1e-12)

    saved_files = []
    start_s = float(t[0])
    end_s = float(t[-1])
    segment_start = start_s

    while segment_start < end_s:
        segment_end = min(segment_start + SEGMENT_PLOT_SECONDS, end_s)
        mask = (t >= segment_start) & (t < segment_end)
        if segment_end >= end_s:
            mask = (t >= segment_start) & (t <= segment_end)

        if np.count_nonzero(mask) >= 2:
            t_seg = t[mask]
            signal_seg = signal[mask]
            power_seg = power_plot[:, mask]
            extent = [float(t_seg[0]), float(t_seg[-1]), float(freqs_plot[0]), float(freqs_plot[-1])]

            fig, axes = plt.subplots(2, 1, figsize=(13, 8), sharex=True)
            fig.suptitle(f"CH2 {mode} wavelet segment {segment_start:.1f}-{segment_end:.1f}s", fontweight="bold")

            axes[0].plot(t_seg, signal_seg, color="#0072B2", lw=1.0)
            axes[0].set_title("Raw CH2")
            axes[0].set_xlabel("Time (s)")
            axes[0].set_ylabel("Raw Value")
            axes[0].tick_params(labelbottom=True)
            axes[0].grid(True, alpha=0.3)

            image = axes[1].imshow(
                power_seg,
                aspect="auto",
                origin="lower",
                extent=extent,
                cmap="magma",
                vmin=0,
                vmax=vmax,
            )
            axes[1].set_title(f"Morlet CWT Energy ({extent[2]:.1f}-{extent[3]:.1f} Hz)")
            axes[1].set_xlabel("Time (s)")
            axes[1].set_ylabel("Frequency (Hz)")
            axes[1].grid(False)
            axes[1].set_xlim(float(t_seg[0]), float(t_seg[-1]))
            fig.colorbar(image, ax=axes[1], label="Wavelet Power")

            fig.tight_layout()
            start_text = f"{segment_start:06.1f}".replace(".", "p")
            end_text = f"{segment_end:06.1f}".replace(".", "p")
            file_name = f"CH2_{mode}_wavelet_{start_text}-{end_text}s.png"
            file_path = os.path.join(segment_dir, file_name)
            fig.savefig(file_path, dpi=150)
            plt.close(fig)
            saved_files.append(file_path)

        segment_start += SEGMENT_PLOT_SECONDS

    return saved_files


def apply_mode_defaults(args):
    if args.mode == "heart":
        if args.fmin is None:
            args.fmin = HEART_FMIN_HZ
        if args.fmax is None:
            args.fmax = HEART_FMAX_HZ
        if args.resp_low is None:
            args.resp_low = args.fmin
        if args.resp_high is None:
            args.resp_high = args.fmax
    elif args.mode == "respiration":
        if args.fmin is None:
            args.fmin = RESP_FMIN_HZ
        if args.fmax is None:
            args.fmax = RESP_FMAX_HZ
        if args.resp_low is None:
            args.resp_low = RESP_FMIN_HZ
        if args.resp_high is None:
            args.resp_high = RESP_FMAX_HZ

    return args


def parse_args():
    parser = argparse.ArgumentParser(description="CH2 wavelet + FFT analyzer for load-cell CSV")
    parser.add_argument("csv_file", nargs="?", default=None, help="Input CSV path")
    parser.add_argument("--mode", choices=("heart", "respiration", "both"), default=DEFAULT_MODE, help="Analysis mode")
    parser.add_argument("--start", type=float, default=DEFAULT_START_SEC, help="Analysis start time in seconds")
    parser.add_argument("--end", type=float, default=DEFAULT_END_SEC, help="Analysis end time in seconds")
    parser.add_argument("--heart-start", type=float, default=DEFAULT_HEART_START_SEC, help="Heart-mode start time in seconds")
    parser.add_argument("--heart-end", type=float, default=DEFAULT_HEART_END_SEC, help="Heart-mode end time in seconds")
    parser.add_argument("--resp-start", type=float, default=DEFAULT_RESP_START_SEC, help="Respiration-mode start time in seconds")
    parser.add_argument("--resp-end", type=float, default=DEFAULT_RESP_END_SEC, help="Respiration-mode end time in seconds")
    parser.add_argument("--fmin", type=float, default=None, help="Override minimum analysis frequency (Hz)")
    parser.add_argument("--fmax", type=float, default=None, help="Override maximum analysis frequency (Hz)")
    parser.add_argument("--freq-bins", type=int, default=DEFAULT_FREQ_BINS, help="Number of wavelet frequency bins")
    parser.add_argument("--cycles", type=float, default=DEFAULT_MORLET_CYCLES, help="Morlet cycles")
    parser.add_argument("--top", type=int, default=DEFAULT_TOP_N, help="Number of energy peaks to report")
    parser.add_argument("--resp-low", type=float, default=None, help="Respiration band low cutoff (Hz)")
    parser.add_argument("--resp-high", type=float, default=None, help="Respiration band high cutoff (Hz)")
    parser.add_argument("--resp-bins", type=int, default=DEFAULT_RESP_FREQ_BINS, help="Extra wavelet bins inside respiration band")
    parser.add_argument("--output", default="output", help="Output directory root")
    return parser.parse_args()


def run_analysis(args):
    if args.fmin <= 0 or args.fmax <= args.fmin:
        print("[ERROR] Frequency range must satisfy 0 < fmin < fmax")
        sys.exit(1)
    if args.resp_low <= 0 or args.resp_high <= args.resp_low:
        print("[ERROR] Respiration band must satisfy 0 < resp-low < resp-high")
        sys.exit(1)

    try:
        t, ch2 = load_ch2_csv(args.csv_file)
        t, ch2 = extract_time_range(t, ch2, args.start, args.end)
        fs = estimate_sampling_rate(t)
        nyquist = fs * 0.5
        fmax = min(args.fmax, nyquist * 0.95)
        if fmax <= args.fmin:
            raise ValueError(f"Frequency range too high for Fs={fs:.2f} Hz")

        wavelet_freqs = build_wavelet_frequency_grid(
            args.fmin,
            fmax,
            args.freq_bins,
            args.resp_low,
            args.resp_high,
            args.resp_bins
        )
        fft_freqs, fft_amps, fft_power = compute_fft(ch2, fs)
        wavelet_power = morlet_cwt_power(ch2, fs, wavelet_freqs, args.cycles)
        wavelet_mean_power = np.mean(wavelet_power, axis=1)
    except Exception as e:
        print(f"[ERROR] {e}")
        sys.exit(1)

    output_path = ensure_output_dir(
        args.csv_file,
        args.output,
        args.mode,
        args.start,
        args.end,
        getattr(args, "combined_output_dir", None)
    )
    duration = float(t[-1] - t[0])

    fft_peaks = find_top_frequency_peaks(fft_freqs, fft_power, args.top, min_hz=args.fmin, max_hz=args.fmax)
    wavelet_peaks = find_top_frequency_peaks(wavelet_freqs, wavelet_mean_power, args.top, min_hz=args.fmin, max_hz=args.fmax)
    fft_resp_peaks = []
    wavelet_resp_peaks = []
    if args.mode == "respiration":
        fft_resp_peaks = find_band_energy_peaks(fft_freqs, fft_power, args.resp_low, args.resp_high, args.top)
        wavelet_resp_peaks = find_band_energy_peaks(wavelet_freqs, wavelet_mean_power, args.resp_low, args.resp_high, args.top)

    save_fft_csv(fft_freqs, fft_amps, fft_power, output_path, args.mode)
    save_wavelet_csv(wavelet_freqs, wavelet_mean_power, output_path)
    wavelet_tf_csv = save_wavelet_time_frequency_csv(t, wavelet_freqs, wavelet_power, output_path)
    raw_plot = plot_raw_waveform(t, ch2, output_path)
    fft_plot = plot_fft(fft_freqs, fft_amps, output_path, fs, args.fmin, args.fmax, args.mode, fft_peaks)
    resp_csv = None
    if args.mode == "respiration":
        resp_csv = save_respiration_peaks(output_path, fft_resp_peaks, wavelet_resp_peaks, args.resp_low, args.resp_high)
    wavelet_plot = plot_wavelet(t, wavelet_freqs, wavelet_power, output_path, args.fmin, args.fmax)
    segment_plots = plot_5s_wavelet_segments(t, ch2, wavelet_freqs, wavelet_power, output_path, args.mode, args.fmin, args.fmax)
    summary_txt, summary_csv = save_summary(
        output_path,
        fs,
        duration,
        args.mode,
        fft_peaks,
        wavelet_peaks,
        fft_resp_peaks,
        wavelet_resp_peaks,
        args.resp_low,
        args.resp_high
    )

    print(f"[INFO] Input: {args.csv_file}")
    print(f"[INFO] Mode: {args.mode}, analysis band: {args.fmin:.3f}~{args.fmax:.3f} Hz")
    if args.start is not None or args.end is not None:
        print(f"[INFO] Selected range: {t[0]:.3f}~{t[-1]:.3f}s")
    print(f"[INFO] Samples: {len(t)}, duration: {duration:.3f}s, Fs~{fs:.2f}Hz")
    print(f"[INFO] Output directory: {output_path}")
    if fft_peaks:
        print(f"[INFO] Strongest FFT peak: {fft_peaks[0][0]:.3f} Hz")
    if wavelet_peaks:
        print(f"[INFO] Strongest wavelet energy: {wavelet_peaks[0][0]:.3f} Hz")
    if args.mode == "respiration" and fft_resp_peaks:
        print(f"[INFO] Strongest FFT respiration-band peak: {fft_resp_peaks[0][0]:.3f} Hz")
    if args.mode == "respiration" and wavelet_resp_peaks:
        print(f"[INFO] Strongest wavelet respiration-band energy: {wavelet_resp_peaks[0][0]:.3f} Hz")
    if args.mode == "respiration" and duration < (3.0 / max(args.resp_low, 1e-6)):
        print("[WARN] Selected duration is short for the lowest respiration frequency; use a longer window for reliable respiration analysis.")
    print(f"[INFO] Saved: {raw_plot}")
    print(f"[INFO] Saved: {fft_plot}")
    print(f"[INFO] Saved: {wavelet_plot}")
    print(f"[INFO] Saved 5-second segment plots: {len(segment_plots)} files in {os.path.join(output_path, 'segments_5s')}")
    print(f"[INFO] Saved: {wavelet_tf_csv}")
    if resp_csv:
        print(f"[INFO] Saved: {resp_csv}")
    print(f"[INFO] Saved: {summary_txt}")
    print(f"[INFO] Saved: {summary_csv}")


def build_mode_args(base_args, mode):
    mode_args = argparse.Namespace(**vars(base_args))
    mode_args.mode = mode
    mode_args.fmin = base_args.fmin
    mode_args.fmax = base_args.fmax
    mode_args.resp_low = base_args.resp_low
    mode_args.resp_high = base_args.resp_high

    if mode == "heart":
        if base_args.heart_start is not None:
            mode_args.start = base_args.heart_start
        if base_args.heart_end is not None:
            mode_args.end = base_args.heart_end
    elif mode == "respiration":
        if base_args.resp_start is not None:
            mode_args.start = base_args.resp_start
        if base_args.resp_end is not None:
            mode_args.end = base_args.resp_end

    return apply_mode_defaults(mode_args)


def main():
    args = parse_args()
    if not args.csv_file:
        args.csv_file = select_csv_file()
    if not args.csv_file:
        print("[ERROR] No CSV file selected.")
        sys.exit(1)

    args.combined_output_dir = None
    if args.mode == "both":
        args.combined_output_dir = ensure_combined_output_dir(args.csv_file, args.output, args.start, args.end)
        print(f"[INFO] Combined output directory: {args.combined_output_dir}")

    modes = ("heart", "respiration") if args.mode == "both" else (args.mode,)
    for mode in modes:
        print(f"\n[INFO] ===== Running {mode} mode =====")
        run_analysis(build_mode_args(args, mode))


if __name__ == "__main__":
    main()
