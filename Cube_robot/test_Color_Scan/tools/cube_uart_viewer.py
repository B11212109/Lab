#!/usr/bin/env python3
"""UART cube face viewer for the AT32 color scan test."""

from __future__ import annotations

import argparse
import csv
import queue
import re
import sys
import threading
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import scrolledtext
from typing import Optional


FACE_ORDER = "URFDLB"
DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200
DEFAULT_CSV = "cube_hsv_samples.csv"
PHYSICAL_ROTATION_TURNS = {
    "U": 2,
    "R": 2,
    "F": 2,
    "D": 2,
    "L": 2,
    "B": 2,
}

FACE_LABELS = {
    "U": "U / yellow / M1",
    "R": "R / red",
    "F": "F / blue / M3",
    "D": "D / white / M2",
    "L": "L / orange",
    "B": "B / green",
}
FACE_COLORS = {
    "U": "#ffd84d",
    "R": "#d13b4c",
    "F": "#2c73d2",
    "D": "#f7f8fa",
    "L": "#ff8a2a",
    "B": "#22a66b",
    "?": "#30363d",
}
TEXT_COLORS = {
    "U": "#111111",
    "R": "#ffffff",
    "F": "#ffffff",
    "D": "#111111",
    "L": "#111111",
    "B": "#ffffff",
    "?": "#30363d",
}
NET_OFFSETS = {
    "U": (3, 0),
    "L": (0, 3),
    "F": (3, 3),
    "R": (6, 3),
    "B": (9, 3),
    "D": (3, 6),
}
RESET_LINES = {"COLOR_SCAN_BOOT", "SCAN_START"}
FACE_LINE_RE = re.compile(r"^FACE_([URFDLB])=([URFDLB?]{9})$")
HSV_SAMPLE_RE = re.compile(r"^HSV_SAMPLE INDEX=(\d+) FACE=([URFDLB?]) H=(\d+) S=(\d+) V=(\d+)$")


class HsvCsvLogger:
    FIELDNAMES = ["timestamp", "scan_id", "index", "face", "h", "s", "v", "source_line"]

    def __init__(self, path: str) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        is_new = not self.path.exists() or self.path.stat().st_size == 0
        self.file = self.path.open("a", newline="", encoding="utf-8")
        self.writer = csv.DictWriter(self.file, fieldnames=self.FIELDNAMES)
        self.scan_id = 0

        if is_new:
            self.writer.writeheader()
            self.file.flush()

    def start_scan(self) -> None:
        self.scan_id += 1

    def write_sample(self, index: str, face: str, h: str, s: str, v: str, source_line: str) -> None:
        if self.scan_id == 0:
            self.start_scan()

        self.writer.writerow(
            {
                "timestamp": datetime.now().isoformat(timespec="milliseconds"),
                "scan_id": self.scan_id,
                "index": index,
                "face": face,
                "h": h,
                "s": s,
                "v": v,
                "source_line": source_line,
            }
        )
        self.file.flush()

    def close(self) -> None:
        self.file.close()


class CubeViewer:
    def __init__(
        self,
        line_queue: queue.Queue[tuple[str, str]],
        port: str,
        baud: int,
        cell_size: int = 46,
        csv_logger: Optional[HsvCsvLogger] = None,
    ) -> None:
        self.line_queue = line_queue
        self.port = port
        self.baud = baud
        self.cell = cell_size
        self.csv_logger = csv_logger
        self.margin = 24
        self.face54 = "?" * 54
        self.face_parts: dict[str, str] = {}
        self.collecting_moves = False
        self.move_lines: list[str] = []
        self.physical_view = False

        self.root = tk.Tk()
        self.root.title("AT32 Cube Color Scan")
        self.root.configure(bg="#15181d")
        self.root.minsize(1020, 720)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        self.connection_var = tk.StringVar(value=f"{self.port} @ {self.baud} baud")
        self.status_var = tk.StringVar(value="Waiting for FACE54")
        self.count_var = tk.StringVar(value="U=0 R=0 F=0 D=0 L=0 B=0")
        self.count_ok_var = tk.StringVar(value="COUNT_OK=--")
        self.center_ref_var = tk.StringVar(value="M1=U/YELLOW   M2=D/WHITE   M3=F/BLUE")
        self.view_mode_var = tk.StringVar(value="View: Raw FACE54")

        self.build_ui()
        self.clear_cube("boot")
        self.root.after(50, self.poll_queue)

    def build_ui(self) -> None:
        header = tk.Frame(self.root, bg="#15181d")
        header.grid(row=0, column=0, columnspan=2, sticky="ew", padx=16, pady=(14, 8))
        header.columnconfigure(0, weight=1)

        title = tk.Label(
            header,
            text="AT32 Cube Color Scan",
            bg="#15181d",
            fg="#f6f8fa",
            font=("Segoe UI", 20, "bold"),
            anchor="w",
        )
        title.grid(row=0, column=0, sticky="w")

        subtitle = tk.Label(
            header,
            textvariable=self.center_ref_var,
            bg="#15181d",
            fg="#9da7b3",
            font=("Segoe UI", 10),
            anchor="w",
        )
        subtitle.grid(row=1, column=0, sticky="w", pady=(2, 0))

        self.connection_label = tk.Label(
            header,
            textvariable=self.connection_var,
            bg="#242a32",
            fg="#d0d7de",
            font=("Consolas", 11, "bold"),
            padx=12,
            pady=8,
        )
        self.connection_label.grid(row=0, column=1, rowspan=2, sticky="e")

        self.view_button = tk.Button(
            header,
            textvariable=self.view_mode_var,
            command=self.toggle_view_mode,
            bg="#2f81f7",
            fg="#ffffff",
            activebackground="#1f6feb",
            activeforeground="#ffffff",
            relief="flat",
            padx=12,
            pady=8,
            font=("Segoe UI", 10, "bold"),
        )
        self.view_button.grid(row=0, column=2, rowspan=2, sticky="e", padx=(10, 0))

        canvas_width = self.margin * 2 + self.cell * 12
        canvas_height = self.margin * 2 + self.cell * 9 + 34
        canvas_frame = tk.Frame(self.root, bg="#1f242b", padx=12, pady=12)
        canvas_frame.grid(row=1, column=0, sticky="nsew", padx=(16, 8), pady=(0, 16))
        self.canvas = tk.Canvas(
            canvas_frame,
            width=canvas_width,
            height=canvas_height,
            bg="#1f242b",
            highlightthickness=0,
        )
        self.canvas.grid(row=0, column=0, sticky="nsew")

        side = tk.Frame(self.root, bg="#15181d")
        side.grid(row=1, column=1, sticky="nsew", padx=(8, 16), pady=(0, 16))
        side.columnconfigure(0, weight=1)
        side.rowconfigure(6, weight=1)
        side.rowconfigure(8, weight=1)

        self.make_card(side, "Status", self.status_var, 0)
        self.make_card(side, "Counts", self.count_var, 1)
        self.make_card(side, "Validation", self.count_ok_var, 2)

        legend_title = self.section_label(side, "Legend", 3)
        legend_title.grid(sticky="ew", pady=(10, 4))
        legend = tk.Frame(side, bg="#15181d")
        legend.grid(row=4, column=0, sticky="ew")
        for i, face in enumerate(FACE_ORDER):
            self.make_legend_chip(legend, face).grid(row=i // 2, column=i % 2, sticky="ew", padx=(0, 8), pady=3)
        legend.columnconfigure(0, weight=1)
        legend.columnconfigure(1, weight=1)

        self.section_label(side, "Move Table", 5).grid(sticky="ew", pady=(12, 4))
        self.move_text = scrolledtext.ScrolledText(
            side,
            height=9,
            width=48,
            bg="#0f1318",
            fg="#d0d7de",
            insertbackground="#d0d7de",
            relief="flat",
            font=("Consolas", 9),
        )
        self.move_text.grid(row=6, column=0, sticky="nsew")
        self.move_text.insert("end", "Waiting for MOVE_TABLE_BEGIN\n")
        self.move_text.configure(state="disabled")

        self.section_label(side, "UART Log", 7).grid(sticky="ew", pady=(12, 4))
        self.log = scrolledtext.ScrolledText(
            side,
            height=12,
            width=48,
            bg="#0f1318",
            fg="#b8c0cc",
            insertbackground="#d0d7de",
            relief="flat",
            font=("Consolas", 9),
        )
        self.log.grid(row=8, column=0, sticky="nsew")

        self.root.rowconfigure(1, weight=1)
        self.root.columnconfigure(0, weight=3)
        self.root.columnconfigure(1, weight=2)

    def make_card(self, parent: tk.Widget, title: str, var: tk.StringVar, row: int) -> None:
        card = tk.Frame(parent, bg="#242a32", padx=12, pady=10)
        card.grid(row=row, column=0, sticky="ew", pady=(0, 8))
        card.columnconfigure(0, weight=1)
        tk.Label(
            card,
            text=title.upper(),
            bg="#242a32",
            fg="#8b949e",
            font=("Segoe UI", 8, "bold"),
            anchor="w",
        ).grid(row=0, column=0, sticky="ew")
        tk.Label(
            card,
            textvariable=var,
            bg="#242a32",
            fg="#f6f8fa",
            font=("Consolas", 11, "bold"),
            anchor="w",
            wraplength=360,
        ).grid(row=1, column=0, sticky="ew", pady=(3, 0))

    def section_label(self, parent: tk.Widget, text: str, row: int) -> tk.Label:
        label = tk.Label(
            parent,
            text=text,
            bg="#15181d",
            fg="#f6f8fa",
            font=("Segoe UI", 11, "bold"),
            anchor="w",
        )
        label.grid(row=row, column=0)
        return label

    def make_legend_chip(self, parent: tk.Widget, face: str) -> tk.Frame:
        chip = tk.Frame(parent, bg="#242a32", padx=8, pady=6)
        swatch = tk.Label(chip, text="  ", bg=FACE_COLORS[face], width=2)
        swatch.grid(row=0, column=0, padx=(0, 8))
        label = tk.Label(
            chip,
            text=FACE_LABELS[face],
            bg="#242a32",
            fg="#d0d7de",
            font=("Segoe UI", 9),
            anchor="w",
        )
        label.grid(row=0, column=1, sticky="ew")
        chip.columnconfigure(1, weight=1)
        return chip

    def close(self) -> None:
        self.root.quit()
        self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()

    def toggle_view_mode(self) -> None:
        self.physical_view = not self.physical_view
        self.view_mode_var.set("View: Physical 180" if self.physical_view else "View: Raw FACE54")
        self.draw_cube("view toggle")

    def poll_queue(self) -> None:
        while True:
            try:
                kind, payload = self.line_queue.get_nowait()
            except queue.Empty:
                break

            if kind == "line":
                self.handle_line(payload)
            elif kind == "status":
                self.status_var.set(payload)
                self.append_log(payload)
            elif kind == "error":
                self.status_var.set(payload)
                self.append_log(payload)

        self.root.after(50, self.poll_queue)

    def append_log(self, text: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="normal")

    def set_move_text(self, lines: list[str]) -> None:
        self.move_text.configure(state="normal")
        self.move_text.delete("1.0", "end")
        if lines:
            self.move_text.insert("end", "\n".join(lines) + "\n")
        else:
            self.move_text.insert("end", "Waiting for MOVE_TABLE_BEGIN\n")
        self.move_text.configure(state="disabled")

    def handle_line(self, line: str) -> None:
        self.append_log(line)

        if line == "SCAN_START" and self.csv_logger is not None:
            self.csv_logger.start_scan()

        if line in RESET_LINES or line.startswith("RESET"):
            self.clear_cube(line)
            return

        match = HSV_SAMPLE_RE.match(line)
        if match:
            if self.csv_logger is not None:
                index, face, h, s, v = match.groups()
                self.csv_logger.write_sample(index, face, h, s, v, line)
            return

        if line.startswith("CENTER_REF "):
            self.center_ref_var.set(line.removeprefix("CENTER_REF "))
            return

        if line == "MOVE_TABLE_BEGIN":
            self.collecting_moves = True
            self.move_lines = []
            self.set_move_text(["Receiving move table..."])
            return

        if line == "MOVE_TABLE_END":
            self.collecting_moves = False
            self.set_move_text(self.move_lines)
            return

        if self.collecting_moves:
            self.move_lines.append(line)
            return

        if line.startswith("FACE54="):
            face54 = line.split("=", 1)[1].strip()
            if len(face54) >= 54:
                self.set_face54(face54[:54], "FACE54")
            return

        match = FACE_LINE_RE.match(line)
        if match:
            face, value = match.groups()
            self.face_parts[face] = value
            if all(face_name in self.face_parts for face_name in FACE_ORDER):
                face54 = "".join(self.face_parts[face_name] for face_name in FACE_ORDER)
                self.set_face54(face54, "FACE_*")
            return

        if line.startswith("COUNT "):
            self.count_var.set(line.removeprefix("COUNT "))
            return

        if line.startswith("COUNT_OK="):
            self.count_ok_var.set(line)
            return

        if line == "SCAN_OK":
            self.status_var.set("Scan finished, waiting for FACE54")
        elif line == "SCAN_FAIL":
            self.status_var.set("Scan failed")

    def clear_cube(self, reason: str) -> None:
        self.face54 = "?" * 54
        self.face_parts.clear()
        self.count_var.set("U=0 R=0 F=0 D=0 L=0 B=0")
        self.count_ok_var.set("COUNT_OK=--")
        self.status_var.set(f"Cleared by {reason}")
        self.draw_cube(reason)

    def set_face54(self, face54: str, source: str) -> None:
        cleaned = "".join(ch if ch in FACE_COLORS else "?" for ch in face54[:54])
        if len(cleaned) != 54:
            return

        self.face54 = cleaned
        counts = {face: cleaned.count(face) for face in FACE_ORDER}
        self.count_var.set(" ".join(f"{face}={counts[face]}" for face in FACE_ORDER))
        self.status_var.set(f"Updated from {source}")
        self.draw_cube(source)

    def display_block_for_face(self, face_name: str, block: str) -> str:
        turns = PHYSICAL_ROTATION_TURNS.get(face_name, 0) if self.physical_view else 0
        cells = list(block[:9])

        while len(cells) < 9:
            cells.append("?")

        for _ in range(turns % 4):
            cells = [
                cells[6], cells[3], cells[0],
                cells[7], cells[4], cells[1],
                cells[8], cells[5], cells[2],
            ]

        return "".join(cells)

    def draw_cube(self, source: str) -> None:
        self.canvas.delete("all")
        self.canvas.create_rectangle(0, 0, 2000, 2000, fill="#1f242b", outline="")
        self.canvas.create_text(
            self.margin,
            self.margin + self.cell * 9 + 18,
            text=f"Source: {source}",
            fill="#9da7b3",
            anchor="w",
            font=("Consolas", 11),
        )

        for face_index, face_name in enumerate(FACE_ORDER):
            raw_block = self.face54[face_index * 9 : face_index * 9 + 9]
            block = self.display_block_for_face(face_name, raw_block)
            origin_col, origin_row = NET_OFFSETS[face_name]
            label_x = self.margin + origin_col * self.cell + self.cell * 1.5
            label_y = self.margin + origin_row * self.cell - 10
            self.canvas.create_text(
                label_x,
                label_y,
                text=FACE_LABELS[face_name],
                fill="#f6f8fa",
                font=("Segoe UI", 9, "bold"),
            )

            for pos, color_key in enumerate(block):
                row = pos // 3
                col = pos % 3
                x0 = self.margin + (origin_col + col) * self.cell
                y0 = self.margin + (origin_row + row) * self.cell
                x1 = x0 + self.cell - 3
                y1 = y0 + self.cell - 3
                fill = FACE_COLORS.get(color_key, FACE_COLORS["?"])
                text_fill = TEXT_COLORS.get(color_key, TEXT_COLORS["?"])
                self.canvas.create_rectangle(x0 + 2, y0 + 3, x1 + 2, y1 + 3, fill="#11161c", outline="")
                self.canvas.create_rectangle(x0, y0, x1, y1, fill=fill, outline="#0d1117", width=2)
                display_text = "" if color_key == "?" else color_key
                self.canvas.create_text(
                    (x0 + x1) / 2,
                    (y0 + y1) / 2,
                    text=display_text,
                    fill=text_fill,
                    font=("Consolas", 16, "bold"),
                )


def serial_worker(port: str, baud: int, line_queue: queue.Queue[tuple[str, str]], stop_event: threading.Event) -> None:
    try:
        import serial
    except ImportError:
        line_queue.put(("error", "pyserial is not installed. Run: python -m pip install pyserial"))
        return

    try:
        with serial.Serial(port, baud, timeout=0.2) as ser:
            line_queue.put(("status", f"Opened {port} at {baud} baud"))
            pending = bytearray()
            while not stop_event.is_set():
                chunk = ser.read(128)
                if not chunk:
                    continue

                pending.extend(chunk)
                while b"\n" in pending:
                    raw_line, _, pending = pending.partition(b"\n")
                    line = raw_line.decode("ascii", errors="replace").strip()
                    if line:
                        line_queue.put(("line", line))
    except Exception as exc:
        line_queue.put(("error", f"Serial error: {exc}"))


def list_ports() -> int:
    try:
        from serial.tools import list_ports as serial_list_ports
    except ImportError:
        print("pyserial is not installed. Run: python -m pip install pyserial", file=sys.stderr)
        return 1

    ports = list(serial_list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return 0

    for port in ports:
        print(f"{port.device}\t{port.description}")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Display AT32 cube scan UART output as a colored cube net.")
    parser.add_argument("port", nargs="?", default=DEFAULT_PORT, help=f"Serial port, default {DEFAULT_PORT}")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Serial baudrate, default {DEFAULT_BAUD}")
    parser.add_argument("--demo", action="store_true", help="Show a solved cube without opening UART")
    parser.add_argument("--list-ports", action="store_true", help="List serial ports and exit")
    parser.add_argument("--csv", default=DEFAULT_CSV, help=f"HSV CSV output path, default {DEFAULT_CSV}")
    parser.add_argument("--no-csv", action="store_true", help="Disable HSV CSV logging")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.list_ports:
        return list_ports()

    line_queue: queue.Queue[tuple[str, str]] = queue.Queue()
    stop_event = threading.Event()
    csv_logger = None

    if not args.no_csv:
        try:
            csv_logger = HsvCsvLogger(args.csv)
        except OSError as exc:
            print(f"Failed to open CSV file {args.csv}: {exc}", file=sys.stderr)
            return 1
        line_queue.put(("status", f"CSV logging to {csv_logger.path}"))

    if args.demo:
        line_queue.put(("line", "CENTER_REF M1=U/YELLOW M2=D/WHITE M3=F/BLUE"))
        line_queue.put(("line", "FACE54=" + "U" * 9 + "R" * 9 + "F" * 9 + "D" * 9 + "L" * 9 + "B" * 9))
        line_queue.put(("line", "COUNT U=9 R=9 F=9 D=9 L=9 B=9 OTHER=0"))
        line_queue.put(("line", "COUNT_OK=YES"))
    else:
        thread = threading.Thread(target=serial_worker, args=(args.port, args.baud, line_queue, stop_event), daemon=True)
        thread.start()

    try:
        viewer = CubeViewer(line_queue, args.port, args.baud, csv_logger=csv_logger)
        viewer.run()
    finally:
        stop_event.set()
        if csv_logger is not None:
            csv_logger.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
