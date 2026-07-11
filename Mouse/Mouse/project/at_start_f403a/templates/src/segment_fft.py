#!/usr/bin/env python3

import os
import sys
import argparse
from datetime import datetime
import tkinter as tk
from tkinter import filedialog, simpledialog
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt


DEFAULT_LOW = 4.0
DEFAULT_HIGH = 15.0
DEFAULT_ORDER = 4
RAW_FFT_MIN_DISPLAY_HZ = 0.5
FFT_Y_MAX_PERCENTILE = 99.5


def load_csv(csv_path):
    data = np.genfromtxt(csv_path, delimiter=',', skip_header=1)
    if data.ndim == 1:
        data = np.expand_dims(data, axis=0)

    if data.shape[1] < 5:
        raise ValueError('CSV columns insufficient (expected Time + 4 channels)')

    valid_mask = np.all(np.isfinite(data[:, :5]), axis=1)
    data = data[valid_mask]
    if len(data) == 0:
        raise ValueError('No valid data rows in CSV')

    t = data[:, 0]
    ch1, ch2, ch3, ch4 = data[:, 1], data[:, 2], data[:, 3], data[:, 4]
    return t, ch1, ch2, ch3, ch4


def estimate_sampling_rate(t):
    dt = np.diff(t)
    dt_valid = dt[np.isfinite(dt) & (dt > 0)]
    if len(dt_valid) == 0:
        raise ValueError('Invalid time column')
    return 1.0 / np.median(dt_valid)


def compute_fft(signal, fs):
    signal = np.asarray(signal, dtype=float)
    signal = signal - np.mean(signal)

    n = len(signal)
    window = np.hanning(n)
    spectrum = np.fft.rfft(signal * window)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    amps = np.abs(spectrum) * 2.0 / np.sum(window)
    return freqs, amps


def apply_bandpass_filter(signal, fs, low, high, order):
    nyquist = fs / 2.0

    if high >= nyquist:
        high = nyquist * 0.99
    if low >= nyquist:
        return signal

    low_norm = np.clip(low / nyquist, 1e-6, 0.9999)
    high_norm = np.clip(high / nyquist, 1e-6, 0.9999)

    if low_norm >= high_norm:
        return signal

    b, a = butter(order, [low_norm, high_norm], btype='band')
    return filtfilt(b, a, signal)


def save_fft_csv(freqs, amps, output_path, ch_name, fft_type):
    file_path = os.path.join(output_path, f'{ch_name}_{fft_type}_fft.csv')
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write('Frequency_Hz,Amplitude\n')
        for freq, amp in zip(freqs, amps):
            f.write(f'{freq:.6f},{amp:.6f}\n')
    return file_path


def plot_fft_comparison(freqs_raw, amps_raw, freqs_filt, amps_filt, ch_name, output_path, fs, low, high):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 9))
    fig.suptitle(f'{ch_name} - Segment FFT Analysis (Raw vs Filtered)', fontsize=14, fontweight='bold')

    nyquist = fs * 0.5
    xlim = min(40.0, nyquist)

    ax1.plot(freqs_raw, amps_raw, 'b-', lw=1.0, alpha=0.8, label='Raw FFT')
    ax1.set_title(f'Raw FFT (Fs~{fs:.2f} Hz)')
    ax1.set_xlabel('Frequency (Hz)')
    ax1.set_ylabel('Amplitude')
    ax1.set_xlim(RAW_FFT_MIN_DISPLAY_HZ, xlim)
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    ax2.plot(freqs_filt, amps_filt, 'r-', lw=1.2, label=f'Filtered FFT ({low}-{high} Hz)')
    ax2.axvline(low, color='orange', linestyle='--', alpha=0.7, label='Filter Band')
    ax2.axvline(high, color='orange', linestyle='--', alpha=0.7)
    ax2.set_title('Filtered FFT')
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('Amplitude')
    ax2.set_xlim(0, xlim)
    ax2.grid(True, alpha=0.3)
    ax2.legend()

    raw_mask = (freqs_raw >= RAW_FFT_MIN_DISPLAY_HZ) & (freqs_raw <= xlim)
    filt_mask = (freqs_filt >= 0) & (freqs_filt <= xlim)
    raw_ymax = np.percentile(amps_raw[raw_mask], FFT_Y_MAX_PERCENTILE) * 1.2 if np.any(raw_mask) else 1.0
    filt_ymax = np.percentile(amps_filt[filt_mask], FFT_Y_MAX_PERCENTILE) * 1.2 if np.any(filt_mask) else 1.0
    common_ymax = max(1.0, raw_ymax, filt_ymax)
    ax1.set_ylim(0, common_ymax)
    ax2.set_ylim(0, common_ymax)

    plt.tight_layout()
    file_path = os.path.join(output_path, f'{ch_name}_segment_fft_comparison.png')
    fig.savefig(file_path, dpi=150)
    plt.close(fig)
    return file_path


def extract_segment(t, channels, start_s, end_s):
    mask = (t >= start_s) & (t <= end_s)
    if not np.any(mask):
        raise ValueError(f'No data points in selected range: {start_s}~{end_s}s')

    t_seg = t[mask]
    ch_seg = [ch[mask] for ch in channels]
    return t_seg, ch_seg


def ensure_output_dir(csv_path, start_s, end_s, low, high, base_dir='output'):
    base_name = os.path.splitext(os.path.basename(csv_path))[0]
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    folder = f'{base_name}_segment_{start_s:.1f}-{end_s:.1f}s_Butterworth_{low}-{high}Hz_{timestamp}'
    output_path = os.path.join(base_dir, folder)
    os.makedirs(output_path, exist_ok=True)
    return output_path


def parse_args():
    parser = argparse.ArgumentParser(description='Segment FFT analyzer for 4-channel load-cell CSV')
    parser.add_argument('csv_file', nargs='?', help='Input CSV file path')
    parser.add_argument('--start', type=float, help='Segment start time (s)')
    parser.add_argument('--end', type=float, help='Segment end time (s)')
    parser.add_argument('--low', type=float, default=DEFAULT_LOW, help='Bandpass low cutoff (Hz)')
    parser.add_argument('--high', type=float, default=DEFAULT_HIGH, help='Bandpass high cutoff (Hz)')
    parser.add_argument('--order', type=int, default=DEFAULT_ORDER, help='Butterworth order')
    parser.add_argument('--output', default='output', help='Output directory root')
    return parser.parse_args()


def prompt_missing_args(args):
    """若缺少必要參數，透過對話框補齊。"""
    root = tk.Tk()
    root.withdraw()
    root.attributes('-topmost', True)

    if not args.csv_file:
        args.csv_file = filedialog.askopenfilename(
            title='Select CSV file',
            filetypes=[('CSV Files', '*.csv'), ('All Files', '*.*')]
        )

    if args.start is None:
        args.start = simpledialog.askfloat('Segment Start', '請輸入區段起點秒數 (start):', minvalue=0.0)

    if args.end is None:
        args.end = simpledialog.askfloat('Segment End', '請輸入區段終點秒數 (end):', minvalue=0.0)

    root.destroy()
    return args


def main():
    args = parse_args()

    if not args.csv_file or args.start is None or args.end is None:
        args = prompt_missing_args(args)

    if not args.csv_file:
        print('[INFO] No file selected. Exiting.')
        sys.exit(0)

    if args.start is None or args.end is None:
        print('[INFO] Start/End not provided. Exiting.')
        sys.exit(0)

    if args.end <= args.start:
        print('[ERROR] --end must be greater than --start')
        sys.exit(1)

    if not os.path.exists(args.csv_file):
        print(f'[ERROR] File not found: {args.csv_file}')
        sys.exit(1)

    try:
        t, ch1, ch2, ch3, ch4 = load_csv(args.csv_file)
        t_seg, channels_seg = extract_segment(t, [ch1, ch2, ch3, ch4], args.start, args.end)
        fs = estimate_sampling_rate(t_seg)
    except Exception as e:
        print(f'[ERROR] {e}')
        sys.exit(1)

    output_path = ensure_output_dir(args.csv_file, args.start, args.end, args.low, args.high, args.output)
    print(f'[INFO] Output directory: {output_path}')
    print(f'[INFO] Segment: {args.start:.3f}~{args.end:.3f}s, points={len(t_seg)}, Fs~{fs:.2f} Hz')

    for idx, sig in enumerate(channels_seg, 1):
        ch_name = f'CH{idx}'
        freqs_raw, amps_raw = compute_fft(sig, fs)

        sig_filt = apply_bandpass_filter(sig, fs, args.low, args.high, args.order)
        freqs_filt, amps_filt = compute_fft(sig_filt, fs)

        save_fft_csv(freqs_raw, amps_raw, output_path, ch_name, 'raw')
        save_fft_csv(freqs_filt, amps_filt, output_path, ch_name, 'filtered')
        plot_fft_comparison(freqs_raw, amps_raw, freqs_filt, amps_filt, ch_name, output_path, fs, args.low, args.high)
        print(f'[INFO] {ch_name} done')

    print('[INFO] Segment FFT analysis complete')


if __name__ == '__main__':
    main()
