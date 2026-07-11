#!/usr/bin/env python3
"""
MCU Load Cell (HX711) 雙通道資料收集器 (精簡版)
功能：即時顯示 Raw Data (3秒視窗)，支援錄製與 CSV 儲存
"""

import argparse
import os
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox
from collections import deque
from queue import Queue, Empty
from datetime import datetime

import matplotlib
matplotlib.use('TkAgg')
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import numpy as np

try:
    import serial
except ImportError:
    serial = None

# ========== 核心參數 ==========
DISPLAY_WINDOW_SEC = 2         # 繪圖 X 軸顯示跨度，永遠只畫最新的 3 秒資料
UPDATE_INTERVAL_MS = 50        # GUI 畫面重繪間隔 (毫秒)。50ms 約等於 20 FPS
MIN_LOCK_Y_SPAN = 500          # 當啟動「鎖定 Y 軸」時，圖表最少會以均值 ±500 為跨度，以免雜訊放太大
TESTER_OPTIONS = ("三千", "小粉")

class SerialReader(threading.Thread):
    """獨立執行緒：專職在背景不斷讀取 Serial Port，避免在主程式讀取造成畫面卡頓"""
    def __init__(self, port, baud, data_queue, save_queue=None):
        super().__init__(daemon=True)  # 設定為 daemon，主程式關閉時它會自動結束
        self.port = port
        self.baud = baud
        self.data_queue = data_queue   # 傳送給「繪圖區」的資料通訊佇列
        self.save_queue = save_queue   # 傳送給「錄製存檔」的備用通訊佇列
        self._stop_event = threading.Event()
        self.ser = None
        self.hz_counter = 0
        self.last_hz_time = time.time()

    def stop(self):
        self._stop_event.set()
        if self.ser: self.ser.close()

    def run(self):
        if not serial: return
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            time.sleep(1) # 等待 MCU 重啟穩態
            self.ser.reset_input_buffer()
            print(f"Connected to {self.port}")
        except Exception as e:
            print(f"Serial Error: {e}")
            return

        while not self._stop_event.is_set():
            try:
                # 讀取一行資料 (預期收到: "val1\r\n")
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not line: continue
                
                # 將字串以逗號切割，並轉換成浮點數。如果遇到缺漏值則補 0.0
                parts = line.split(',')
                v1 = float(parts[0])

                # 擷取當下的電腦系統時間 (作為圖表的精準時間軸)
                timestamp = time.time()
                
                # 軟體接收速率上限：限制處理速率不超過 200Hz (0.005s)
                # 這是為了防止 MCU 如果發生資料暴衝，導致 Python 的資料佇列堆積過多而當機
                if not hasattr(self, '_last_rx_t'):
                    self._last_rx_t = 0
                if timestamp - self._last_rx_t < 0.005:
                    continue
                self._last_rx_t = timestamp

                # 將時間與 4 個通道的 Raw Data 打包成字典，塞入佇列
                packet = {'t': timestamp, 'v1': v1}
                self.data_queue.put(packet)
                if self.save_queue:
                    self.save_queue.put(packet)
                
                # [除錯用] 計算實際每秒收到幾筆資料 (Hz)，並印在終端機
                self.hz_counter += 1
                now = time.time()
                if now - self.last_hz_time >= 1.0:
                    actual_hz = self.hz_counter / (now - self.last_hz_time)
                    print(f"[DEBUG] Actual RX Hz: {actual_hz:.1f}")
                    self.hz_counter = 0
                    self.last_hz_time = now
            except:
                continue

class App:
    """主程式：負責建立 GUI 監控介面與背景定期重繪波形"""
    def __init__(self, root, port, baud):
        self.root = root
        self.root.title(f"Load Cell Monitor (Port: {port})")
        self.root.geometry("1400x900")

        # 資料結構設定
        self.data_q = Queue()      # 接收來自 Thread 的最新資料
        self.save_q = Queue()      # 錄製時專用的資料佇列
        self.buffer = deque()      # 雙向資料串列：存放目前畫面上的歷史波形點
        
        # 錄製狀態旗標
        self.is_recording = False
        self.record_data = []      # 存放錄製期間的完整歷史庫
        self.record_file_path = None

        # 啟動背景接收執行緒
        self.reader = SerialReader(port, baud, self.data_q, self.save_q)
        self.reader.start()

        # 配置所有視窗按鈕與圖表
        self._setup_ui()
        # 啟動不斷循環畫圖的引擎
        self._update_loop()

    def _setup_ui(self):
        # 1. 設置上方「控制列」框架
        ctrl_frame = ttk.Frame(self.root, padding=5)
        ctrl_frame.pack(side=tk.TOP, fill=tk.X)

        # - 控制列：錄製時間單選按鈕 (10s, 30s, 60s, 300s)
        ttk.Label(ctrl_frame, text="錄製長度:").pack(side=tk.LEFT)
        self.dur_var = tk.IntVar(value=10)
        for d in [10, 30, 60, 300]:
            ttk.Radiobutton(ctrl_frame, text=f"{d}s", variable=self.dur_var, value=d).pack(side=tk.LEFT)

        # - 控制列：測試者選擇
        ttk.Label(ctrl_frame, text="測試者:").pack(side=tk.LEFT, padx=(15, 2))
        self.tester_var = tk.StringVar(value=TESTER_OPTIONS[0])
        self.tester_combo = ttk.Combobox(
            ctrl_frame,
            textvariable=self.tester_var,
            values=TESTER_OPTIONS,
            state="readonly",
            width=8
        )
        self.tester_combo.pack(side=tk.LEFT)
        
        # - 控制列：開始錄製按鈕
        self.btn_rec = ttk.Button(ctrl_frame, text="● 開始錄製", command=self._start_record)
        self.btn_rec.pack(side=tk.LEFT, padx=15)
        self.lbl_record_timer = ttk.Label(ctrl_frame, text="錄製: 0.0 / 0.0s", font=("Consolas", 10))
        self.lbl_record_timer.pack(side=tk.LEFT)

        # - 控制列：分隔線
        ttk.Separator(ctrl_frame, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # - 控制列：是否「鎖定Y軸」的核取方塊與設定值輸入框
        self.lock_y = tk.BooleanVar(value=False)
        cb = ttk.Checkbutton(ctrl_frame, text="鎖定 Y 軸範圍 (±)", variable=self.lock_y)
        cb.pack(side=tk.LEFT)
        self.range_var = tk.StringVar(value="500")
        ttk.Entry(ctrl_frame, textvariable=self.range_var, width=10).pack(side=tk.LEFT, padx=2)

        # - 控制列：當下即時數值的文字標籤 (狀態列)
        self.lbl_status = ttk.Label(ctrl_frame, text="等待數據...", font=("Consolas", 10))
        self.lbl_status.pack(side=tk.RIGHT, padx=10)

        # 2. 設置下方「波形圖表」區塊 (分為 2x2 陣列共 4 張圖)
        plot_frame = ttk.Frame(self.root)
        plot_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.fig = Figure(figsize=(14, 8), dpi=100)
        self.fig.subplots_adjust(left=0.08, right=0.95, top=0.95, bottom=0.08)
        
        # - 註冊四個通道的圖表 (設定標題、單位與預設的線條屬性)
        self.ax1 = self.fig.add_subplot(111)
        self.ax1.set_title("Channel 1", fontweight='bold')
        self.ax1.set_xlabel("Time (s)")
        self.ax1.set_ylabel("Raw Value")
        self.ax1.grid(True, alpha=0.3)
        self.line1, = self.ax1.plot([], [], 'b-', lw=1.2) # 藍色線

        # 將 matplotlib 的畫布綁定到 tkinter 的 UI 元件裡
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # 加入 matplotlib 的預設導覽列 (可以放大、拖曳)
        toolbar = NavigationToolbar2Tk(self.canvas, plot_frame)
        toolbar.update()

    def _update_loop(self):
        """核心繪圖引擎：每 50 毫秒會自動呼叫自己一次"""
        # 1. 趁這一次重繪的時候，將通訊佇列裡累積的最新資料全數取出並存進緩衝區
        while not self.data_q.empty():
            self.buffer.append(self.data_q.get())

        # 2. 清理過期資料：只保留最近 3 秒的波形，實現「波形往左邊滾動」的視覺效果
        if self.buffer:
            latest_t = self.buffer[-1]['t']
            cutoff = latest_t - DISPLAY_WINDOW_SEC
            while self.buffer and self.buffer[0]['t'] < cutoff:
                self.buffer.popleft() # 彈出最左邊的老資料

        # 3. 更新圖表上的線條
        if len(self.buffer) > 2:
            # 將 Queue 取出的東西轉成 Numpy 陣列以加速運算
            ts = np.array([p['t'] for p in self.buffer])
            v1 = np.array([p['v1'] for p in self.buffer])
            
            # 將最後一筆進來的系統時間點當作 "0"，前面的點全部變成負數 (例如 -3.0 ~ 0.0)
            t_rel = ts - ts[-1]

            # 把新資料送進 4 條繪圖線裡
            self.line1.set_data(t_rel, v1)

            # 4. Y 軸範圍縮放邏輯與保護機制
            try:
                span = float(self.range_var.get())
            except:
                span = MIN_LOCK_Y_SPAN
            span = max(MIN_LOCK_Y_SPAN, span)

            for ax, data in [(self.ax1, v1)]:
                # 強制固定 X 軸，永遠呈現最近 3 秒的寬度
                left = min(t_rel) if len(t_rel) > 0 else -DISPLAY_WINDOW_SEC
                ax.set_xlim(left, 0.05)
                
                if self.lock_y.get():
                    # 如果使用者勾選了鎖定：以平均值為中心上下拓展span，如果超出範圍才強迫自動放大
                    mean_val = np.mean(data) if len(data) > 0 else 0
                    if np.isnan(mean_val): mean_val = 0
                    
                    base_low = mean_val - span
                    base_high = mean_val + span
                    if len(data) > 0:
                        dmin = np.min(data)
                        dmax = np.max(data)
                        # 如果有極值衝破了框框，我們就提供上下 15% 的臨時緩衝空間
                        if dmin < base_low or dmax > base_high:
                            pad = max(1.0, (dmax - dmin) * 0.15)
                            ax.set_ylim(dmin - pad, dmax + pad)
                        else:
                            ax.set_ylim(base_low, base_high)
                    else:
                        ax.set_ylim(base_low, base_high)
                else:
                    # 如果使用者不鎖定 (Auto-scale)：隨時根據畫面上的最大最小值，上下各留白 15%
                    if len(data) > 0:
                        dmin = np.min(data)
                        dmax = np.max(data)
                        pad = max(1.0, (dmax - dmin) * 0.15)
                        ax.set_ylim(dmin - pad, dmax + pad)

            # 實際要求 UI 進行渲染
            self.canvas.draw_idle()
            # 同時更新右上角的狀態列，印出四個通道最新的整數值
            self.lbl_status.config(text=f"CH1: {v1[-1]:.0f}")

        # 5. 背景錄製監視邏輯
        if self.is_recording:
            elapsed_sec = time.time() - self.rec_start_time
            self.lbl_record_timer.config(
                text=f"錄製: {elapsed_sec:0.1f} / {self.rec_target_sec:0.1f}s"
            )

            try:
                # 不斷把備用佇列裡的資料移出存進錄製清單中
                while True:
                    self.record_data.append(self.save_q.get_nowait())
            except Empty:
                pass
            
            # 若經過的實際系統時間已經達到指定的秒數，就自動停止，防止感測器斷線導致永遠卡住
            if hasattr(self, 'rec_start_time') and elapsed_sec >= self.rec_target_sec:
                self._stop_record()
        else:
            # 防止記憶體洩漏：沒有在錄製時，清空備用佇列
            with self.save_q.mutex:
                self.save_q.queue.clear()

        # 呼叫自己：向 tkinter 註冊 50 毫秒後再執行這整支函式一次
        self.root.after(UPDATE_INTERVAL_MS, self._update_loop)

    def _sanitize_filename(self, filename):
        """移除 Windows 檔名不允許的字元，保留使用者輸入的主要命名。"""
        filename = filename.strip()
        for ch in '<>:"/\\|?*':
            filename = filename.replace(ch, '_')
        return filename.rstrip(' .')

    def _prepare_record_file_path(self):
        now = datetime.now()
        time_name = now.strftime("%H%M%S")
        tester_name = self._sanitize_filename(self.tester_var.get())
        if not tester_name:
            messagebox.showwarning("測試者無效", "請先選擇測試者。")
            return None

        file_name = f"{time_name}_{self.dur_var.get()}s.csv"

        script_dir = os.path.dirname(os.path.abspath(__file__))
        date_folder = now.strftime("%Y-%m-%d")
        output_dir = os.path.join(script_dir, "csv_output", tester_name, date_folder)
        os.makedirs(output_dir, exist_ok=True)

        file_path = os.path.join(output_dir, file_name)
        if os.path.exists(file_path):
            suffix = 1
            while os.path.exists(file_path):
                file_name = f"{time_name}_{self.dur_var.get()}s_{suffix}.csv"
                file_path = os.path.join(output_dir, file_name)
                suffix += 1

        return file_path

    def _start_record(self):
        """按下錄製按鈕觸發的功能"""
        if self.is_recording: return
        self.record_file_path = self._prepare_record_file_path()
        if not self.record_file_path:
            self.lbl_status.config(text="錄製已取消", foreground="red")
            self.lbl_record_timer.config(text="錄製: 0.0 / 0.0s")
            return

        self.is_recording = True
        self.rec_target_sec = self.dur_var.get()
        self.record_data = [] # 建立乾淨的錄製站
        self.rec_start_time = time.time() # 紀錄系統當下的絕對起點時間
        self.lbl_record_timer.config(text=f"錄製: 0.0 / {self.rec_target_sec:0.1f}s")
        
        # 錄製前把之前累積的舊資料清空，對齊起點
        with self.save_q.mutex:
            self.save_q.queue.clear()
            
        self.btn_rec.config(state=tk.DISABLED, text="錄製中...")
        self.tester_combo.config(state=tk.DISABLED)
        self.lbl_status.config(text=f"Recording {self.rec_target_sec}s...", foreground="red")

    def _stop_record(self):
        """錄製秒數到了之後觸發的功能 (將清單裡的字典轉化為 csv 檔案)"""
        self.is_recording = False
        self.btn_rec.config(state=tk.NORMAL, text="● 開始錄製")
        self.tester_combo.config(state="readonly")
        self.lbl_status.config(text="錄製完成", foreground="green")
        self.lbl_record_timer.config(text=f"錄製: {self.rec_target_sec:0.1f} / {self.rec_target_sec:0.1f}s")

        filename = self.record_file_path
        if not filename:
            self.lbl_status.config(text="沒有指定存檔路徑", foreground="red")
            self.record_data = [] # 釋放資料陣列
            return
        
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                if self.record_data:
                    # 抓取第一筆作為 T=0 出發點
                    t0 = self.record_data[0]['t']
                    # 寫入欄位標籤：時間與四大通道
                    f.write("Time_Rel,CH1_Raw\n")
                    
                    # 將內存陣列印入純文字
                    for p in self.record_data:
                        # 分別對應：相對過過時間 (.4四位小數點), 四個沒加工過的整數值
                        f.write(f"{p['t']-t0:.4f},{p['v1']:.0f}\n")

            messagebox.showinfo("完成", f"已儲存: {filename}")
        except Exception as e:
            messagebox.showerror("存檔失敗", str(e))
        finally:
            self.record_data = [] # 存檔完成或失敗皆徹底釋放記憶體
            self.record_file_path = None

    def on_close(self):
        """關閉程式 (按右上角叉叉) 時的清理功能"""
        self.reader.stop() # 停止後台的 Serial 執行緒，釋放 COM Port
        self.root.destroy()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM10')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()

    root = tk.Tk()
    app = App(root, args.port, args.baud)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
