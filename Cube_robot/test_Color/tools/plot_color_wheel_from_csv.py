#!/usr/bin/env python3
"""
從掃描 CSV 讀取顏色感測器資料，並輸出色環圖。

用途:
1. 將每筆量測資料依 HSV 的 H(色相) 與 S(飽和度) 放到極座標色環。
2. 點的顏色使用量測到的 RGB（正規化後）。
3. 輸出門檻統計表與可選 JSON。
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import matplotlib.pyplot as plt

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox
except Exception:
    tk = None
    filedialog = None
    messagebox = None


FACE_NAME_EN: Dict[str, str] = {
    "白色": "White",
    "紅色": "Red",
    "綠色": "Green",
    "黃色": "Yellow",
    "橘色": "Orange",
    "藍色": "Blue",
}

EXPECTED_FACE_ORDER = ["白色", "紅色", "綠色", "黃色", "橘色", "藍色"]

FACE_TEXT_COLOR: Dict[str, str] = {
    "白色": "#2f2f2f",
    "紅色": "#8a1c1c",
    "綠色": "#1f6b2f",
    "黃色": "#8a6d00",
    "橘色": "#a34e00",
    "藍色": "#163c7a",
}

HUE_GUIDES = [
    (0, "Red (Primary)"),
    (120, "Green (Primary)"),
    (240, "Blue (Primary)"),
]

HUE_GUIDE_COLORS: Dict[str, str] = {
    "Red (Primary)": "#b71c1c",
    "Green (Primary)": "#2e7d32",
    "Blue (Primary)": "#1565c0",
}

FACE_COLORS: Dict[str, Tuple[float, float, float]] = {
    "白色": (0.82, 0.82, 0.82),      # Light gray
    "紅色": (0.99, 0.20, 0.20),      # Red
    "綠色": (0.18, 0.49, 0.20),      # Green
    "黃色": (0.99, 0.80, 0.00),      # Yellow
    "橘色": (0.99, 0.55, 0.00),      # Orange
    "藍色": (0.08, 0.40, 0.75),      # Blue
}


def clamp01(x: float) -> float:
    if x < 0.0:
        return 0.0
    if x > 1.0:
        return 1.0
    return x


def normalize_rgb(r: int, g: int, b: int) -> Tuple[float, float, float]:
    return (clamp01(r / 255.0), clamp01(g / 255.0), clamp01(b / 255.0))


def normalize_sv(value: float) -> float:
    # 相容三種常見尺度：0..1、0..255、0..1023
    if value <= 1.0:
        return clamp01(value)
    if value <= 255.0:
        return clamp01(value / 255.0)
    return clamp01(value / 1023.0)


def parse_float(value: object) -> Optional[float]:
    try:
        if value is None:
            return None
        return float(str(value).strip())
    except Exception:
        return None


def parse_int(value: object) -> Optional[int]:
    try:
        if value is None:
            return None
        return int(float(str(value).strip()))
    except Exception:
        return None


def parse_bool(value: object) -> bool:
    if value is None:
        return False
    text = str(value).strip().lower()
    return text in {"1", "true", "t", "yes", "y"}


def circular_mean_degrees(degrees: List[float]) -> float:
    if not degrees:
        return 0.0
    x = 0.0
    y = 0.0
    for degree in degrees:
        angle = math.radians(degree)
        x += math.cos(angle)
        y += math.sin(angle)
    if x == 0.0 and y == 0.0:
        return float(degrees[0])
    return math.degrees(math.atan2(y, x)) % 360.0


def offset_label(angle_deg: float, radius: float) -> Tuple[Tuple[int, int], str]:
    if 45.0 <= angle_deg < 135.0:
        return (8, -6), "left"
    if 135.0 <= angle_deg < 225.0:
        return (-8, 0), "right"
    if 225.0 <= angle_deg < 315.0:
        return (8, 6), "left"
    return (8, 0), "left"


def resolve_default_csv(root_dir: Path) -> Path:
    output_dir = root_dir / "output"
    candidates: List[Path] = []
    if output_dir.exists():
        candidates.extend(output_dir.rglob("scan_result_cube*_sansor*.csv"))
        if not candidates:
            candidates.extend(output_dir.rglob("scan_result*.csv"))

    if not candidates:
        return root_dir / "scan_result_cube1_sansor1.csv"

    candidates.sort(key=lambda p: p.stat().st_mtime)
    return candidates[-1]


def load_rows(csv_path: Path, color_filter: str = "") -> List[Dict[str, object]]:
    rows: List[Dict[str, object]] = []
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        for raw in reader:
            target_face = str(raw.get("target_face_name", "")).strip()
            if color_filter and target_face != color_filter:
                continue

            h = parse_float(raw.get("measured_h"))
            s = parse_float(raw.get("measured_s"))
            v = parse_float(raw.get("measured_v"))
            r = parse_int(raw.get("measured_r"))
            g = parse_int(raw.get("measured_g"))
            b = parse_int(raw.get("measured_b"))
            cube_index = parse_int(raw.get("cube_index"))
            is_predicted_center = parse_bool(raw.get("is_predicted_center"))

            if h is None or s is None or v is None or r is None or g is None or b is None:
                continue

            rows.append(
                {
                    "target_face_name": target_face,
                    "h": h,
                    "s": normalize_sv(s),
                    "v": normalize_sv(v),
                    "r": max(0, min(255, r)),
                    "g": max(0, min(255, g)),
                    "b": max(0, min(255, b)),
                    "cube_index": cube_index if cube_index is not None else 1,
                    "is_predicted_center": is_predicted_center,
                }
            )
    return rows


def percentile(values: List[float], p: float) -> float:
    if not values:
        return 0.0
    if len(values) == 1:
        return float(values[0])
    sorted_vals = sorted(values)
    p = clamp01(p)
    idx = p * (len(sorted_vals) - 1)
    lo = int(math.floor(idx))
    hi = int(math.ceil(idx))
    if lo == hi:
        return float(sorted_vals[lo])
    w = idx - lo
    return float(sorted_vals[lo] * (1.0 - w) + sorted_vals[hi] * w)


def build_thresholds(rows: List[Dict[str, object]], low_pct: float, high_pct: float) -> Dict[str, object]:
    by_color: Dict[str, List[Dict[str, object]]] = {}
    for row in rows:
        if bool(row.get("is_predicted_center")):
            continue
        name = str(row.get("target_face_name", "")).strip()
        if not name:
            continue
        by_color.setdefault(name, []).append(row)

    result: Dict[str, object] = {
        "version": 1,
        "method": "percentile",
        "low_pct": low_pct,
        "high_pct": high_pct,
        "colors": {},
    }

    for name, group in by_color.items():
        hs = [float(r["h"]) for r in group]
        ss = [float(r["s"]) for r in group]
        vs = [float(r["v"]) for r in group]

        h_min = percentile(hs, low_pct / 100.0)
        h_max = percentile(hs, high_pct / 100.0)
        s_min = percentile(ss, low_pct / 100.0)
        s_max = percentile(ss, high_pct / 100.0)
        v_min = percentile(vs, low_pct / 100.0)
        v_max = percentile(vs, high_pct / 100.0)

        result["colors"][name] = {
            "count": len(group),
            "h_deg": [round(h_min, 2), round(h_max, 2)],
            "s_0to1": [round(s_min, 4), round(s_max, 4)],
            "v_0to1": [round(v_min, 4), round(v_max, 4)],
            "hsv_scaled_255": {
                "h_0to255": [round(h_min * 255.0 / 360.0), round(h_max * 255.0 / 360.0)],
                "s_0to255": [round(s_min * 255.0), round(s_max * 255.0)],
                "v_0to255": [round(v_min * 255.0), round(v_max * 255.0)],
            },
        }
    return result


def print_data_summary(rows: List[Dict[str, object]]) -> None:
    total = len(rows)
    pred = sum(1 for r in rows if bool(r.get("is_predicted_center")))
    meas = total - pred
    print("=== Data Summary ===")
    print(f"Total rows: {total}")
    print(f"Measured:   {meas}")
    print(f"Predicted:  {pred}")

    by_cube: Dict[int, int] = {}
    for r in rows:
        cube_id = int(r.get("cube_index", 1))
        by_cube[cube_id] = by_cube.get(cube_id, 0) + 1
    for cube_id in sorted(by_cube.keys()):
        print(f"Cube {cube_id}: {by_cube[cube_id]}")


def print_thresholds(thresholds: Dict[str, object]) -> None:
    print("\n=== Threshold Table (from CSV) ===")
    colors = thresholds.get("colors", {})
    if not isinstance(colors, dict):
        colors = {}

    def fmt_range(values: object, width: int) -> str:
        if isinstance(values, list) and len(values) == 2:
            return f"[{values[0]}, {values[1]}]".ljust(width)
        return "-".ljust(width)

    headers = [
        ("Color", 8),
        ("n", 5),
        ("H(deg)", 17),
        ("S(0..1)", 17),
        ("V(0..1)", 17),
        ("H(0..255)", 17),
        ("S(0..255)", 17),
        ("V(0..255)", 17),
    ]

    sep = "+" + "+".join("-" * w for _, w in headers) + "+"
    head = "|" + "|".join(name.center(w) for name, w in headers) + "|"
    print(sep)
    print(head)
    print(sep)

    for face_name in EXPECTED_FACE_ORDER:
        info = colors.get(face_name)
        color_label = FACE_NAME_EN.get(face_name, face_name)
        if isinstance(info, dict):
            n = int(info.get("count", 0))
            h_deg = fmt_range(info.get("h_deg"), 17)
            s_01 = fmt_range(info.get("s_0to1"), 17)
            v_01 = fmt_range(info.get("v_0to1"), 17)
            hsv_255 = info.get("hsv_scaled_255", {})
            h_255 = fmt_range(hsv_255.get("h_0to255"), 17) if isinstance(hsv_255, dict) else "-".ljust(17)
            s_255 = fmt_range(hsv_255.get("s_0to255"), 17) if isinstance(hsv_255, dict) else "-".ljust(17)
            v_255 = fmt_range(hsv_255.get("v_0to255"), 17) if isinstance(hsv_255, dict) else "-".ljust(17)
        else:
            n = 0
            h_deg = "-".ljust(17)
            s_01 = "-".ljust(17)
            v_01 = "-".ljust(17)
            h_255 = "-".ljust(17)
            s_255 = "-".ljust(17)
            v_255 = "-".ljust(17)

        row = (
            "|"
            + f"{color_label:<8}|"
            + f"{n:>5}|"
            + f"{h_deg}|"
            + f"{s_01}|"
            + f"{v_01}|"
            + f"{h_255}|"
            + f"{s_255}|"
            + f"{v_255}|"
        )
        print(row)
    print(sep)


def ask_save_path(default_output_path: Path) -> Optional[Path]:
    if tk is not None and filedialog is not None and messagebox is not None:
        root = tk.Tk()
        root.withdraw()
        should_save = messagebox.askyesno("儲存圖表", "要儲存目前預覽的圖表嗎？")
        if not should_save:
            root.destroy()
            return None
        selected = filedialog.asksaveasfilename(
            title="儲存色環圖",
            initialdir=str(default_output_path.parent),
            initialfile=default_output_path.name,
            defaultextension=".png",
            filetypes=[("PNG", "*.png"), ("All files", "*.*")],
        )
        root.destroy()
        if not selected:
            return None
        return Path(selected)

    ans = input("是否儲存圖表？(y/N): ").strip().lower()
    if ans not in {"y", "yes"}:
        return None
    path_in = input(f"輸出路徑（預設: {default_output_path}）: ").strip()
    return Path(path_in) if path_in else default_output_path


def _plot_one_cube(ax: plt.Axes, rows: List[Dict[str, object]], title: str) -> None:
    # 依 target_face_name 分組資料
    by_face: Dict[str, List[Dict[str, object]]] = {}
    for r in rows:
        face = str(r.get("target_face_name", "")).strip()
        if face:
            by_face.setdefault(face, []).append(r)

    ax.set_theta_zero_location("N")
    ax.set_theta_direction(-1)
    ax.set_rmax(1.28)
    ax.set_rticks([0.25, 0.5, 0.75, 1.0])
    ax.set_rlabel_position(135)
    theta_ticks = [0, 45, 90, 135, 180, 225, 270, 315]
    ax.set_thetagrids(theta_ticks, [f"{deg}°" for deg in theta_ticks])
    ax.set_title(title, pad=22, fontsize=12, fontweight="semibold")
    ax.set_facecolor("#fbfbfb")
    ax.grid(alpha=0.35)
    ax.tick_params(axis="both", labelsize=8)

    ref_hues = [0, 60, 120, 180, 240, 300]
    for h in ref_hues:
        th = math.radians(h)
        ax.plot([th, th], [0.0, 0.96], linewidth=0.8, alpha=0.25, color="#666666")

    for deg, label in HUE_GUIDES:
        guide_color = HUE_GUIDE_COLORS.get(label, "#4a4a4a")
        if label == "Red (Primary)":
            xytext = (-14, 10)
            ha = "right"
        elif label == "Green (Primary)":
            xytext = (16, -8)
            ha = "left"
        else:
            xytext = (-16, -8)
            ha = "right"
        ax.annotate(
            label,
            xy=(math.radians(deg), 1.0),
            xytext=xytext,
            textcoords="offset points",
            ha=ha,
            va="center",
            fontsize=8.5,
            color=guide_color,
            bbox={"boxstyle": "round,pad=0.14", "facecolor": "white", "edgecolor": guide_color, "linewidth": 0.6, "alpha": 0.92},
            arrowprops={"arrowstyle": "-", "color": guide_color, "lw": 0.7, "alpha": 0.7},
        )

    label_positions: Dict[str, Tuple[float, float]] = {}

    for face, group in sorted(by_face.items(), key=lambda kv: kv[0]):
        theta_group = [math.radians(float(r["h"])) for r in group]
        radius_group = [float(r["s"]) for r in group]
        sizes_group = []
        for r in group:
            sizes_group.append(24 + 56 * float(r["v"]))
        
        face_label = FACE_NAME_EN.get(face, face)
        face_color = FACE_COLORS.get(face, (0.5, 0.5, 0.5))
        ax.scatter(
            theta_group,
            radius_group,
            c=[face_color],
            s=sizes_group,
            alpha=0.9,
            edgecolors="#202020",
            linewidths=0.55,
            label=face_label,
        )

        label_theta_deg = circular_mean_degrees([float(r["h"]) for r in group])
        label_radius = min(1.16, max(radius_group) + 0.06)
        label_positions[face] = (label_theta_deg, label_radius)

    for face, (label_theta_deg, label_radius) in label_positions.items():
        face_label = FACE_NAME_EN.get(face, face)
        text_color = FACE_TEXT_COLOR.get(face, "#2f2f2f")
        theta_rad = math.radians(label_theta_deg)
        if face == "紅色":
            offset = (-8, -2)
            ha = "right"
        elif 45.0 <= label_theta_deg < 135.0:
            offset = (8, -6)
            ha = "left"
        elif 135.0 <= label_theta_deg < 225.0:
            offset = (-8, 0)
            ha = "right"
        elif 225.0 <= label_theta_deg < 315.0:
            offset = (8, 6)
            ha = "left"
        else:
            offset = (8, 0)
            ha = "left"
        ax.annotate(
            face_label,
            xy=(theta_rad, label_radius),
            xytext=offset,
            textcoords="offset points",
            ha=ha,
            va="center",
            fontsize=9,
            color=text_color,
            bbox={"boxstyle": "round,pad=0.18", "facecolor": "white", "edgecolor": text_color, "linewidth": 0.6, "alpha": 0.94},
        )


def plot_color_wheel(rows: List[Dict[str, object]], title: str, default_output_path: Path, show_plot: bool, ask_save: bool) -> Optional[Path]:
    if not rows:
        raise ValueError("沒有可繪圖資料，請確認 CSV 欄位或篩選條件。")

    cube_groups: Dict[int, List[Dict[str, object]]] = {}
    for row in rows:
        cube_id = int(row.get("cube_index", 1))
        cube_groups.setdefault(cube_id, []).append(row)

    sorted_cube_ids = sorted(cube_groups.keys())
    panel_count = len(sorted_cube_ids)
    fig_width = max(13.0, 5.8 * panel_count)
    fig_height = 7.2 if panel_count == 1 else 7.6
    fig, axes = plt.subplots(
        1,
        panel_count,
        subplot_kw={"projection": "polar"},
        figsize=(fig_width, fig_height),
        dpi=130,
        constrained_layout=False,
    )
    axes_list = [axes] if panel_count == 1 else list(axes)

    for ax, cube_id in zip(axes_list, sorted_cube_ids):
        _plot_one_cube(ax, cube_groups[cube_id], f"Cube {cube_id}")

    fig.suptitle(title, fontsize=13, fontweight="semibold", y=0.98)
    fig.tight_layout(rect=(0, 0.02, 1.0, 1.05))

    if show_plot:
        plt.show()

    saved_path: Optional[Path] = None
    if ask_save:
        selected_path = ask_save_path(default_output_path)
        if selected_path is not None:
            selected_path.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(selected_path)
            saved_path = selected_path

    plt.close(fig)
    return saved_path


def main() -> int:
    parser = argparse.ArgumentParser(description="從掃描 CSV 畫色環散點圖")
    default_csv = resolve_default_csv(Path(__file__).resolve().parent.parent)
    parser.add_argument("--csv", type=Path, default=None, help="輸入 CSV 路徑；未指定時會開啟檔案選擇器")
    parser.add_argument("--out", type=Path, default=Path("color_wheel.png"), help="儲存圖檔時的預設路徑")
    parser.add_argument("--color", type=str, default="", help="只畫指定 target_face_name（例如 白色）")
    parser.add_argument("--show", action=argparse.BooleanOptionalAction, default=True, help="是否開啟預覽視窗（預設開啟）")
    parser.add_argument("--ask-save", action=argparse.BooleanOptionalAction, default=True, help="是否詢問是否儲存圖檔（預設詢問）")
    parser.add_argument("--threshold-json", type=Path, default=None, help="輸出閾值 JSON 路徑")
    parser.add_argument("--pct-low", type=float, default=5.0, help="閾值下百分位（預設 5）")
    parser.add_argument("--pct-high", type=float, default=95.0, help="閾值上百分位（預設 95）")
    args = parser.parse_args()

    csv_path = args.csv
    if csv_path is None:
        if filedialog is not None and tk is not None:
            root = tk.Tk()
            root.withdraw()
            selected = filedialog.askopenfilename(
                title="選擇掃描 CSV",
                initialdir=str(default_csv.parent),
                filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
            )
            root.destroy()
            if selected:
                csv_path = Path(selected)
        if csv_path is None:
            csv_path = default_csv

    if not csv_path.exists():
        raise FileNotFoundError(f"找不到 CSV: {csv_path}")

    rows = load_rows(csv_path, args.color.strip())
    print_data_summary(rows)

    suffix = f" | filter={args.color.strip()}" if args.color.strip() else ""
    title = f"Color Sensor Wheel (H vs S, point size by V){suffix}"
    saved_path = plot_color_wheel(rows, title, args.out, args.show, args.ask_save)

    thresholds = build_thresholds(rows, args.pct_low, args.pct_high)
    print_thresholds(thresholds)

    if args.threshold_json is not None:
        args.threshold_json.parent.mkdir(parents=True, exist_ok=True)
        args.threshold_json.write_text(json.dumps(thresholds, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"已輸出閾值 JSON: {args.threshold_json}")

    if saved_path is not None:
        print(f"已輸出色環圖: {saved_path}")
    else:
        print("未儲存色環圖（你可在預覽後選擇不儲存）")
    print(f"資料點數: {len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
