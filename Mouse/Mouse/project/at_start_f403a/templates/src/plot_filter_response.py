#!/usr/bin/env python3
"""
檢視目前使用的 Butterworth 帶通濾波器之頻率響應 (Bode Plot)
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import butter, freqz

# ========== 濾波器參數 (與 data_process.py 保持一致) ==========
FS = 80.0          # 預估的硬體取樣率 (Hz)
BUTTER_ORDER = 4   # 濾波器階數
FILTER_BANDS = [
    (4.0, 15.0, '4-15Hz', 'b'),
    (0.5, 3.0, '0.5-3Hz', 'g'),
]

def plot_filter_response():
    # 奈奎斯特頻率 (取樣率的一半是物理極限)
    nyquist = FS / 2.0
    
    # --- 開始繪圖 (只畫振幅響應) ---
    # 設定 matplotlib 支援微軟正黑體，解決中文字顯示為方塊的問題
    plt.rcParams['font.sans-serif'] = ['Microsoft JhengHei'] 
    plt.rcParams['axes.unicode_minus'] = False # 解決負號 '-' 顯示為方塊的問題

    fig, ax1 = plt.subplots(figsize=(10, 5))
    fig.suptitle(f'Butterworth Bandpass Filter Response\n(Order: {BUTTER_ORDER}, Fs: {FS} Hz)', fontsize=14, fontweight='bold')
    
    # 1. 振幅響應圖 (Magnitude Response)
    for low, high, label, color in FILTER_BANDS:
        # 轉換為介於 0~1 的正規化頻率 (給 scipy 函數使用)
        low_norm = low / nyquist
        high_norm = high / nyquist

        # 產生 Butterworth 帶通濾波器係數
        b, a = butter(BUTTER_ORDER, [low_norm, high_norm], btype='band')

        # 計算頻率響應
        w, h = freqz(b, a, worN=2000)
        freqs = (w / np.pi) * nyquist
        amplitude_db = 20 * np.log10(np.maximum(np.abs(h), 1e-10))

        ax1.plot(freqs, amplitude_db, color=color, lw=2, label=f'{label} Response')
        ax1.axvline(low, color=color, linestyle='--', alpha=0.7)
        ax1.axvline(high, color=color, linestyle='--', alpha=0.7)

    ax1.set_title('Magnitude Response (振幅頻率響應)')
    ax1.set_xlabel('Frequency (Hz)')
    ax1.set_ylabel('Amplitude (dB)')
    ax1.set_xlim(0, nyquist)    # 畫到奈奎斯特極限(40Hz)
    ax1.set_ylim(-60, 5)        # 限制範圍看清楚主要通帶
    ax1.grid(True, which='both', linestyle='--', alpha=0.7)
    ax1.axhline(-3, color='orange', linestyle='-.', alpha=0.8, label='-3dB Limit') # 工程上定義的能量衰減一半基準線
    ax1.legend(loc='lower right')
    
    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    print("正在計算並繪製濾波器響應圖...")
    plot_filter_response()
