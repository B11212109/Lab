#!/usr/bin/env python3
"""
CH2 IIR filter comparison.

Default input CSV format:
  Time_Rel,CH1_Raw,CH2_Raw

This script reads CH2, estimates Fs from Time_Rel, and exports:
  - Butterworth order 8 bandpass result
  - Chebyshev Type I order 8 bandpass result, rp = 0.5 dB

Bands:
  - heart: 4.0 ~ 12.0 Hz
  - respiration: 0.5 ~ 3.0 Hz
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

try:
    from scipy.signal import butter, cheby1, sosfiltfilt, sosfreqz
except ModuleNotFoundError as exc:
    if exc.name != "scipy":
        raise

    venv_python = r"D:\Lab\Mouse\Mouse\.venv\Scripts\python.exe"
    print("[ERROR] scipy is not installed in the Python environment currently running this script.")
    print("[HINT] This project venv already has scipy. Please run:")
    print(f"       {venv_python} {os.path.abspath(__file__)}")
    print("[HINT] Or install scipy into your current Python:")
    print("       python -m pip install scipy")
    sys.exit(1)


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUTPUT_ROOT = os.path.join(SCRIPT_DIR, "output")
DEFAULT_START_SEC = 30.0
DEFAULT_END_SEC = 60.0

FILTER_ORDER = 8
CHEBY_RP_DB = 0.5
FFT_Y_MAX_PERCENTILE = 99.5
SEGMENT_PLOT_SECONDS = 5.0

BANDS = {
    "heart": {
        "low": 4.0,
        "high": 12.0,
        "color": "#0000FF",
    },
    "respiration": {
        "low": 0.5,
        "high": 3.0,
        "color": "#008000",
    },
}

FILTERS = {
    "butterworth8": {
        "label": "Butterworth 8",
        "csv_name": "Butterworth8",
        "color": "#009E73",
    },
    "chebyshev1_8_rp0p5db": {
        "label": "Chebyshev Type I 8, rp=0.5 dB",
        "csv_name": "ChebyshevI8_rp0p5dB",
        "color": "#CC79A7",
    },
}


def select_csv_file():
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    csv_path = filedialog.askopenfilename(
        title="Select load-cell CSV file",
        filetypes=[("CSV Files", "*.csv"), ("All Files", "*.*")]
    )
    root.destroy()
    return csv_path


def format_segment_name(start_s=None, end_s=None):
    start_text = "start" if start_s is None else f"{start_s:.1f}"
    end_text = "end" if end_s is None else f"{end_s:.1f}"
    return f"segment_{start_text}-{end_text}s"


def ensure_output_dir(csv_path, output_root, start_s, end_s):
    base_name = os.path.splitext(os.path.basename(csv_path))[0]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    segment_name = format_segment_name(start_s, end_s)
    output_path = os.path.join(output_root, f"{base_name}_CH2_iir_filters_{segment_name}_{timestamp}")
    os.makedirs(output_path, exist_ok=True)
    return output_path


def load_csv(csv_path):
    with open(csv_path, "r", encoding="utf-8-sig") as f:
        header = f.readline().strip().split(",")

    data = np.genfromtxt(csv_path, delimiter=",", skip_header=1)
    if data.ndim == 1:
        data = np.expand_dims(data, axis=0)

    if data.shape[1] < 3:
        raise ValueError("CSV columns insufficient. Expected Time_Rel, CH1_Raw, CH2_Raw.")

    header_map = {name.strip(): idx for idx, name in enumerate(header)}
    time_idx = header_map.get("Time_Rel", 0)
    ch2_idx = header_map.get("CH2_Raw", 2)

    valid_mask = np.isfinite(data[:, time_idx]) & np.isfinite(data[:, ch2_idx])
    data = data[valid_mask]
    if len(data) == 0:
        raise ValueError("No valid data rows in CSV.")

    t = data[:, time_idx].astype(float)
    ch2 = data[:, ch2_idx].astype(float)

    order = np.argsort(t)
    t = t[order]
    ch2 = ch2[order]

    unique_mask = np.concatenate(([True], np.diff(t) > 0))
    t = t[unique_mask]
    ch2 = ch2[unique_mask]
    if len(t) < 8:
        raise ValueError("Not enough valid CH2 samples.")

    return t, ch2


def select_time_segment(t, signal, start_s, end_s):
    mask = np.ones_like(t, dtype=bool)
    if start_s is not None:
        mask &= t >= start_s
    if end_s is not None:
        mask &= t <= end_s

    t_seg = t[mask]
    signal_seg = signal[mask]
    if len(t_seg) < 32:
        raise ValueError("Selected segment is too short for 8th-order IIR filtering.")

    return t_seg, signal_seg


def estimate_sampling_rate(t):
    dt = np.diff(t)
    dt_valid = dt[np.isfinite(dt) & (dt > 0)]
    if len(dt_valid) == 0:
        raise ValueError("Invalid Time_Rel column.")
    return 1.0 / np.median(dt_valid)


def validate_band(low_hz, high_hz, fs):
    nyquist = fs * 0.5
    if low_hz <= 0:
        raise ValueError("Band low frequency must be greater than 0 Hz.")
    if high_hz >= nyquist:
        raise ValueError(f"Band high frequency {high_hz:.3f} Hz must be below Nyquist {nyquist:.3f} Hz.")
    if low_hz >= high_hz:
        raise ValueError("Band low frequency must be lower than high frequency.")


def design_filter(filter_key, low_hz, high_hz, fs):
    validate_band(low_hz, high_hz, fs)
    normalized_band = [low_hz, high_hz]

    if filter_key == "butterworth8":
        return butter(FILTER_ORDER, normalized_band, btype="bandpass", fs=fs, output="sos")

    if filter_key == "chebyshev1_8_rp0p5db":
        return cheby1(FILTER_ORDER, CHEBY_RP_DB, normalized_band, btype="bandpass", fs=fs, output="sos")

    raise ValueError(f"Unknown filter: {filter_key}")


def apply_filter(signal, sos):
    signal = np.asarray(signal, dtype=float)
    return sosfiltfilt(sos, signal)


def compute_fft(signal, fs):
    signal = np.asarray(signal, dtype=float)
    signal = signal - np.mean(signal)

    n = len(signal)
    window = np.hanning(n)
    spectrum = np.fft.rfft(signal * window)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    amps = np.abs(spectrum) * 2.0 / np.sum(window)
    return freqs, amps


def save_filtered_csv(t, raw, filtered_results, output_path, band_name):
    file_path = os.path.join(output_path, f"CH2_{band_name}_filtered.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        headers = ["Time_s", "CH2_Raw"]
        for filter_key in FILTERS:
            headers.append(f"CH2_{FILTERS[filter_key]['csv_name']}")
        f.write(",".join(headers) + "\n")

        for i in range(len(t)):
            row = [f"{t[i]:.6f}", f"{raw[i]:.6f}"]
            for filter_key in FILTERS:
                row.append(f"{filtered_results[filter_key][i]:.6f}")
            f.write(",".join(row) + "\n")
    return file_path


def save_fft_csv(freqs, raw_amps, filtered_fft_results, output_path, band_name):
    file_path = os.path.join(output_path, f"CH2_{band_name}_fft.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        headers = ["Frequency_Hz", "CH2_Raw_Amplitude"]
        for filter_key in FILTERS:
            headers.append(f"CH2_{FILTERS[filter_key]['csv_name']}_Amplitude")
        f.write(",".join(headers) + "\n")

        for i in range(len(freqs)):
            row = [f"{freqs[i]:.6f}", f"{raw_amps[i]:.8f}"]
            for filter_key in FILTERS:
                row.append(f"{filtered_fft_results[filter_key][i]:.8f}")
            f.write(",".join(row) + "\n")
    return file_path


def save_single_filtered_csv(t, raw, filtered, output_path, band_name, filter_key):
    cfg = FILTERS[filter_key]
    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_filtered.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("Time_s,CH2_Raw,CH2_Filtered\n")
        for ti, raw_value, filtered_value in zip(t, raw, filtered):
            f.write(f"{ti:.6f},{raw_value:.6f},{filtered_value:.6f}\n")
    return file_path


def save_single_fft_csv(freqs, raw_amps, filtered_amps, output_path, band_name, filter_key):
    cfg = FILTERS[filter_key]
    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_fft.csv")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("Frequency_Hz,CH2_Raw_Amplitude,CH2_Filtered_Amplitude\n")
        for freq, raw_amp, filtered_amp in zip(freqs, raw_amps, filtered_amps):
            f.write(f"{freq:.6f},{raw_amp:.8f},{filtered_amp:.8f}\n")
    return file_path


def plot_waveforms(t, raw, filtered_results, output_path, band_name, low_hz, high_hz):
    fig, axes = plt.subplots(3, 1, figsize=(14, 9), sharex=True)
    fig.suptitle(f"CH2 {band_name} bandpass waveform ({low_hz:.1f}-{high_hz:.1f} Hz)", fontweight="bold")

    axes[0].plot(t, raw, color="#4D4D4D", lw=1.0)
    axes[0].set_title("Raw CH2")
    axes[0].set_xlabel("Time (s)")
    axes[0].set_ylabel("Raw")
    axes[0].tick_params(labelbottom=True)
    axes[0].grid(True, alpha=0.3)

    for ax, filter_key in zip(axes[1:], FILTERS):
        cfg = FILTERS[filter_key]
        filtered = filtered_results[filter_key]
        ax.plot(t, filtered, color=cfg["color"], lw=1.0)
        ax.set_title(cfg["label"])
        ax.set_ylabel("Filtered")
        ax.grid(True, alpha=0.3)

    axes[-1].set_xlabel("Time (s)")
    axes[-1].set_xlim(float(t[0]), float(t[-1]))
    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_{band_name}_waveforms.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_single_waveform(t, raw, filtered, output_path, band_name, filter_key, low_hz, high_hz):
    cfg = FILTERS[filter_key]
    fig, axes = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    fig.suptitle(
        f"CH2 {band_name} {cfg['label']} waveform ({low_hz:.1f}-{high_hz:.1f} Hz)",
        fontweight="bold",
    )

    axes[0].plot(t, raw, color="#4D4D4D", lw=1.0)
    axes[0].set_title("Raw CH2")
    axes[0].set_xlabel("Time (s)")
    axes[0].set_ylabel("Raw")
    axes[0].tick_params(labelbottom=True)
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(t, filtered, color=cfg["color"], lw=1.0)
    axes[1].set_title(cfg["label"])
    axes[1].set_ylabel("Filtered")
    axes[1].set_xlabel("Time (s)")
    axes[1].grid(True, alpha=0.3)
    axes[1].set_xlim(float(t[0]), float(t[-1]))

    fig.tight_layout()
    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_waveform.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_5s_waveforms(t, raw, filtered, output_path, band_name, filter_key):
    cfg = FILTERS[filter_key]
    segment_dir = os.path.join(output_path, "segments_5s")
    os.makedirs(segment_dir, exist_ok=True)

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
            raw_seg = raw[mask]
            filt_seg = filtered[mask]

            fig, axes = plt.subplots(2, 1, figsize=(12, 6), sharex=True)
            fig.suptitle(
                f"CH2 {band_name} {cfg['label']} {segment_start:.1f}-{segment_end:.1f}s",
                fontweight="bold",
            )

            axes[0].plot(t_seg, raw_seg, color="#4D4D4D", lw=1.0)
            axes[0].set_xlabel("Time (s)")
            axes[0].set_ylabel("Raw")
            axes[0].tick_params(labelbottom=True)
            axes[0].grid(True, alpha=0.3)

            axes[1].plot(t_seg, filt_seg, color=cfg["color"], lw=1.0)
            axes[1].set_ylabel("Filtered")
            axes[1].set_xlabel("Time (s)")
            axes[1].grid(True, alpha=0.3)
            axes[1].set_xlim(segment_start, segment_end)

            fig.tight_layout()
            start_text = f"{segment_start:06.1f}".replace(".", "p")
            end_text = f"{segment_end:06.1f}".replace(".", "p")
            file_name = f"CH2_{band_name}_{cfg['csv_name']}_{start_text}-{end_text}s.png"
            file_path = os.path.join(segment_dir, file_name)
            fig.savefig(file_path, dpi=150)
            plt.close(fig)
            saved_files.append(file_path)

        segment_start += SEGMENT_PLOT_SECONDS

    return saved_files


def draw_target_band_lines(ax, band_name, low_hz, high_hz):
    color = BANDS[band_name]["color"]
    ax.axvline(low_hz, color=color, linestyle="--", lw=1.0, alpha=0.8, label="Target band")
    ax.axvline(high_hz, color=color, linestyle="--", lw=1.0, alpha=0.8)


def plot_fft(freqs, raw_amps, filtered_fft_results, output_path, band_name, low_hz, high_hz, fs):
    fig, ax = plt.subplots(figsize=(13, 5))
    x_max = min(high_hz * 1.6, fs * 0.5)
    x_min = max(0.0, low_hz * 0.4)

    ax.plot(freqs, raw_amps, color="#999999", lw=0.9, alpha=0.75, label="Raw")
    for filter_key, amps in filtered_fft_results.items():
        cfg = FILTERS[filter_key]
        ax.plot(freqs, amps, color=cfg["color"], lw=1.2, label=cfg["label"])

    draw_target_band_lines(ax, band_name, low_hz, high_hz)
    ax.set_title(f"CH2 {band_name} FFT ({low_hz:.1f}-{high_hz:.1f} Hz)", fontweight="bold")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Amplitude")
    ax.set_xlim(x_min, x_max)

    mask = (freqs >= x_min) & (freqs <= x_max)
    if np.any(mask):
        y_candidates = [raw_amps[mask]]
        for amps in filtered_fft_results.values():
            y_candidates.append(amps[mask])
        y_stack = np.concatenate(y_candidates)
        y_max = np.percentile(y_stack, FFT_Y_MAX_PERCENTILE) * 1.25
        ax.set_ylim(0, max(float(y_max), 1.0))

    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_{band_name}_fft.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_single_fft(freqs, raw_amps, filtered_amps, output_path, band_name, filter_key, low_hz, high_hz, fs):
    cfg = FILTERS[filter_key]
    x_max = min(high_hz * 1.6, fs * 0.5)
    x_min = max(0.0, low_hz * 0.4)

    fig, axes = plt.subplots(2, 1, figsize=(13, 8), sharex=True)
    fig.suptitle(f"CH2 {band_name} {cfg['label']} FFT ({low_hz:.1f}-{high_hz:.1f} Hz)", fontweight="bold")

    axes[0].plot(freqs, raw_amps, color="#666666", lw=0.9, label="Raw")
    draw_target_band_lines(axes[0], band_name, low_hz, high_hz)
    axes[0].set_title("Raw CH2")
    axes[0].set_ylabel("Raw FFT")
    axes[0].set_xlim(x_min, x_max)

    mask = (freqs >= x_min) & (freqs <= x_max)
    if np.any(mask):
        y_max = np.percentile(raw_amps[mask], FFT_Y_MAX_PERCENTILE) * 1.25
        axes[0].set_ylim(0, max(float(y_max), 1.0))

    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(freqs, filtered_amps, color=cfg["color"], lw=1.2, label=cfg["label"])
    draw_target_band_lines(axes[1], band_name, low_hz, high_hz)
    axes[1].set_title(cfg["label"])
    axes[1].set_xlabel("Frequency (Hz)")
    axes[1].set_ylabel("Filtered FFT")
    axes[1].set_xlim(x_min, x_max)

    if np.any(mask):
        y_max = np.percentile(filtered_amps[mask], FFT_Y_MAX_PERCENTILE) * 1.25
        axes[1].set_ylim(0, max(float(y_max), 1.0))

    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")
    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_fft.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return [file_path]


def plot_filter_response(sos_results, output_path, band_name, low_hz, high_hz, fs):
    fig, ax = plt.subplots(figsize=(13, 5))

    for filter_key, sos in sos_results.items():
        cfg = FILTERS[filter_key]
        freqs, response = sosfreqz(sos, worN=4096, fs=fs)
        mag_db = 20.0 * np.log10(np.maximum(np.abs(response), 1e-12))
        ax.plot(freqs, mag_db, color=cfg["color"], lw=1.2, label=cfg["label"])

    draw_target_band_lines(ax, band_name, low_hz, high_hz)
    ax.set_title(f"CH2 {band_name} filter response ({low_hz:.1f}-{high_hz:.1f} Hz)", fontweight="bold")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_xlim(0, min(high_hz * 2.0, fs * 0.5))
    ax.set_ylim(-80, 5)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="lower right")
    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_{band_name}_filter_response.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_single_filter_response(sos, output_path, band_name, filter_key, low_hz, high_hz, fs):
    cfg = FILTERS[filter_key]
    freqs, response = sosfreqz(sos, worN=4096, fs=fs)
    mag_db = 20.0 * np.log10(np.maximum(np.abs(response), 1e-12))

    fig, ax = plt.subplots(figsize=(13, 5))
    ax.plot(freqs, mag_db, color=cfg["color"], lw=1.2, label=cfg["label"])
    draw_target_band_lines(ax, band_name, low_hz, high_hz)
    ax.set_title(f"CH2 {band_name} {cfg['label']} response ({low_hz:.1f}-{high_hz:.1f} Hz)", fontweight="bold")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_xlim(0, min(high_hz * 2.0, fs * 0.5))
    ax.set_ylim(-80, 5)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="lower right")
    fig.tight_layout()

    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_filter_response.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def plot_filter_response_by_filter(root_output_path, filter_key, fs):
    cfg = FILTERS[filter_key]
    response_dir = os.path.join(root_output_path, "filter_response")
    os.makedirs(response_dir, exist_ok=True)

    fig, ax = plt.subplots(figsize=(13, 5))
    for band_name, band_cfg in BANDS.items():
        sos = design_filter(filter_key, band_cfg["low"], band_cfg["high"], fs)
        freqs, response = sosfreqz(sos, worN=4096, fs=fs)
        mag_db = 20.0 * np.log10(np.maximum(np.abs(response), 1e-12))
        label = f"{band_cfg['low']:.1f}-{band_cfg['high']:.1f}Hz Response"
        ax.plot(freqs, mag_db, color=band_cfg["color"], lw=1.6, label=label)
        ax.axvline(band_cfg["low"], color=band_cfg["color"], linestyle="--", lw=1.0, alpha=0.75)
        ax.axvline(band_cfg["high"], color=band_cfg["color"], linestyle="--", lw=1.0, alpha=0.75)

    ax.axhline(-3.0, color="#FF7F0E", linestyle="-.", lw=1.0, label="-3dB Limit")
    ax.set_title(f"{cfg['label']} Magnitude Response")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Amplitude (dB)")
    ax.set_xlim(0, min(40.0, fs * 0.5))
    ax.set_ylim(-60, 5)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend(loc="lower right")
    fig.tight_layout()

    file_path = os.path.join(response_dir, f"{cfg['csv_name']}_magnitude_response.png")
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def save_summary(output_path, csv_path, band_name, start_s, end_s, fs, low_hz, high_hz):
    file_path = os.path.join(output_path, f"CH2_{band_name}_filter_summary.txt")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("CH2 IIR filter summary\n")
        f.write(f"Input CSV: {csv_path}\n")
        f.write(f"Band: {band_name}\n")
        f.write(f"Segment: {start_s:.3f} ~ {end_s:.3f} s\n")
        f.write(f"Estimated Fs: {fs:.6f} Hz\n")
        f.write(f"Bandpass: {low_hz:.3f} ~ {high_hz:.3f} Hz\n")
        f.write(f"Butterworth order: {FILTER_ORDER}\n")
        f.write(f"Chebyshev Type I order: {FILTER_ORDER}\n")
        f.write(f"Chebyshev Type I rp: {CHEBY_RP_DB:.3f} dB\n")
        f.write("Filtering method: scipy.signal.sosfiltfilt zero-phase filtering\n")
    return file_path


def save_single_summary(output_path, csv_path, band_name, filter_key, start_s, end_s, fs, low_hz, high_hz):
    cfg = FILTERS[filter_key]
    file_path = os.path.join(output_path, f"CH2_{band_name}_{cfg['csv_name']}_filter_summary.txt")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("CH2 IIR filter summary\n")
        f.write(f"Input CSV: {csv_path}\n")
        f.write(f"Band: {band_name}\n")
        f.write(f"Filter: {cfg['label']}\n")
        f.write(f"Segment: {start_s:.3f} ~ {end_s:.3f} s\n")
        f.write(f"Estimated Fs: {fs:.6f} Hz\n")
        f.write(f"Bandpass: {low_hz:.3f} ~ {high_hz:.3f} Hz\n")
        f.write(f"Order: {FILTER_ORDER}\n")
        if filter_key == "chebyshev1_8_rp0p5db":
            f.write(f"Ripple rp: {CHEBY_RP_DB:.3f} dB\n")
        f.write("Filtering method: scipy.signal.sosfiltfilt zero-phase filtering\n")
        f.write(f"5-second segment plots: segments_5s\n")
    return file_path


def run_band(t, ch2, fs, csv_path, root_output_path, band_name):
    band_cfg = BANDS[band_name]
    low_hz = band_cfg["low"]
    high_hz = band_cfg["high"]
    output_path = os.path.join(root_output_path, band_name)
    os.makedirs(output_path, exist_ok=True)

    freqs, raw_amps = compute_fft(ch2, fs)
    outputs = []

    for filter_key in FILTERS:
        filter_output_path = os.path.join(output_path, filter_key)
        os.makedirs(filter_output_path, exist_ok=True)

        sos = design_filter(filter_key, low_hz, high_hz, fs)
        filtered = apply_filter(ch2, sos)
        filt_freqs, filt_amps = compute_fft(filtered, fs)
        if not np.allclose(freqs, filt_freqs):
            raise ValueError("FFT frequency bins do not match.")

        outputs.extend([
            save_single_filtered_csv(t, ch2, filtered, filter_output_path, band_name, filter_key),
            save_single_fft_csv(freqs, raw_amps, filt_amps, filter_output_path, band_name, filter_key),
            plot_single_waveform(t, ch2, filtered, filter_output_path, band_name, filter_key, low_hz, high_hz),
            save_single_summary(
                filter_output_path,
                csv_path,
                band_name,
                filter_key,
                float(t[0]),
                float(t[-1]),
                fs,
                low_hz,
                high_hz,
            ),
        ])
        outputs.extend(plot_single_fft(freqs, raw_amps, filt_amps, filter_output_path, band_name, filter_key, low_hz, high_hz, fs))
        outputs.extend(plot_5s_waveforms(t, ch2, filtered, filter_output_path, band_name, filter_key))

    return outputs


def parse_args():
    parser = argparse.ArgumentParser(description="Apply CH2 Butterworth8 and Chebyshev Type I 8 bandpass filters.")
    parser.add_argument("csv_file", nargs="?", default=None, help="Input CSV path")
    parser.add_argument("--start", type=float, default=DEFAULT_START_SEC, help="Start time in seconds")
    parser.add_argument("--end", type=float, default=DEFAULT_END_SEC, help="End time in seconds")
    parser.add_argument("--output", default=DEFAULT_OUTPUT_ROOT, help="Output root directory")
    return parser.parse_args()


def main():
    args = parse_args()
    if not args.csv_file:
        args.csv_file = select_csv_file()
    if not args.csv_file:
        print("[ERROR] No CSV file selected.")
        sys.exit(1)

    csv_path = os.path.abspath(args.csv_file)
    output_root = os.path.abspath(args.output)

    t, ch2 = load_csv(csv_path)
    t, ch2 = select_time_segment(t, ch2, args.start, args.end)
    fs = estimate_sampling_rate(t)

    root_output_path = ensure_output_dir(csv_path, output_root, args.start, args.end)
    all_outputs = []
    for band_name in ("heart", "respiration"):
        all_outputs.extend(run_band(t, ch2, fs, csv_path, root_output_path, band_name))
    for filter_key in FILTERS:
        all_outputs.append(plot_filter_response_by_filter(root_output_path, filter_key, fs))

    print(f"[INFO] Input: {csv_path}")
    print(f"[INFO] Samples: {len(t)}, segment: {t[0]:.3f}~{t[-1]:.3f}s, Fs~{fs:.2f}Hz")
    print(f"[INFO] Output: {root_output_path}")
    for file_path in all_outputs:
        print(f"[INFO] Saved: {file_path}")


if __name__ == "__main__":
    main()
