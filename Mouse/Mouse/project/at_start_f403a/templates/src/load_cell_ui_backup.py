#!/usr/bin/env python3
"""
MCU Load Cell (HX711) 資料收集工具 (帶 GUI)

功能：
1. 即時顯示 Load Cell 原始數值波形
2. 可設定收集時間（1/5/10/60 秒）
3. 點擊「開始收集」按鈕與標記資料
4. 儲存數據以供校正分析 (記錄參考重量)

依賴：
  pip install pyserial matplotlib numpy

使用方法：
  python load_cell_ui.py --port COM10
"""

import argparse
import threading
import time
from collections import deque
from queue import Queue, Empty
from datetime import datetime
import tkinter as tk
from tkinter import ttk, messagebox
import os
import csv

try:
    import serial
except Exception:
    serial = None

import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import numpy as np

# 設定中文字體
plt.rcParams['font.sans-serif'] = ['Microsoft JhengHei', 'SimHei', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False


# ========== 可調參數 ==========
SAMPLING_RATE = 80            # 重要：若要量測小白鼠心跳，MCU 取樣率需設為 ~80Hz (HX711 需改RATE腳)
DISPLAY_WINDOW_SEC = 5       # 顯示時長縮短一點，看細節比較清楚
DISPLAY_BUFFER_SIZE = int(SAMPLING_RATE * DISPLAY_WINDOW_SEC)
GUI_UPDATE_INTERVAL_MS = 100
RESP_LOW_HZ = 0.5             # 呼吸訊號頻率下限
RESP_HIGH_HZ = 5.0            # 呼吸訊號頻率上限


def parse_line(line):
    """解析 MCU 輸出"""
    line = line.strip()
    if not line:
        return None

    try:
        if ',' in line:
            parts = line.split(',')
            val1 = float(parts[0])
            val2 = float(parts[1]) if len(parts) > 1 else 0.0
            return {
                'value': val1,
                'value2': val2,
                'resp_val': 0.0,
                'heart_val': 0.0
            }
        else:
            value = float(line)
            return {
                'value': value,
                'value2': 0.0,
                'resp_val': 0.0,
                'heart_val': 0.0
            }
    except ValueError:
        return None


class SerialReader(threading.Thread):
    def __init__(self, port, baud, data_queue, collect_queue=None):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.data_queue = data_queue
        self.collect_queue = collect_queue
        self._stop_event = threading.Event()
        self.ser = None
        self.sample_counter = 0
        self.start_time = None

    def stop(self):
        self._stop_event.set()
        if self.ser and self.ser.is_open:
            self.ser.close()

    def run(self):
        if serial is None:
            print("錯誤：未安裝 pyserial")
            return

        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            print(f"✓ 成功連接到 {self.port}, baud rate: {self.baud}")
            #有些 MCU 在開啟 Serial 時會重啟，稍作等待
            time.sleep(1) 
            self.ser.reset_input_buffer()
        except Exception as e:
            print(f"✗ 無法開啟串列埠 {self.port}: {e}")
            return

        while not self._stop_event.is_set():
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore')
            except Exception:
                line = ''
            
            if not line:
                continue

            parsed = parse_line(line)
            if parsed:
                # 初始化起始時間
                if self.start_time is None:
                    self.start_time = time.time()
                
                # 計算時間戳
                parsed['timestamp'] = self.start_time + (self.sample_counter / SAMPLING_RATE)
                self.sample_counter += 1
                
                self.data_queue.put(parsed)
                if self.collect_queue is not None:
                    try:
                        self.collect_queue.put_nowait(parsed)
                    except:
                        pass

        if self.ser:
            self.ser.close()


class LoadCellApp:
    def __init__(self, root, port, baud):
        self.root = root
        self.root.title("Load Cell 原始數據分析工具")
        self.root.geometry("1000x700")
        
        self.port = port
        self.baud = baud
        
        self.data_queue = Queue()
        self.collect_queue = Queue(maxsize=10000)
        self.display_buffer = deque(maxlen=DISPLAY_BUFFER_SIZE)
        
        self.is_collecting = False
        self.collect_duration = tk.IntVar(value=5)
        self.collected_data = []
        self.collect_start_time = None
        self.last_item_name = ""
        
        self.serial_reader = SerialReader(port, baud, self.data_queue, self.collect_queue)
        self.serial_reader.start()
        
        self._create_widgets()
        self._update_display()

    def _create_widgets(self):
        # 控制面板
        control_frame = ttk.Frame(self.root, padding="10")
        control_frame.pack(side=tk.TOP, fill=tk.X)
        
        ttk.Label(control_frame, text="收集時長：").pack(side=tk.LEFT, padx=5)
        
        for duration in [1, 5, 10, 60]:
            ttk.Radiobutton(
                control_frame, 
                text=f"{duration}s", 
                variable=self.collect_duration, 
                value=duration
            ).pack(side=tk.LEFT, padx=5)
        
        self.collect_btn = ttk.Button(
            control_frame, 
            text="開始收集", 
            command=self._start_collection,
            style="Accent.TButton"
        )
        self.collect_btn.pack(side=tk.LEFT, padx=20)

        # 鎖定 Y 軸 Checkbox
        self.lock_y_axis = tk.BooleanVar(value=True)
        ttk.Checkbutton(control_frame, text="鎖定顯示範圍", variable=self.lock_y_axis).pack(side=tk.LEFT, padx=10)
        
        # 範圍輸入框
        ttk.Label(control_frame, text="範圍(±):").pack(side=tk.LEFT, padx=2)
        self.range_var = tk.StringVar(value="1000000")
        ttk.Entry(control_frame, textvariable=self.range_var, width=10).pack(side=tk.LEFT, padx=5)
        
        self.status_label = ttk.Label(
            control_frame, 
            text="等待資料...", 
            font=("Arial", 10, "bold"),
            foreground="blue"
        )
        plot_frame = ttk.Frame(self.root)
        plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.fig = Figure(figsize=(12, 6), dpi=100)
        self.fig.subplots_adjust(hspace=0.4, wspace=0.3)
        
        # --- Channel 1 (左欄) ---
        self.ax1 = self.fig.add_subplot(121)
        self.ax1.set_title("CH1 (PA0/PA1): Raw Signal", fontsize=12, fontweight='bold')
        self.ax1.set_ylabel("ADC Value")
        self.ax1.set_xlabel("Time (s)")
        self.ax1.grid(True, alpha=0.3)
        self.ax1.set_ylim(-8388608, 8388607) 
        self.line1, = self.ax1.plot([], [], 'b-', linewidth=1.5, alpha=0.8)

        # --- Channel 2 (右欄) ---
        self.ax1_ch2 = self.fig.add_subplot(122)
        self.ax1_ch2.set_title("CH2 (PA2/PA3): Raw Signal", fontsize=12, fontweight='bold')
        self.ax1_ch2.set_ylabel("ADC Value")
        self.ax1_ch2.set_xlabel("Time (s)")
        self.ax1_ch2.grid(True, alpha=0.3)
        self.line1_ch2, = self.ax1_ch2.plot([], [], 'g-', linewidth=1.5, alpha=0.8)
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # 加入工具列 (Toolbar) 以便手動縮放
        self.toolbar = NavigationToolbar2Tk(self.canvas, plot_frame)
        self.toolbar.update()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # 資訊列
        info_frame = ttk.Frame(self.root, padding="10")
        info_frame.pack(side=tk.BOTTOM, fill=tk.X)
        self.val_label = ttk.Label(info_frame, text="目前數值: ---", font=("Consolas", 12))
        self.val_label.pack(side=tk.LEFT)
        
        ttk.Label(info_frame, text=f" | Port: {self.port} | Rate: ~{SAMPLING_RATE}Hz").pack(side=tk.LEFT)

    def _start_collection(self):
        if self.is_collecting: return
        
        if len(self.display_buffer) == 0:
            messagebox.showerror("錯誤", "未偵測到數據輸入！")
            return
        
        duration = self.collect_duration.get()
        self.is_collecting = True
        self.collected_data = []
        self.collect_start_time = time.time()
        
        self.collect_btn.config(state=tk.DISABLED)
        
        threading.Thread(target=self._collect_data, args=(duration,), daemon=True).start()
        self._update_collection_status(duration, time.time())

    def _update_collection_status(self, total_duration, start_time):
        if not self.is_collecting: return
        
        elapsed = time.time() - start_time
        remaining = max(0, total_duration - elapsed)
        
        self.status_label.config(
            text=f"收集中... {remaining:.1f}s | count: {len(self.collected_data)}",
            foreground="red"
        )
        
        if remaining > 0:
            self.root.after(100, self._update_collection_status, total_duration, start_time)

    def _collect_data(self, duration):
        start_time = time.time()
        temp_data = []
        target_count = duration * SAMPLING_RATE
        
        # 清空 Queue
        with self.collect_queue.mutex:
            self.collect_queue.queue.clear()
            
        while len(temp_data) < target_count:
            try:
                # Timeout 稍微設長一點，因為 HX711 有時會慢
                data = self.collect_queue.get(timeout=1.0)
                temp_data.append(data)
                
                if len(temp_data) % 10 == 0:
                    self.collected_data = temp_data.copy()
            except Empty:
                if time.time() - start_time > duration + 2:
                    break
                continue
                
        self.collected_data = temp_data
        self.is_collecting = False
        self.root.after(0, self._finish_collection)

    def _finish_collection(self):
        self.collect_btn.config(state=tk.NORMAL)
        self.status_label.config(text="收集完成", foreground="green")
        
        if not self.collected_data:
            messagebox.showerror("錯誤", "無數據 collected")
            return
            
        label_info = self._input_labels()
        if not label_info:
            self.status_label.config(text="取消儲存", foreground="gray")
            return
            
        saved_file = self._save_data(label_info)
        if saved_file:
            self._generate_plot(saved_file)
            messagebox.showinfo("完成", f"已儲存至:\n{os.path.basename(saved_file)}")

    def _input_labels(self):
        dialog = tk.Toplevel(self.root)
        dialog.title("輸入測量資訊")
        dialog.geometry("400x300")
        
        # Center
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() - dialog.winfo_width()) // 2
        y = (dialog.winfo_screenheight() - dialog.winfo_height()) // 2
        dialog.geometry(f"+{x}+{y}")
        
        frame = ttk.Frame(dialog, padding=20)
        frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(frame, text="物品/測試名稱:").grid(row=0, column=0, sticky=tk.W, pady=5)
        name_entry = ttk.Entry(frame, width=20)
        name_entry.grid(row=0, column=1, pady=5)
        if self.last_item_name:
            name_entry.insert(0, self.last_item_name)
            
        ttk.Label(frame, text="參考重量 (g/kg):").grid(row=1, column=0, sticky=tk.W, pady=5)
        weight_entry = ttk.Entry(frame, width=20)
        weight_entry.grid(row=1, column=1, pady=5)
        ttk.Label(frame, text="(選填，用於校正)").grid(row=1, column=2, sticky=tk.W, padx=5)
        
        ttk.Label(frame, text="備註:").grid(row=2, column=0, sticky=tk.W, pady=5)
        note_entry = ttk.Entry(frame, width=20)
        note_entry.grid(row=2, column=1, pady=5)
        
        result = {"data": None}
        
        def on_ok():
            name = name_entry.get().strip()
            w_str = weight_entry.get().strip()
            
            if not name:
                name = "Unknown"
            
            self.last_item_name = name
            
            result["data"] = {
                "name": name,
                "ref_weight": w_str if w_str else "N/A",
                "note": note_entry.get().strip()
            }
            dialog.destroy()
            
        ttk.Button(frame, text="儲存", command=on_ok).grid(row=4, column=1, pady=20)
        dialog.wait_window()
        return result["data"]

    def _save_data(self, info):
        date_str = datetime.now().strftime("%Y%m%d")
        base_dir = os.path.join(os.path.dirname(__file__), "collected_loadcell")
        target_dir = os.path.join(base_dir, date_str, info['name'])
        os.makedirs(target_dir, exist_ok=True)
        
        # Filename: Weight_Timestamp.txt
        safe_weight = info['ref_weight'].replace('.', 'p')
        timestamp_str = datetime.now().strftime("%H%M%S")
        filename = f"{safe_weight}g_{timestamp_str}.txt"
        filepath = os.path.join(target_dir, filename)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(f"# Name: {info['name']}\n")
            f.write(f"# Ref Weight: {info['ref_weight']}\n")
            f.write(f"# Note: {info['note']}\n")
            f.write("Time_Rel, Raw_Value\n")
            
            if self.collected_data:
                t0 = self.collected_data[0]['timestamp']
                for d in self.collected_data:
                    f.write(f"{d['timestamp'] - t0:.4f}, {d['value']:.0f}\n")
                    
        return filepath

    def _generate_plot(self, txt_file):
        try:
            times, vals = [], []
            with open(txt_file, 'r', encoding='utf-8') as f:
                for line in f:
                    if line.startswith('#') or 'Time' in line: continue
                    parts = line.split(',')
                    if len(parts) >= 2:
                        times.append(float(parts[0]))
                        vals.append(float(parts[1]))
            
            if not times: return

            fig, ax = plt.subplots(figsize=(10, 6))
            ax.plot(times, vals, '.-')
            ax.set_title(f"Load Cell Raw: {os.path.basename(txt_file)}")
            ax.set_xlabel("Time (s)")
            ax.set_ylabel("Raw Value")
            ax.grid(True)
            
            mean_val = np.mean(vals)
            std_val = np.std(vals)
            stats = f"Mean: {mean_val:.1f}\nStd: {std_val:.1f}\nMin: {min(vals)}\nMax: {max(vals)}"
            ax.text(0.02, 0.98, stats, transform=ax.transAxes, 
                    verticalalignment='top', bbox=dict(facecolor='white', alpha=0.8))
            
            out_img = os.path.splitext(txt_file)[0] + ".png"
            plt.savefig(out_img)
            plt.close(fig)
        except Exception as e:
            print(f"Plot error: {e}")

    def _update_display(self):
        # 從 Queue 取出資料
        while True:
            try:
                data = self.data_queue.get_nowait()
                self.display_buffer.append(data)
            except Empty:
                break
        
        if len(self.display_buffer) > SAMPLING_RATE: 
            times = np.array([d['timestamp'] for d in self.display_buffer])
            raw_vals = np.array([d['value'] for d in self.display_buffer])
            raw_vals_2 = np.array([d.get('value2', 0.0) for d in self.display_buffer])
            
            # 使用目前的 tare_offset (若有做 tare 功能保留，如果被undo了就預設0)
            offset = getattr(self, 'tare_offset', 0)
            raw_vals = raw_vals - offset
            
            # --- 安全檢查: 替換 NaN 或 Inf ---
            raw_vals = np.nan_to_num(raw_vals, nan=0.0, posinf=0.0, neginf=0.0)
            raw_vals_2 = np.nan_to_num(raw_vals_2, nan=0.0, posinf=0.0, neginf=0.0)
            # -------------------------------

            if len(times) > 0:
                t_latest = times[-1]
                t_rel = times - t_latest
                
                # === CH1 Drawing ===
                self.line1.set_data(t_rel, raw_vals)
                self.ax1.set_xlim(min(t_rel), 0.1)
                
                # 智慧定範圍
                if self.lock_y_axis.get():
                    curr_mean = np.mean(raw_vals)
                    if np.isnan(curr_mean) or np.isinf(curr_mean):
                        curr_mean = 0
                    
                    try:
                        fixed_span = float(self.range_var.get())
                    except:
                        fixed_span = 1000000
                        
                    self.ax1.set_ylim(curr_mean - fixed_span, curr_mean + fixed_span)

                # === CH2 Drawing ===
                self.line1_ch2.set_data(t_rel, raw_vals_2)
                self.ax1_ch2.set_xlim(min(t_rel), 0.1)
                
                if self.lock_y_axis.get():
                    curr_mean_2 = np.mean(raw_vals_2)
                    if np.isnan(curr_mean_2) or np.isinf(curr_mean_2): curr_mean_2 = 0
                    
                    try:
                        fixed_span = float(self.range_var.get())
                    except:
                        fixed_span = 1000000

                    self.ax1_ch2.set_ylim(curr_mean_2 - fixed_span, curr_mean_2 + fixed_span)
                
                self.canvas.draw_idle()
                # 更新數值
                self.val_label.config(text=f"CH1: {raw_vals[-1]:.0f} | CH2: {raw_vals_2[-1]:.0f}")
        
        self.root.after(GUI_UPDATE_INTERVAL_MS, self._update_display)

    def on_closing(self):
        self.serial_reader.stop()
        self.root.destroy()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM10')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()
    
    root = tk.Tk()
    app = LoadCellApp(root, args.port, args.baud)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()