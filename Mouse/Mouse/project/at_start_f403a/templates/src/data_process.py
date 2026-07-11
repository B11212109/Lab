#!/usr/bin/env python3
"""
Load Cell CSV 完整分析工具
功能：原始波形 → FFT → 帶通濾波(3-8Hz) → 濾波波形 → 濾波FFT
輸出：4份CSV（時間序列） + 4張圖檔
"""

import os
import sys
import argparse
from datetime import datetime
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

# ========== 設定 ==========
BUTTER_ORDER = 4
FILTER_BANDS = [
    {"name": "4_15Hz", "low": 4.0, "high": 15.0},
    {"name": "0p5_3Hz", "low": 0.5, "high": 3.0},
]
RAW_Y_PADDING_RATIO = 0.2   # [修正] 原始波形依最大/最小值上下拓展的比例 (取代寫死的數值)
FILT_Y_PADDING_RATIO = 0.2  # [新增] 濾波波形依最大/最小值上下拓展的比例 (例如 0.2 表示 20%)
RAW_FFT_MIN_DISPLAY_HZ = 0.5  # Raw FFT 圖表起始顯示頻率，避免 0Hz 分量壓縮可視化
RAW_FFT_Y_MAX_PERCENTILE = 99.5  # Raw FFT Y 軸上限百分位，避免少數尖峰壓扁主體頻譜
FILT_FFT_Y_MAX_PERCENTILE = 99.5 # Filtered FFT Y 軸上限百分位

def ensure_output_dir(csv_path, duration, base_dir="output"):
    """建立以輸入檔名、濾波參數與錄製長度命名的輸出資料夾"""
    base_name = os.path.splitext(os.path.basename(csv_path))[0]
    band_str = "_".join([f"{b['low']}-{b['high']}Hz" for b in FILTER_BANDS])
    folder_name = f"{base_name}_Butterworth_{band_str}_{duration:.1f}s"
    output_path = os.path.join(base_dir, folder_name)
    os.makedirs(output_path, exist_ok=True)
    print(f"[INFO] Output directory: {output_path}")
    return output_path

def load_csv(csv_path):
    """載入 CSV 檔，返回時間與各通道資料"""
    try:
        data = np.genfromtxt(csv_path, delimiter=',', skip_header=1)
        if data.ndim == 1:
            data = np.expand_dims(data, axis=0)
        
        if data.shape[1] < 5:
            raise ValueError("CSV columns insufficient (expected Time + 4 channels)")
        
        # 篩選有效資料（所有欄位都是有限數字）
        valid_mask = np.all(np.isfinite(data[:, :5]), axis=1)
        data = data[valid_mask]
        
        t = data[:, 0]
        ch1, ch2, ch3, ch4 = data[:, 1], data[:, 2], data[:, 3], data[:, 4]
        
        print(f"[INFO] Loading {csv_path}")
        print(f"[INFO] Data points: {len(t)}, Time range: {t[0]:.3f}~{t[-1]:.3f}s")
        
        return t, ch1, ch2, ch3, ch4
    except Exception as e:
        print(f"[ERROR] Failed to read CSV: {e}")
        sys.exit(1)

def estimate_sampling_rate(t):
    """估算取樣率"""
    dt = np.diff(t)
    dt_valid = dt[np.isfinite(dt) & (dt > 0)]
    if len(dt_valid) == 0:
        raise ValueError("Invalid time column")
    fs = 1.0 / np.median(dt_valid)
    print(f"[INFO] Estimated sampling rate: {fs:.2f} Hz")
    return fs

def compute_fft(signal, fs):
    """計算 FFT 頻譜"""
    signal = np.asarray(signal, dtype=float)
    signal = signal - np.mean(signal)  # 去 DC
    
    n = len(signal)
    window = np.hanning(n)
    spectrum = np.fft.rfft(signal * window)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    amps = np.abs(spectrum) * 2.0 / np.sum(window)
    
    return freqs, amps

def apply_bandpass_filter(signal, fs, low, high, order=BUTTER_ORDER):
    """套用帶通濾波"""
    # 根據奈奎斯特取樣定理，最高頻率只能是採樣率的一半
    nyquist = fs / 2.0
    
    # 動態保護防錯：如果設定的 high 高於或等於 nyquist，強制降到極限值以下 (0.99倍)
    if high >= nyquist:
        print(f"[WARN] Filter HIGH ({high}Hz) >= Nyquist ({nyquist}Hz). Auto-adjusting to {nyquist * 0.99:.2f}Hz.")
        high = nyquist * 0.99
    # 如果設定的 low 也大於等於 nyquist，那我們就沒辦法做帶通，乾脆不濾波    
    if low >= nyquist:
        print("[WARN] Filter LOW is greater than Nyquist limit. Skipping filter.")
        return signal

    low_norm = low / nyquist
    high_norm = high / nyquist
    
    # 確保正規化頻率在 (0, 1) 範圍內
    low_norm = np.clip(low_norm, 1e-6, 0.9999)
    high_norm = np.clip(high_norm, 1e-6, 0.9999)
    
    if low_norm >= high_norm:
        print("[WARN] Filter band setting error, skipping filter")
        return signal
    
    b, a = butter(order, [low_norm, high_norm], btype='band')
    filtered = filtfilt(b, a, signal)
    return filtered

def save_waveform_csv(t, signal, output_path, ch_name, waveform_type):
    """儲存波形 CSV"""
    filename = os.path.join(output_path, f"{ch_name}_{waveform_type}_waveform.csv")
    with open(filename, 'w', encoding='utf-8') as f:
        f.write("Time_s,Amplitude\n")
        for ti, sig in zip(t, signal):
            f.write(f"{ti:.6f},{sig:.2f}\n")
    print(f"[INFO] Saved: {filename}")
    return filename

def save_fft_csv(freqs, amps, output_path, ch_name, fft_type):
    """儲存 FFT CSV"""
    filename = os.path.join(output_path, f"{ch_name}_{fft_type}_fft.csv")
    with open(filename, 'w', encoding='utf-8') as f:
        f.write("Frequency_Hz,Amplitude\n")
        for freq, amp in zip(freqs, amps):
            f.write(f"{freq:.6f},{amp:.6f}\n")
    print(f"[INFO] Saved: {filename}")
    return filename

def plot_waveform(t, raw_signal, filtered_results, ch_name, output_path):
    """繪製三個波形圖（Raw、4-15Hz、0.5-3Hz），每 5 秒分割為一張圖"""
    t_base = t[0]
    t_max = t[-1]
    total_duration = t_max - t_base
    num_segments = max(1, int(np.ceil(total_duration / 5.0)))
    
    saved_files = []
    for i in range(num_segments):
        t_start = t_base + i * 5.0
        t_end = min(t_start + 5.0, t_max)
        
        # 確保最後一段有資料
        if t_start >= t_max and i > 0:
            break
            
        fig, (ax_raw, ax_4_15, ax_0p5_3) = plt.subplots(3, 1, figsize=(14, 12))
        fig.suptitle(
            f"{ch_name} - Waveform Analysis (Raw, 4-15Hz, 0.5-3Hz) - Part {i+1} ({t_start-t_base:.1f}s ~ {t_end-t_base:.1f}s)",
            fontsize=14,
            fontweight='bold'
        )
        
        # 尋找該 5 秒段內的最大值與最小值
        mask = (t >= t_start) & (t <= t_start + 5.0)
        if np.any(mask):
            min_val = np.min(raw_signal[mask])
            max_val = np.max(raw_signal[mask])
        else:
            min_val, max_val = np.min(raw_signal), np.max(raw_signal)
            
        r_span = max_val - min_val
        if r_span == 0: r_span = 1.0 # 避免為 0
        r_pad = r_span * RAW_Y_PADDING_RATIO

        # 取得兩組濾波訊號
        filt_4_15 = filtered_results["4_15Hz"]["waveform"]
        filt_0p5_3 = filtered_results["0p5_3Hz"]["waveform"]

        # 各自計算濾波圖的 Y 軸範圍
        if np.any(mask):
            f1_min_val = np.min(filt_4_15[mask])
            f1_max_val = np.max(filt_4_15[mask])
            f2_min_val = np.min(filt_0p5_3[mask])
            f2_max_val = np.max(filt_0p5_3[mask])
        else:
            f1_min_val, f1_max_val = np.min(filt_4_15), np.max(filt_4_15)
            f2_min_val, f2_max_val = np.min(filt_0p5_3), np.max(filt_0p5_3)

        f1_span = f1_max_val - f1_min_val
        if f1_span == 0:
            f1_span = 1.0
        f1_pad = f1_span * FILT_Y_PADDING_RATIO

        f2_span = f2_max_val - f2_min_val
        if f2_span == 0:
            f2_span = 1.0
        f2_pad = f2_span * FILT_Y_PADDING_RATIO

        # 原始波形
        ax_raw.plot(t, raw_signal, 'b-', lw=1, alpha=0.8, label='Original')
        ax_raw.set_title(f"Raw Waveform ({t_start-t_base:.1f}s ~ {t_start-t_base+5.0:.1f}s)", fontweight='bold')
        ax_raw.set_xlabel("Time (s)")
        ax_raw.set_ylabel("Amplitude")
        ax_raw.set_xlim(t_start, t_start + 5.0)
        ax_raw.set_ylim(min_val - r_pad, max_val + r_pad)
        ax_raw.grid(True, alpha=0.3)
        ax_raw.legend(loc='upper right')

        ax_4_15.plot(t, filt_4_15, 'r-', lw=1, alpha=0.85, label='Bandpass (4-15 Hz)')
        ax_4_15.set_title(f"Filtered Waveform 4-15Hz ({t_start-t_base:.1f}s ~ {t_start-t_base+5.0:.1f}s)", fontweight='bold')
        ax_4_15.set_xlabel("Time (s)")
        ax_4_15.set_ylabel("Amplitude")
        ax_4_15.set_xlim(t_start, t_start + 5.0)
        ax_4_15.set_ylim(f1_min_val - f1_pad, f1_max_val + f1_pad)
        ax_4_15.grid(True, alpha=0.3)
        ax_4_15.legend(loc='upper right')

        ax_0p5_3.plot(t, filt_0p5_3, 'g-', lw=1, alpha=0.85, label='Bandpass (0.5-3 Hz)')
        ax_0p5_3.set_title(f"Filtered Waveform 0.5-3Hz ({t_start-t_base:.1f}s ~ {t_start-t_base+5.0:.1f}s)", fontweight='bold')
        ax_0p5_3.set_xlabel("Time (s)")
        ax_0p5_3.set_ylabel("Amplitude")
        ax_0p5_3.set_xlim(t_start, t_start + 5.0)
        ax_0p5_3.set_ylim(f2_min_val - f2_pad, f2_max_val + f2_pad)
        ax_0p5_3.grid(True, alpha=0.3)
        ax_0p5_3.legend(loc='upper right')
        
        plt.tight_layout()
        filename = os.path.join(output_path, f"{ch_name}_waveforms_combined_part{i+1}.png")
        fig.savefig(filename, dpi=150)
        plt.close(fig)
        print(f"[INFO] Saved segment: {filename}")
        saved_files.append(filename)
        
    return saved_files

def plot_fft(freqs_raw, amps_raw, filtered_results, ch_name, output_path, fs):
    """繪製原始與兩組濾波 FFT 對比圖（同一張圖分開顯示）"""
    fig, (ax_raw, ax_4_15, ax_0p5_3) = plt.subplots(3, 1, figsize=(14, 12))
    fig.suptitle(f"{ch_name} - FFT Analysis (Raw, 4-15Hz, 0.5-3Hz)", fontsize=14, fontweight='bold')
    
    nyquist = fs * 0.5
    xlim = min(40.0, nyquist)
    
    # 原始 FFT
    ax_raw.plot(freqs_raw, amps_raw, 'b-', lw=1.0, alpha=0.8, label='Raw FFT')
    ax_raw.set_title(f"Raw FFT (Fs~{fs:.2f} Hz)", fontweight='bold')
    ax_raw.set_xlabel("Frequency (Hz)")
    ax_raw.set_ylabel("Amplitude")
    ax_raw.set_xlim(RAW_FFT_MIN_DISPLAY_HZ, xlim)
    raw_mask = (freqs_raw >= RAW_FFT_MIN_DISPLAY_HZ) & (freqs_raw <= xlim)
    ax_raw.grid(True, alpha=0.3)
    ax_raw.legend()

    # 依指定順序分開畫兩個濾波頻帶
    band_axes = [
        ("4_15Hz", ax_4_15, 'r'),
        ("0p5_3Hz", ax_0p5_3, 'g')
    ]
    for band_name, ax, color in band_axes:
        if band_name not in filtered_results:
            continue
        band_cfg = next((b for b in FILTER_BANDS if b["name"] == band_name), None)
        if band_cfg is None:
            continue
        band_low = band_cfg["low"]
        band_high = band_cfg["high"]
        freqs_filt, amps_filt = filtered_results[band_name]['fft']

        ax.plot(freqs_filt, amps_filt, color=color, lw=1.2, label=f'Filtered FFT ({band_low}-{band_high} Hz)')
        ax.axvline(band_low, color=color, linestyle='--', alpha=0.7)
        ax.axvline(band_high, color=color, linestyle='--', alpha=0.7)
        ax.set_title(f"Filtered FFT ({band_low}-{band_high} Hz)", fontweight='bold')
        ax.set_xlabel("Frequency (Hz)")
        ax.set_ylabel("Amplitude")
        ax.set_xlim(0, xlim)
        ax.grid(True, alpha=0.3)
        ax.legend()

    # 統一兩張圖的 Y 軸上限，避免比較時視覺尺度不一致
    raw_ymax = np.percentile(amps_raw[raw_mask], RAW_FFT_Y_MAX_PERCENTILE) * 1.2 if np.any(raw_mask) else 1.0
    filt_ymax_list = []
    for band in FILTER_BANDS:
        band_name = band["name"]
        freqs_filt, amps_filt = filtered_results[band_name]['fft']
        filt_mask = (freqs_filt >= 0) & (freqs_filt <= xlim)
        if np.any(filt_mask):
            filt_ymax_list.append(np.percentile(amps_filt[filt_mask], FILT_FFT_Y_MAX_PERCENTILE) * 1.2)
    filt_ymax = max(filt_ymax_list) if filt_ymax_list else 1.0
    common_ymax = max(1.0, raw_ymax, filt_ymax)
    ax_raw.set_ylim(0, common_ymax)
    ax_4_15.set_ylim(0, common_ymax)
    ax_0p5_3.set_ylim(0, common_ymax)
    
    plt.tight_layout()
    filename = os.path.join(output_path, f"{ch_name}_fft_comparison_combined.png")
    fig.savefig(filename, dpi=150)
    plt.close(fig)
    print(f"[INFO] Saved: {filename}")
    return filename

def process_channel(t, signal, ch_name, output_path, fs):
    """處理單一通道的完整流程"""
    print(f"\n{'='*60}")
    print(f"Processing: {ch_name}")
    print(f"{'='*60}")
    
    # 檢查有效數據
    if np.sum(np.isfinite(signal)) < 16:
        print(f"[WARN] {ch_name} insufficient data, skipping")
        return None
    
    # 1. 原始波形 CSV
    save_waveform_csv(t, signal, output_path, ch_name, "raw")
    
    # 2. 原始 FFT
    freqs_raw, amps_raw = compute_fft(signal, fs)
    save_fft_csv(freqs_raw, amps_raw, output_path, ch_name, "raw")
    
    filtered_results = {}
    for band in FILTER_BANDS:
        band_name = band["name"]
        band_low = band["low"]
        band_high = band["high"]

        # 濾波波形
        filtered_signal = apply_bandpass_filter(signal, fs, band_low, band_high)
        save_waveform_csv(t, filtered_signal, output_path, ch_name, f"filtered_{band_name}")

        # 濾波 FFT
        freqs_filt, amps_filt = compute_fft(filtered_signal, fs)
        save_fft_csv(freqs_filt, amps_filt, output_path, ch_name, f"filtered_{band_name}")

        filtered_results[band_name] = {
            'waveform': filtered_signal,
            'fft': (freqs_filt, amps_filt)
        }

    # 繪圖（兩組頻帶結果放同一張圖）
    plot_waveform(t, signal, filtered_results, ch_name, output_path)
    plot_fft(freqs_raw, amps_raw, filtered_results, ch_name, output_path, fs)
    
    print(f"[✓] {ch_name} Processing complete")
    return {
        'ch_name': ch_name,
        'raw_waveform': signal,
        'raw_fft': (freqs_raw, amps_raw),
        'filtered_bands': filtered_results
    }

def main():
    parser = argparse.ArgumentParser(description='Load Cell CSV Complete Analysis Tool')
    parser.add_argument('csv_file', nargs='?', help='Input CSV file path (optional, will prompt if omitted)')
    parser.add_argument('--output', default='output', help='Output directory (default: output)')
    args = parser.parse_args()
    
    csv_file = args.csv_file
    
    # 如果沒有帶入參數，就彈出視窗讓使用者選檔案
    if not csv_file:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw() # 隱藏主視窗
        root.attributes('-topmost', True) # 讓選檔視窗跳到最上層
        csv_file = filedialog.askopenfilename(
            title="Please select the CSV file to analyze",
            filetypes=[("CSV Files", "*.csv"), ("All Files", "*.*")]
        )
        root.destroy() # [修正] 釋放 tkinter 資源
        if not csv_file:
            print("[INFO] No file selected. Exiting.")
            sys.exit(0)
    
    if not os.path.exists(csv_file):
        print(f"[ERROR] File not found: {csv_file}")
        sys.exit(1)
    
    # 載入數據 (必須先載入才能計算總錄製時間)
    t, ch1, ch2, ch3, ch4 = load_csv(csv_file)
    fs = estimate_sampling_rate(t)
    duration = t[-1] - t[0] if len(t) > 0 else 0.0
    
    # 建立專屬的輸出資料夾
    output_path = ensure_output_dir(csv_file, duration, args.output)
    
    band_info = ", ".join([f"{b['low']}-{b['high']}Hz" for b in FILTER_BANDS])
    print(f"\nStarting analysis... (Filter bands: {band_info})")
    
    # 處理各通道
    results = []
    for ch_idx, (signal, ch_name) in enumerate([(ch1, 'CH1'), (ch2, 'CH2'), (ch3, 'CH3'), (ch4, 'CH4')], 1):
        result = process_channel(t, signal, ch_name, output_path, fs)
        if result:
            results.append(result)
    
    print(f"\n{'='*60}")
    print(f"[✓] Analysis complete!")
    print(f"Output directory: {output_path}")
    print(f"Channels processed: {len(results)}")
    print(f"{'='*60}")

if __name__ == '__main__':
    main()
