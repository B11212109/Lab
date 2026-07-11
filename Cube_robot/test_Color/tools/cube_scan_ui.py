#!/usr/bin/env python3
"""
逐色塊掃描介面（兩顆魔方）

功能：
1. 依照預設色面順序，逐塊提示掃描。
2. 每一塊都必須輸入「好」(或 ok/yes/y) 才會進入下一塊。
3. 支援掃描兩顆魔方（可用參數調整）。
4. 可選擇連接序列埠，顯示最近一筆感測器輸出。
5. 掃描完成後自動輸出 JSON 紀錄檔。

執行範例：
python cube_scan_ui.py
python cube_scan_ui.py --cubes 2 --output scan_result.json
python cube_scan_ui.py --port COM5 --baud 115200
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover
    serial = None

try:
    import tkinter as tk
except Exception:  # pragma: no cover
    tk = None


CONFIRM_WORDS = {"好", "ok", "yes", "y", "1"}
QUIT_WORDS = {"q", "quit", "exit"}
BACK_WORDS = {"b", "back", "上一個", "上一步"}

# 你可以按習慣修改順序。預設是常見的 U R F D L B 對應色。
SCAN_PLAN: List[Tuple[str, str]] = [
    ("白色", "D"),
    ("紅色", "R"),
    ("綠色", "B"),
    ("黃色", "U"),
    ("橘色", "L"),
    ("藍色", "F"),
]

# 每面實際只需掃描 8 格，中心格由程式自動補入。
FACE_BLOCK_ORDER = [1, 2, 3, 4, 6, 7, 8, 9]

# 統一 UART 封包格式（MCU 與 Python 共用）：
#   CS1,SEQ=12,SENSOR=0,ST=0,R=123,G=456,B=789,H=12,SAT=34,V=56
# 欄位名稱大小寫不拘，分隔符可用逗號或空白。
UART_KV_PATTERN = re.compile(r"(?P<key>[A-Za-z_]+)\s*=\s*(?P<value>[^,\s]+)")
SENSOR_PARSE_PATTERN = re.compile(
    r"CS1,SEQ=(?P<seq>\d+),SENSOR=(?P<sensor>\d+),ST=(?P<status>-?\d+),"
    r"R=(?P<r>\d+),G=(?P<g>\d+),B=(?P<b>\d+),H=(?P<h>-?\d+),SAT=(?P<s>-?\d+),V=(?P<v>-?\d+)"
)

SENSOR_SEGMENT_PATTERN = re.compile(
    r"CS1,SEQ=(?P<seq>\d+),SENSOR=(?P<sensor>\d+),ST=(?P<status>-?\d+),"
    r"R=(?P<r>\d+),G=(?P<g>\d+),B=(?P<b>\d+),H=(?P<h>-?\d+),SAT=(?P<s>-?\d+),V=(?P<v>-?\d+)"
)

NET_FACE_ANCHOR = {
    "U": (1, 0),
    "L": (0, 1),
    "F": (1, 1),
    "R": (2, 1),
    "B": (3, 1),
    "D": (1, 2),
}

RUBIK_PALETTE = {
    "white": (242, 242, 238),
    "red": (196, 40, 27),
    "green": (33, 155, 72),
    "yellow": (255, 214, 0),
    "orange": (255, 108, 0),
    "blue": (0, 70, 173),
}

TARGET_FACE_TO_PALETTE = {
    "白色": "white",
    "紅色": "red",
    "綠色": "green",
    "黃色": "yellow",
    "橘色": "orange",
    "藍色": "blue",
}

FACE_CODE_TO_TARGET_FACE = {face_code: face_name for face_name, face_code in SCAN_PLAN}

UI_THEME = {
    "bg": "#111318",
    "panel": "#171b22",
    "panel_alt": "#1d232c",
    "panel_edge": "#2e3947",
    "text": "#e8edf4",
    "muted": "#9aa7b7",
    "accent": "#61dafb",
    "accent2": "#7ef0a6",
    "good": "#1f8a3a",
    "warn": "#c28b2c",
    "bad": "#8f2f2f",
}

class CubeNetViewer:
    def __init__(self, enable_gui: bool = True) -> None:
        self.enable_gui = enable_gui and tk is not None
        self.root = None
        self.canvas = None
        self.exit_button = None
        self.cell_items: Dict[Tuple[str, int], int] = {}
        self.status_item = None
        self.prompt_item = None
        self.measure_item = None
        self.log_item = None
        self.log_lines: List[str] = []
        self.pending_action: Optional[str] = None
        self.bg_color = UI_THEME["bg"]
        self.empty_color = "#3a404a"

        if not self.enable_gui:
            return

        try:
            self.root = tk.Tk()
            self.root.title("魔方掃描校正工具")
            self.root.geometry("1320x760")
            self.root.minsize(1180, 700)
            self.canvas = tk.Canvas(self.root, width=1320, height=700, bg=self.bg_color, highlightthickness=0)
            self.canvas.pack(fill="both", expand=True)
            self.exit_button = tk.Button(
                self.root,
                text="結束",
                width=14,
                bg=UI_THEME["bad"],
                fg="#ffffff",
                activebackground="#a33a3a",
                activeforeground="#ffffff",
                relief="flat",
                command=lambda: self.set_action("quit"),
            )
            self.canvas.bind("<Button-1>", lambda _e: self.set_action("confirm"))
            self.canvas.bind("<Button-3>", lambda _e: self.set_action("back"))
            self.root.protocol("WM_DELETE_WINDOW", lambda: self.set_action("quit"))
            self._draw_grid()
            self._pump()
        except Exception:
            self.enable_gui = False
            self.root = None
            self.canvas = None

    def _draw_grid(self) -> None:
        if self.canvas is None:
            return

        self.canvas.delete("all")
        self.canvas.create_rectangle(0, 0, 1320, 700, fill=self.bg_color, outline=self.bg_color)
        self.canvas.create_rectangle(18, 14, 1298, 106, fill=UI_THEME["panel"], outline=UI_THEME["panel_edge"], width=2)
        self.status_item = self.canvas.create_text(
            34,
            28,
            anchor="w",
            fill=UI_THEME["accent2"],
            font=("Segoe UI", 16, "bold"),
            text="顏色掃描器",
        )
        self.prompt_item = self.canvas.create_text(
            34,
            60,
            anchor="w",
            fill=UI_THEME["text"],
            font=("Segoe UI", 12, "bold"),
            text="等待掃描...",
        )
        self.measure_item = self.canvas.create_text(
            34,
            86,
            anchor="w",
            fill=UI_THEME["accent"],
            font=("Segoe UI", 11),
            text="UART: 等待資料...",
        )

        self.canvas.create_text(
            1260,
            30,
            anchor="ne",
            fill=UI_THEME["muted"],
            font=("Segoe UI", 10),
            text="左鍵下一格 / 右鍵上一格",
        )

        tile = 38
        gap = 5
        face_gap = 24
        origin_x = 90
        origin_y = 160

        for face_code, (fx, fy) in NET_FACE_ANCHOR.items():
            face_x = origin_x + fx * (3 * tile + 2 * gap + face_gap)
            face_y = origin_y + fy * (3 * tile + 2 * gap + face_gap)

            self.canvas.create_text(
                face_x + (3 * tile + 2 * gap) // 2,
                face_y - 18,
                fill=UI_THEME["text"],
                font=("Segoe UI", 12, "bold"),
                text=face_code,
            )

            for r in range(3):
                for c in range(3):
                    idx = r * 3 + c + 1
                    x0 = face_x + c * (tile + gap)
                    y0 = face_y + r * (tile + gap)
                    x1 = x0 + tile
                    y1 = y0 + tile
                    rect = self.canvas.create_rectangle(
                        x0,
                        y0,
                        x1,
                        y1,
                        fill=self.empty_color,
                        outline="#202632",
                        width=1,
                    )
                    self.cell_items[(face_code, idx)] = rect

        self._paint_static_centers()

        self.canvas.create_rectangle(820, 152, 1270, 510, fill=UI_THEME["panel_alt"], outline=UI_THEME["panel_edge"], width=2)
        self.canvas.create_text(
            838,
            170,
            anchor="w",
            fill=UI_THEME["accent2"],
            font=("Segoe UI", 12, "bold"),
            text="掃描訊息",
        )
        self.log_item = self.canvas.create_text(
            838,
            198,
            anchor="nw",
            fill="#d3d8df",
            font=("Consolas", 10),
            text="",
        )

        if self.exit_button is not None:
            self.canvas.create_window(1045, 586, window=self.exit_button)

    def _paint_static_centers(self) -> None:
        for face_name, face_code in SCAN_PLAN:
            color_hex = palette_hex(TARGET_FACE_TO_PALETTE.get(face_name, "white"))
            self.set_block_color(face_code, 5, color_hex)

    def _pump(self) -> None:
        if not self.enable_gui or self.root is None:
            return
        try:
            self.root.update_idletasks()
            self.root.update()
        except Exception:
            self.enable_gui = False

    def update_status(
        self,
        cube_index: int,
        cubes: int,
        face_index: int,
        face_total: int,
        face_name: str,
        face_code: str,
        block_index: int,
    ) -> None:
        if not self.enable_gui or self.canvas is None or self.prompt_item is None:
            return
        text = f"{face_name} 第 {block_index}/8 格"
        self.canvas.itemconfigure(self.prompt_item, text=text)
        self._pump()

    def update_measurement_text(self, text: str) -> None:
        if not self.enable_gui or self.canvas is None or self.measure_item is None:
            return
        self.canvas.itemconfigure(self.measure_item, text=text)
        self._pump()

    def append_log(self, text: str) -> None:
        if not self.enable_gui or self.canvas is None or self.log_item is None:
            return
        self.log_lines.append(text)
        if len(self.log_lines) > 16:
            self.log_lines = self.log_lines[-16:]
        display_lines: List[str] = []
        for line in self.log_lines:
            display_lines.append(line)
            if "#" in line:
                display_lines.append("")
        self.canvas.itemconfigure(self.log_item, text="\n".join(display_lines))
        self._pump()

    def set_block_color(self, face_code: str, block_index: int, color_hex: str) -> None:
        if not self.enable_gui or self.canvas is None:
            return
        cell = self.cell_items.get((face_code, block_index))
        if cell is None:
            return
        self.canvas.itemconfigure(cell, fill=color_hex)
        self._pump()

    def reset_cube(self) -> None:
        if not self.enable_gui or self.canvas is None:
            return
        for item_id in self.cell_items.values():
            self.canvas.itemconfigure(item_id, fill=self.empty_color)
        self._paint_static_centers()
        self._pump()

    def redraw_by_records(self, records: List[StepRecord], cube_index: int) -> None:
        self.reset_cube()
        for rec in records:
            if rec.cube_index != cube_index:
                continue
            self.set_block_color(
                rec.target_face_code,
                rec.block_index,
                palette_hex(TARGET_FACE_TO_PALETTE.get(rec.target_face_name, "white")),
            )
        self._pump()

    def set_action(self, action: str) -> None:
        self.pending_action = action

    def wait_action(self) -> str:
        if not self.enable_gui:
            return "confirm"
        while True:
            self._pump()
            if self.pending_action:
                action = self.pending_action
                self.pending_action = None
                return action
            time.sleep(0.02)

    def close(self) -> None:
        if not self.enable_gui or self.root is None:
            return
        try:
            self.root.destroy()
        except Exception:
            pass


def measured_rgb_to_hex(r: Optional[int], g: Optional[int], b: Optional[int]) -> str:
    if r is None or g is None or b is None:
        return "#7f8c8d"
    mx = max(r, g, b)
    if mx <= 0:
        return "#7f8c8d"
    rr = int((r * 255) / mx)
    gg = int((g * 255) / mx)
    bb = int((b * 255) / mx)
    return f"#{rr:02x}{gg:02x}{bb:02x}"


def _norm_vec(r: int, g: int, b: int) -> Tuple[float, float, float]:
    mag = (r * r + g * g + b * b) ** 0.5
    if mag <= 0.0:
        return (0.0, 0.0, 0.0)
    return (r / mag, g / mag, b / mag)


def palette_hex(name: str) -> str:
    rgb = RUBIK_PALETTE.get(name, (127, 140, 157))
    return f"#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}"


def measured_to_palette_hex(r: Optional[int], g: Optional[int], b: Optional[int], target_face_name: str) -> str:
    if r is None or g is None or b is None:
        return palette_hex(TARGET_FACE_TO_PALETTE.get(target_face_name, "white"))

    mv = _norm_vec(r, g, b)
    best_name = "white"
    best_score = -1.0
    for name, prgb in RUBIK_PALETTE.items():
        pv = _norm_vec(prgb[0], prgb[1], prgb[2])
        score = mv[0] * pv[0] + mv[1] * pv[1] + mv[2] * pv[2]
        if score > best_score:
            best_score = score
            best_name = name

    return palette_hex(best_name)


def parse_segment_frames(line: str) -> Dict[int, UartFrame]:
    frames: Dict[int, UartFrame] = {}
    if not line:
        return frames

    for m in SENSOR_SEGMENT_PATTERN.finditer(line):
        sensor_index = int(m.group("sensor"))
        frames[sensor_index] = UartFrame(
            raw_line=line,
            sensor_index=sensor_index,
            sensor_status=int(m.group("status")),
            is_predicted_center=False,
            measured_r=int(m.group("r")),
            measured_g=int(m.group("g")),
            measured_b=int(m.group("b")),
            measured_h=int(m.group("h")),
            measured_s=int(m.group("s")),
            measured_v=int(m.group("v")),
            parsed_face_id=int(m.group("face_id")),
            parsed_face_text=m.group("face_text").strip(),
            cube_index=None,
            timestamp=now_str(),
        )

    return frames


@dataclass
class StepRecord:
    cube_index: int
    target_face_name: str
    target_face_code: str
    block_index: int
    is_predicted_center: bool
    selected_sensor_id: int
    operator_confirm: str
    sensor_line: str
    parsed_face_id: Optional[int]
    parsed_face_text: Optional[str]
    sensor_index: Optional[int]
    sensor_status: Optional[int]
    measured_r: Optional[int]
    measured_g: Optional[int]
    measured_b: Optional[int]
    measured_h: Optional[int]
    measured_s: Optional[int]
    measured_v: Optional[int]
    ratio_g_over_r_x1000: Optional[int]
    ratio_b_over_r_x1000: Optional[int]
    timestamp: str


@dataclass
class UartFrame:
    raw_line: str
    sensor_index: Optional[int]
    sensor_status: Optional[int]
    is_predicted_center: bool
    measured_r: Optional[int]
    measured_g: Optional[int]
    measured_b: Optional[int]
    measured_h: Optional[int]
    measured_s: Optional[int]
    measured_v: Optional[int]
    parsed_face_id: Optional[int]
    parsed_face_text: Optional[str]
    cube_index: Optional[int]
    timestamp: str


def now_str() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())


def parse_sensor_fields(line: str) -> Dict[str, Optional[int]]:
    m = SENSOR_PARSE_PATTERN.search(line)
    if not m:
        return {
            "seq": None,
            "sensor_index": None,
            "sensor_status": None,
            "measured_r": None,
            "measured_g": None,
            "measured_b": None,
            "measured_h": None,
            "measured_s": None,
            "measured_v": None,
        }

    return {
        "seq": int(m.group("seq")),
        "sensor_index": int(m.group("sensor")),
        "sensor_status": int(m.group("status")),
        "measured_r": int(m.group("r")),
        "measured_g": int(m.group("g")),
        "measured_b": int(m.group("b")),
        "measured_h": int(m.group("h")),
        "measured_s": int(m.group("s")),
        "measured_v": int(m.group("v")),
    }


def parse_uart_kv_fields(line: str) -> Dict[str, Optional[int]]:
    pairs = {m.group("key").strip().lower(): m.group("value").strip() for m in UART_KV_PATTERN.finditer(line)}
    if not pairs:
        return {
            "seq": None,
            "sensor_index": None,
            "sensor_status": None,
            "measured_r": None,
            "measured_g": None,
            "measured_b": None,
            "measured_h": None,
            "measured_s": None,
            "measured_v": None,
        }

    def get_int(*names: str) -> Optional[int]:
        for name in names:
            if name in pairs:
                try:
                    return int(pairs[name])
                except ValueError:
                    pass
        return None

    return {
        "seq": get_int("seq"),
        "sensor_index": get_int("sensor", "s", "sensor_index"),
        "sensor_status": get_int("st", "status"),
        "measured_r": get_int("r", "red"),
        "measured_g": get_int("g", "green"),
        "measured_b": get_int("b", "blue"),
        "measured_h": get_int("h", "hue"),
        "measured_s": get_int("sat", "s", "saturation"),
        "measured_v": get_int("v", "value"),
    }


def safe_ratio_x1000(num: Optional[int], den: Optional[int]) -> Optional[int]:
    if num is None or den is None or den <= 0:
        return None
    return int((num * 1000) / den)


def build_uart_frame(line: str) -> UartFrame:
    fields = parse_sensor_fields(line)
    if fields["measured_r"] is None:
        kv_fields = parse_uart_kv_fields(line)
        for key, value in kv_fields.items():
            if key in fields and fields[key] is None:
                fields[key] = value
    
    if fields["measured_r"] is None or fields["sensor_index"] is None:
        print(f"[PARSE] 無法解析 CS1: {line[:70]}")

    frame = UartFrame(
        raw_line=line,
        sensor_index=fields["sensor_index"],
        sensor_status=fields["sensor_status"],
        is_predicted_center=False,
        measured_r=fields["measured_r"],
        measured_g=fields["measured_g"],
        measured_b=fields["measured_b"],
        measured_h=fields["measured_h"],
        measured_s=fields["measured_s"],
        measured_v=fields["measured_v"],
        parsed_face_id=None,
        parsed_face_text=None,
        cube_index=None,
        timestamp=now_str(),
    )
    
    return frame


def calc_stats(values: List[int]) -> Dict[str, float]:
    return {
        "min": float(min(values)),
        "max": float(max(values)),
        "avg": round(sum(values) / len(values), 3),
    }


def rgb_to_record_hsv(r: int, g: int, b: int) -> Tuple[int, int, int]:
    mx = max(r, g, b)
    mn = min(r, g, b)
    diff = mx - mn

    if diff == 0:
        h = 0
    elif mx == r:
        h = int((g - b) * 60 / diff)
        if h < 0:
            h += 360
    elif mx == g:
        h = int((b - r) * 60 / diff + 120)
    else:
        h = int((r - g) * 60 / diff + 240)

    if mx == 0:
        s = 0
    else:
        s = int(255 * diff / mx)

    v = mx
    return h, s, v


def build_predicted_center_frame(face_name: str, face_code: str, sensor_id: int) -> UartFrame:
    palette_name = TARGET_FACE_TO_PALETTE.get(face_name, "white")
    r, g, b = RUBIK_PALETTE.get(palette_name, RUBIK_PALETTE["white"])
    h, s, v = rgb_to_record_hsv(r, g, b)
    return UartFrame(
        raw_line=f"[PREDICTED_CENTER] {face_name}({face_code})",
        sensor_index=sensor_id,
        sensor_status=-1,
        is_predicted_center=True,
        measured_r=r,
        measured_g=g,
        measured_b=b,
        measured_h=h,
        measured_s=s,
        measured_v=v,
        parsed_face_id=None,
        parsed_face_text=None,
        cube_index=None,
        timestamp=now_str(),
    )


def build_threshold_summary(records: List[StepRecord]) -> Dict[str, object]:
    by_color: Dict[str, List[StepRecord]] = {}
    for rec in records:
        if rec.is_predicted_center:
            continue
        by_color.setdefault(rec.target_face_name, []).append(rec)

    summary: Dict[str, object] = {}
    for color_name, color_records in by_color.items():
        metrics: Dict[str, List[int]] = {
            "r": [],
            "g": [],
            "b": [],
            "h": [],
            "s": [],
            "v": [],
            "gr_x1000": [],
            "br_x1000": [],
        }

        for rec in color_records:
            if rec.measured_r is not None:
                metrics["r"].append(rec.measured_r)
            if rec.measured_g is not None:
                metrics["g"].append(rec.measured_g)
            if rec.measured_b is not None:
                metrics["b"].append(rec.measured_b)
            if rec.measured_h is not None:
                metrics["h"].append(rec.measured_h)
            if rec.measured_s is not None:
                metrics["s"].append(rec.measured_s)
            if rec.measured_v is not None:
                metrics["v"].append(rec.measured_v)
            if rec.ratio_g_over_r_x1000 is not None:
                metrics["gr_x1000"].append(rec.ratio_g_over_r_x1000)
            if rec.ratio_b_over_r_x1000 is not None:
                metrics["br_x1000"].append(rec.ratio_b_over_r_x1000)

        metric_summary: Dict[str, object] = {}
        for key, vals in metrics.items():
            if vals:
                metric_summary[key] = calc_stats(vals)

        summary[color_name] = {
            "count": len(color_records),
            "parsed_count": len(metrics["r"]),
            "metrics": metric_summary,
        }

    return summary


def write_records_csv(csv_path: Path, records: List[StepRecord]) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", encoding="utf-8", newline="") as fp:
        writer = csv.writer(fp)
        writer.writerow(
            [
                "timestamp",
                "cube_index",
                "target_face_name",
                "target_face_code",
                "block_index",
                "is_predicted_center",
                "selected_sensor_id",
                "sensor_index",
                "sensor_status",
                "measured_r",
                "measured_g",
                "measured_b",
                "measured_h",
                "measured_s",
                "measured_v",
                "ratio_g_over_r_x1000",
                "ratio_b_over_r_x1000",
            ]
        )
        for rec in records:
            if rec.is_predicted_center:
                continue
            writer.writerow(
                [
                    rec.timestamp,
                    rec.cube_index,
                    rec.target_face_name,
                    rec.target_face_code,
                    rec.block_index,
                    rec.is_predicted_center,
                    rec.selected_sensor_id,
                    rec.sensor_index,
                    rec.sensor_status,
                    rec.measured_r,
                    rec.measured_g,
                    rec.measured_b,
                    rec.measured_h,
                    rec.measured_s,
                    rec.measured_v,
                    rec.ratio_g_over_r_x1000,
                    rec.ratio_b_over_r_x1000,
                ]
            )


def list_available_serial_ports() -> List[str]:
    if serial is None:
        return []

    try:
        from serial.tools import list_ports  # type: ignore
    except Exception:
        return []

    return [port.device for port in list_ports.comports()]


def read_latest_sensor_line(ser: Optional[Any], wait_s: float = 0.4) -> str:
    if ser is None:
        return ""

    deadline = time.time() + wait_s
    latest = ""
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode(errors="ignore").strip()
        if "CS1," in text:
            latest = text
    return latest


class UartReader:
    def __init__(self, ser: Optional[Any]) -> None:
        self.ser = ser
        self.latest_frame: Optional[UartFrame] = None
        self.latest_line: str = ""
        self.latest_by_sensor: Dict[int, UartFrame] = {}
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        if self.ser is None:
            print("[UART] 序列埠為空，後台線程不啟動")
            return
        if self._thread is not None:
            print("[UART] 後台線程已啟動過")
            return

        print("[UART] 正在啟動後台線程...")
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        print("[UART] 後台線程已啟動（daemon=True）")

    def _run(self) -> None:
        if self.ser is None:
            return

        print("[UART] 後台線程啟動，開始監聽序列埠...")
        line_count = 0
        data_count = 0
        while not self._stop.is_set():
            try:
                raw = self.ser.readline()
                if not raw:
                    continue
                line = raw.decode(errors="ignore").strip()
                if not line:
                    continue
                
                line_count += 1
                with self._lock:
                    self.latest_line = line

                # 檢查是否是 CS1 格式的關鍵行
                if "CS1," in line:
                    data_count += 1
                    print(f"[UART] ✓ #{data_count} 收到 CS1 行: {line[:85]}")
                    
                    # 嘗試解析
                    frame = build_uart_frame(line)
                    if frame.sensor_index is not None:
                        print(f"[UART]   → 解析成功: S{frame.sensor_index} RGB({frame.measured_r},{frame.measured_g},{frame.measured_b})")
                        with self._lock:
                            self.latest_by_sensor[frame.sensor_index] = frame
                            self.latest_frame = frame
                            print(f"[UART]   → 已緩存到 latest_by_sensor[{frame.sensor_index}]")
                    else:
                        print(f"[UART]   ✗ 解析失敗: sensor_index=None")
                        # 即使 sensor_index 不對也嘗試儲存一份
                        with self._lock:
                            self.latest_frame = frame
                            print(f"[UART]   → 存為 latest_frame 備用")
                    continue

                # 處理舊格式行
                segment_frames = parse_segment_frames(line)
                if segment_frames:
                    with self._lock:
                        for sid, frame in segment_frames.items():
                            self.latest_by_sensor[sid] = frame
                        self.latest_frame = next(iter(segment_frames.values()))
                    continue

            except Exception as e:
                print(f"[UART] 例外: {e}")
                import traceback
                traceback.print_exc()
                break

    def get_latest_line(self) -> str:
        with self._lock:
            return self.latest_line

    def get_latest_frame(self, sensor_id: Optional[int] = None) -> Optional[UartFrame]:
        with self._lock:
            if sensor_id is not None:
                # 優先查特定感測器的緩存
                frame = self.latest_by_sensor.get(sensor_id)
                if frame is not None:
                    return frame
                # 備用：檢查通用幀是否符合
                if self.latest_frame is not None and self.latest_frame.sensor_index == sensor_id:
                    return self.latest_frame
                return None
            return self.latest_frame

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=0.5)


def ask_confirm(prompt: str) -> str:
    while True:
        ans = input(prompt).strip()
        if ans:
            return ans


def frame_to_text(frame: Optional[UartFrame], sensor_id: int) -> str:
    if frame is None:
        return f"⧇ S{sensor_id} 尋找 UART..."
    if frame.is_predicted_center:
        status_icon = "◎"
    else:
        status_icon = "✓" if frame.sensor_status == 0 else "✗"
    return (
        f"{status_icon} S{sensor_id} | "
        f"RGB({frame.measured_r},{frame.measured_g},{frame.measured_b}) | "
        f"HSV({frame.measured_h},{frame.measured_s},{frame.measured_v}) | "
        f"{measured_rgb_to_hex(frame.measured_r, frame.measured_g, frame.measured_b)}"
    )


def wait_sensor_frame(uart_reader: UartReader, sensor_id: int, timeout_s: float) -> Optional[UartFrame]:
    """等待接收指定感測器的數據"""
    deadline = time.time() + timeout_s
    attempts = 0
    last_line = ""
    
    while time.time() < deadline:
        frame = uart_reader.get_latest_frame(sensor_id)
        if frame is not None:
            print(f"[WAIT] ✓ 獲得 S{sensor_id}: RGB({frame.measured_r},{frame.measured_g},{frame.measured_b})")
            return frame
        
        # 也檢查通用 latest_frame
        latest_frame = uart_reader.get_latest_frame(None)
        if latest_frame is not None and latest_frame.sensor_index == sensor_id:
            print(f"[WAIT] ✓ 從通用幀獲得 S{sensor_id}")
            return latest_frame
        
        time.sleep(0.1)
        attempts += 1
        if attempts % 3 == 0:
            curr_line = uart_reader.get_latest_line()
            if curr_line != last_line:
                print(f"[WAIT] 收到行: {curr_line[:70]}...")
                last_line = curr_line
            elapsed = time.time() - (deadline - timeout_s)
            print(f"[WAIT] 等待中... ({elapsed:.1f}s/{timeout_s}s)")
    
    print(f"[WAIT] ✗ 超時，無 S{sensor_id} 數據")
    return None


def run_scan(
    cubes: int,
    output: Path,
    ser: Optional[Any],
    enable_gui: bool,
    sensor_id: int,
) -> int:
    all_records: List[StepRecord] = []
    total_steps = cubes * len(SCAN_PLAN) * len(FACE_BLOCK_ORDER)
    total_records = cubes * len(SCAN_PLAN) * 9
    finished_steps = 0
    interrupted = False
    viewer = CubeNetViewer(enable_gui=enable_gui)
    uart_reader = UartReader(ser)
    uart_reader.start()

    if enable_gui and not viewer.enable_gui:
        print("警告: GUI 初始化失敗，改為純文字模式。")

    print("=" * 64)
    print("掃描開始")
    print(f"- 魔方數量: {cubes}")
    print(f"- 每顆: {len(SCAN_PLAN)} 個色面，每面 8 格實測 + 1 格中心補色")
    print(f"- 校正感測器: S{sensor_id}")
    print(f"- 總步驟: {total_steps}")
    print(f"- 最終紀錄筆數: {total_records}")
    print("- GUI 指令: 好/上一格/結束")
    print("- 若使用 no-gui: 輸入『好』前進，輸入 b 回上一塊，輸入 q 離開")
    print("=" * 64)

    cube_i = 1
    face_i = 0
    face_step_i = 0

    while cube_i <= cubes:
        face_name, face_code = SCAN_PLAN[face_i]
        block_i = FACE_BLOCK_ORDER[face_step_i]
        viewer.update_status(cube_i, cubes, face_i + 1, len(SCAN_PLAN), face_name, face_code, block_i)
        viewer.redraw_by_records(all_records, cube_i)

        msg1 = f"第 {cube_i}/{cubes} 顆 | 色面 {face_i + 1}/{len(SCAN_PLAN)}: {face_name} ({face_code})"
        msg2 = f"請對準第 {face_step_i + 1}/8 格（位序 {block_i}）"
        viewer.append_log(msg1)
        viewer.append_log(msg2)

        if not viewer.enable_gui:
            print("\n" + "-" * 64)
            print(msg1)
            print(msg2)
            print("中心格將自動補色，不需感測器讀值")

        live_frame = None
        sensor_line = uart_reader.get_latest_line() or read_latest_sensor_line(ser, wait_s=0.50)
        live_frame = uart_reader.get_latest_frame(sensor_id)
        
        if live_frame is None and sensor_line:
            temp_frame = build_uart_frame(sensor_line)
            if temp_frame.sensor_index == sensor_id or temp_frame.measured_r is not None:
                live_frame = temp_frame
        
        viewer.update_measurement_text(frame_to_text(live_frame, sensor_id))
        if live_frame is not None:
            m = frame_to_text(live_frame, sensor_id)
            viewer.append_log(m)
            if not viewer.enable_gui:
                print(m)
        else:
            m = f"S{sensor_id} 最新量測: (無 / 尚未收到 UART)"
            viewer.append_log(m)
            if not viewer.enable_gui:
                print(m)

        if viewer.enable_gui:
            try:
                action = viewer.wait_action()
            except KeyboardInterrupt:
                interrupted = True
                print("\n使用者中止掃描 (KeyboardInterrupt)。")
                break
            ans = "好" if action == "confirm" else ("b" if action == "back" else "q")
            ans_l = ans.lower()
        else:
            try:
                ans = ask_confirm("輸入『好』繼續，b 上一塊，q 結束: ")
            except KeyboardInterrupt:
                interrupted = True
                print("\n使用者中止掃描 (KeyboardInterrupt)。")
                break
            ans_l = ans.lower()

        if ans_l in QUIT_WORDS:
            print("使用者中止掃描。")
            break

        if ans_l in BACK_WORDS:
            if not all_records:
                print("已在第一步，無法再往前。")
                continue

            removed = all_records.pop()
            if not removed.is_predicted_center:
                finished_steps -= 1

            if face_step_i > 0:
                face_step_i -= 1
            else:
                if face_i > 0:
                    face_i -= 1
                    face_step_i = len(FACE_BLOCK_ORDER) - 1
                else:
                    if cube_i > 1:
                        cube_i -= 1
                        face_i = len(SCAN_PLAN) - 1
                        face_step_i = len(FACE_BLOCK_ORDER) - 1
                    else:
                        face_i = 0
                        face_step_i = 0

            curr_face_name, curr_face_code = SCAN_PLAN[face_i]
            block_i = FACE_BLOCK_ORDER[face_step_i]
            print(f"已回到: 第 {cube_i} 顆 {curr_face_name} 第 {block_i} 格")
            viewer.update_status(
                cube_i,
                cubes,
                face_i + 1,
                len(SCAN_PLAN),
                curr_face_name,
                curr_face_code,
                block_i,
            )
            viewer.append_log(f"已回到: 第 {cube_i} 顆 {curr_face_name} 第 {block_i} 格")
            viewer.redraw_by_records(all_records, cube_i)
            continue

        if ans_l not in CONFIRM_WORDS:
            print("未識別指令，請輸入『好』/b/q。")
            continue

        print(f"\n[SAVE] 記錄塊: {face_name}({face_code}) #{block_i}/9, 使用 S{sensor_id}")
        frame = wait_sensor_frame(uart_reader, sensor_id, timeout_s=2.0)
        if frame is None and sensor_line:
            print(f"[SAVE] 從最近行解析: {sensor_line[:70]}")
            seg_frames = parse_segment_frames(sensor_line)
            frame = seg_frames.get(sensor_id)
            if frame is None:
                parsed = build_uart_frame(sensor_line)
                if parsed.measured_r is not None:
                    frame = parsed
        if frame is None:
            print(f"[SAVE] 無數據，創建空值紀錄")
            frame = UartFrame(
                raw_line=sensor_line,
                sensor_index=sensor_id,
                sensor_status=None,
                is_predicted_center=False,
                measured_r=None,
                measured_g=None,
                measured_b=None,
                measured_h=None,
                measured_s=None,
                measured_v=None,
                parsed_face_id=None,
                parsed_face_text=None,
                cube_index=None,
                timestamp=now_str(),
            )
        else:
            print(f"[SAVE] ✓ 記錄成功: RGB({frame.measured_r},{frame.measured_g},{frame.measured_b})")

        ratio_g_over_r_x1000 = safe_ratio_x1000(frame.measured_g, frame.measured_r)
        ratio_b_over_r_x1000 = safe_ratio_x1000(frame.measured_b, frame.measured_r)
        rec = StepRecord(
            cube_index=cube_i,
            target_face_name=face_name,
            target_face_code=face_code,
            block_index=block_i,
            is_predicted_center=frame.is_predicted_center,
            selected_sensor_id=sensor_id,
            operator_confirm=ans,
            sensor_line=frame.raw_line,
            parsed_face_id=frame.parsed_face_id,
            parsed_face_text=frame.parsed_face_text,
            sensor_index=frame.sensor_index,
            sensor_status=frame.sensor_status,
            measured_r=frame.measured_r,
            measured_g=frame.measured_g,
            measured_b=frame.measured_b,
            measured_h=frame.measured_h,
            measured_s=frame.measured_s,
            measured_v=frame.measured_v,
            ratio_g_over_r_x1000=ratio_g_over_r_x1000,
            ratio_b_over_r_x1000=ratio_b_over_r_x1000,
            timestamp=frame.timestamp,
        )
        all_records.append(rec)
        finished_steps += 1
        viewer.set_block_color(
            rec.target_face_code,
            rec.block_index,
            palette_hex(TARGET_FACE_TO_PALETTE.get(rec.target_face_name, "white")),
        )

        if face_step_i == 3:
            center_frame = build_predicted_center_frame(face_name, face_code, sensor_id)
            center_rec = StepRecord(
                cube_index=cube_i,
                target_face_name=face_name,
                target_face_code=face_code,
                block_index=5,
                is_predicted_center=True,
                selected_sensor_id=sensor_id,
                operator_confirm=ans,
                sensor_line=center_frame.raw_line,
                parsed_face_id=center_frame.parsed_face_id,
                parsed_face_text=center_frame.parsed_face_text,
                sensor_index=center_frame.sensor_index,
                sensor_status=center_frame.sensor_status,
                measured_r=center_frame.measured_r,
                measured_g=center_frame.measured_g,
                measured_b=center_frame.measured_b,
                measured_h=center_frame.measured_h,
                measured_s=center_frame.measured_s,
                measured_v=center_frame.measured_v,
                ratio_g_over_r_x1000=safe_ratio_x1000(center_frame.measured_g, center_frame.measured_r),
                ratio_b_over_r_x1000=safe_ratio_x1000(center_frame.measured_b, center_frame.measured_r),
                timestamp=center_frame.timestamp,
            )
            all_records.append(center_rec)
            viewer.set_block_color(
                center_rec.target_face_code,
                center_rec.block_index,
                palette_hex(TARGET_FACE_TO_PALETTE.get(center_rec.target_face_name, "white")),
            )
            if not viewer.enable_gui:
                print(f"[SAVE] ◎ 中心格自動補色: RGB({center_frame.measured_r},{center_frame.measured_g},{center_frame.measured_b})")
            viewer.append_log(f"中心格自動補色: {face_name}({face_code})")

        if face_step_i < len(FACE_BLOCK_ORDER) - 1:
            face_step_i += 1
            continue

        # 進下一個色面
        face_step_i = 0
        if face_i < len(SCAN_PLAN) - 1:
            face_i += 1
            continue

        # 進下一顆魔方
        face_i = 0
        cube_i += 1
        if cube_i <= cubes:
            viewer.redraw_by_records(all_records, cube_i)

    threshold_summary = build_threshold_summary(all_records)

    payload: Dict[str, object] = {
        "saved_at": now_str(),
        "calibration_sensor_id": sensor_id,
        "scan_plan": [{"face_name": n, "face_code": c} for n, c in SCAN_PLAN],
        "summary": {
            "cubes_requested": cubes,
            "steps_finished": finished_steps,
            "steps_total": total_steps,
            "interrupted": interrupted,
        },
        "threshold_tuning_summary": threshold_summary,
        "records": [rec.__dict__ for rec in all_records],
    }

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    csv_path = output.with_name(f"{output.stem}_cube{cubes}_sansor{sensor_id}.csv")
    write_records_csv(csv_path, all_records)

    print("\n" + "=" * 64)
    print(f"掃描結束，已儲存: {output}")
    print(f"校正分析 CSV: {csv_path}")
    print(f"完成實測步驟: {finished_steps}/{total_steps}")
    print(f"最終紀錄筆數: {len(all_records)}/{total_records}")
    print("=" * 64)
    uart_reader.stop()
    viewer.close()
    return 0


def open_serial(port: Optional[str], baud: int) -> Optional[Any]:
    if serial is None:
        print("警告: 未安裝 pyserial，將以無序列埠模式執行。")
        print("可安裝: pip install pyserial")
        return None

    chosen_port = port.strip() if port else "COM10"
    if not port or not port.strip():
        print(f"[SERIAL] 未指定序列埠，預設使用: {chosen_port}")

    try:
        print(f"[SERIAL] 嘗試開啟 {chosen_port}@{baud} baud...")
        ser = serial.Serial(port=chosen_port, baudrate=baud, timeout=0.05)
        time.sleep(0.2)
        print(f"[SERIAL] ✓ 序列埠已開啟: {chosen_port} @ {baud} baud")
        return ser
    except Exception as exc:
        print(f"[SERIAL] ✗ 無法開啟序列埠 {chosen_port}: {exc}")
        print("[SERIAL] 無法建立連線，請確認裝置或改用 --port 指定正確埠號。")
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description="雙魔方逐色塊掃描介面")
    parser.add_argument("--cubes", type=int, default=2, help="要掃描的魔方數量（預設 2）")
    parser.add_argument("--output", type=Path, default=Path("scan_result.json"), help="輸出 JSON 檔案路徑")
    parser.add_argument("--port", type=str, default="COM10", help="序列埠，例如 COM5（預設 COM10）")
    parser.add_argument("--baud", type=int, default=115200, help="序列埠鮑率（預設 115200）")
    parser.add_argument("--sensor-id", type=int, default=1, choices=[0, 1], help="本次校正使用的感測器編號（0 或 1）")
    parser.add_argument("--no-gui", action="store_true", help="關閉展開圖視窗，使用純文字模式")
    parser.add_argument("--uart-format", type=str, default="raw", choices=["raw"], help="UART 封包格式：raw 只使用 CS1 原始量測欄位")
    args = parser.parse_args()

    if args.cubes <= 0:
        print("錯誤: --cubes 必須大於 0")
        return 2

    script_dir = Path(__file__).resolve().parent.parent
    output_path = args.output if args.output.is_absolute() else script_dir / args.output

    ser = open_serial(args.port.strip(), args.baud)
    if ser is None:
        print("[SERIAL] 無法建立序列埠連線，掃描中止。")
        return 1
    try:
        try:
            return run_scan(
                cubes=args.cubes,
                output=output_path,
                ser=ser,
                enable_gui=not args.no_gui,
                sensor_id=args.sensor_id,
            )
        except KeyboardInterrupt:
            print("\n使用者中止執行。")
            return 130
    finally:
        if ser is not None:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
