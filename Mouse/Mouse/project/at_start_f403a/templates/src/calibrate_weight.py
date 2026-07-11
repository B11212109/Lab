#!/usr/bin/env python3
"""
Load Cell CSV 重量校正工具 (Linear Calibration)
功能：透過讀取「空載 CSV」與「已知重量 CSV」，計算 CH1 的基準點 (Offset) 與比例係數 (Scale Factor)
公式：實際重量 = (Raw值 - Offset) / Scale
"""

import os
import sys
import json
from datetime import datetime
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
import numpy as np

def load_mean_from_csv(csv_path):
    """
    讀取 load_cell_ui.py 輸出的 CSV，去除前後不穩定資料後計算 CH1 平均 Raw Data
    CSV 格式: Time_Rel,CH1_Raw
    """
    try:
        data = np.genfromtxt(csv_path, delimiter=',', skip_header=1)
        if data.ndim == 1:
            data = np.expand_dims(data, axis=0)

        if data.size == 0 or data.shape[1] < 2:
            raise ValueError("CSV 格式需為 Time_Rel,CH1_Raw")

        valid_mask = np.isfinite(data[:, 0]) & np.isfinite(data[:, 1])
        data = data[valid_mask]

        if len(data) == 0:
            raise ValueError("CSV 檔案中沒有找到有效的數字")

        # 【防呆機制】：為了避免拿到剛放上法碼或按下錄製時造成的震動
        # 自動切掉前 10% 與後 10% 的資料，只取中間 80% 最穩定的部分來算平均
        n = len(data)
        start_idx = int(n * 0.1)
        end_idx = int(n * 0.9)
        
        if end_idx <= start_idx:
            stable_data = data # 資料太少，直接全取
        else:
            stable_data = data[start_idx:end_idx]

        return float(np.mean(stable_data[:, 1]))
    except Exception as e:
        raise Exception(f"解析 {os.path.basename(csv_path)} 失敗: {e}")

def main():
    # 建立隱藏的 Tkinter 父視窗供對話框使用
    root = tk.Tk()
    root.withdraw()
    root.attributes('-topmost', True)
    
    print("=== Load Cell 感測器力量校正程式 ===")
    
    # [步驟 1]：取得空載資料 (Zero Offset)
    messagebox.showinfo("步驟 1/3：歸零", "請選擇【空載 (0g)】狀態下錄製的 CSV 檔案做為歸零基準。")
    zero_csv = filedialog.askopenfilename(title="選擇空載 (0g) 的 CSV 檔", filetypes=[("CSV Files", "*.csv")])
    if not zero_csv:
        print("[取消] 未選擇空載檔案，程式結束。")
        sys.exit(0)
        
    try:
        z1 = load_mean_from_csv(zero_csv)
        print(f"[空載基準值] CH1: {z1:.1f}")
    except Exception as e:
        messagebox.showerror("錯誤", str(e))
        sys.exit(1)

    # [步驟 2]：取得負載資料 (Known Weight)
    messagebox.showinfo("步驟 2/3：負載", "請選擇【放上標準砝碼】狀態下錄製的 CSV 檔案。")
    weight_csv = filedialog.askopenfilename(title="選擇已知重量的 CSV 檔", filetypes=[("CSV Files", "*.csv")])
    if not weight_csv:
        print("[取消] 未選擇標準重量檔案，程式結束。")
        sys.exit(0)

    try:
        w1 = load_mean_from_csv(weight_csv)
        print(f"[負載平均值] CH1: {w1:.1f}")
    except Exception as e:
        messagebox.showerror("錯誤", str(e))
        sys.exit(1)
        
    # [步驟 3]：詢問使用者剛才放的標準砝碼重量
    target_weight = simpledialog.askfloat(
        "步驟 3/3：重量設定", 
        "請輸入剛才使用的「標準砝碼」重量大小 (單純填入數字)\n\n"
        "※ 以後軟體算出來的重量單位，就會是您現在預設的單位(例如您輸入100代表公克，日後單位便是公克)", 
        minvalue=0.001
    )
    
    if not target_weight:
        print("[取消] 未輸入重量數值，程式結束。")
        sys.exit(0)
        
    # [計算]：計算每個通道的比例係數
    # Weight = (Raw - Offset) / Scale
    # => Scale(count/g) = (Raw_Load - Offset) / Weight
    # => g/count = 1 / Scale
    def calc_channel_params(raw_load, offset, target):
        diff = raw_load - offset
        if abs(diff) < 0.1:  # 幾乎無反應，避免不穩定除法
            return {
                "raw_diff": float(diff),
                "scale_count_per_g": 1.0,
                "gram_per_count": None
            }
        scale = diff / target
        return {
            "raw_diff": float(diff),
            "scale_count_per_g": float(scale),
            "gram_per_count": float(1.0 / scale)
        }

    p1 = calc_channel_params(w1, z1, target_weight)
    
    # 建立要輸出的字典檔
    calib_data = {
        "weight_reference_g": float(target_weight),
        "calibration_date": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "formula": "weight_g = (raw - no_load_offset) / scale_count_per_g",
        "csv_format": "Time_Rel,CH1_Raw",
        "CH1": {
            "no_load_offset": float(z1),
            "loaded_raw_mean": float(w1),
            "raw_diff": p1["raw_diff"],
            "scale_count_per_g": p1["scale_count_per_g"],
            "gram_per_count": p1["gram_per_count"]
        }
    }
    
    # 將校正檔儲存在空載 CSV 所在的同一個資料夾底下
    out_dir = os.path.dirname(zero_csv)
    out_path = os.path.join(out_dir, "calibration.json")
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(calib_data, f, indent=4, ensure_ascii=False)
        
    # 在終端機與彈出視窗顯示結果
    def fmt_g_per_count(v):
        return "N/A" if v is None else f"{v:.8f}"

    result_msg = (
        f"校正完成！\n\n參數已成功儲存至:\n{out_path}\n\n"
        f"【CH1 校正參數】(單位: g)\n"
        f"CH1: 無載Offset={z1:.1f}, 有載平均={w1:.1f}, Scale={p1['scale_count_per_g']:.6f} count/g, 1count={fmt_g_per_count(p1['gram_per_count'])} g\n"
        f"\n【應用方式說明】\n"
        f"重量(g) = (CH1_Raw - 無載Offset) / Scale(count/g)\n"
    )
    print("\n" + result_msg)
    messagebox.showinfo("校正成功", result_msg)
    root.destroy() # 強制清空所有殘留視窗資源

if __name__ == '__main__':
    main()
