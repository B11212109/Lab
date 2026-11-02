# 音訊降取樣比較實驗

在原始 PCM 振幅尺度比較 Linear、FFT、Cubic、Polyphase、SoXR HQ，由 44.1 kHz mono PCM_16 轉為 16 kHz mono PCM_16。原始錄音不會被修改。新結果只寫入 `results/five_methods/`；重跑會更新這次實驗的結果。

## Windows 執行

目前 `.venv` 是 uv 管理的 Python 環境，已有必要套件，未安裝 pip。需要補裝或更新時：

```powershell
uv pip install --python .\.venv\Scripts\python.exe -r requirements.txt
.\.venv\Scripts\python.exe compare_resampling.py
.\.venv\Scripts\python.exe interactive_viewer.py
```

亦可使用 `view_waveforms.py` 開啟同一介面。`generate_test_audio.py` 可單獨產生人工音檔，存在時不會覆寫。`--repeats 100` 可增加正式計時次數（最低 30）。所有路徑相對於程式檔所在位置，不依賴啟動目錄。

```powershell
.\.venv\Scripts\python.exe -m unittest test_resampling test_interactive_viewer -v
.\.venv\Scripts\python.exe interactive_viewer.py --smoke-test
```

無顯示器的自動驗證可先設定 `$env:QT_QPA_PLATFORM='offscreen'`；一般使用請移除此環境變數。

## 輸入與量測

四種非 SoXR 方法統一使用 float32 音訊輸入與包裝函式輸出；SoXR HQ 保持 int16。注意 `numpy.interp` 與 `CubicSpline` 會提升至 float64 原生結果，後者係數也為 float64，因此再轉回 float32，不能宣稱純 float32 內部運算。時間座標與獨立品質分析保留 float64，避免量測精度影響比較。CSV 分別記錄原生與轉型後輸出型別，必要轉型包含於計時。

## 程式檔案用途

|檔案|用途|
|---|---|
|`audio_core.py`|五種降取樣、PCM 轉型與裁切、讀檔驗證、振幅／PSD／時頻分析等共用函式。|
|`generate_test_audio.py`|建立五秒人工測試音檔，含單音、掃頻與脈衝；既有檔案不覆寫。|
|`compare_resampling.py`|主實驗入口：處理兩音檔、計時、寫 WAV、產生圖表、CSV 與報告。|
|`interactive_viewer.py`|PyQtGraph 互動視窗，切換音檔／方法、縮放波形、查看取樣點、PSD 與時頻圖。|
|`view_waveforms.py`|開啟同一互動介面的簡短入口。|
|`test_resampling.py`|驗證資料型別、量化裁切、振幅／混疊、WAV 格式、CSV 一致性、圖片與來源雜湊。|
|`test_interactive_viewer.py`|測試互動介面的音檔切換、方法顯示、選區放大及分析模式。|
|`requirements.txt`|列出環境需要的 Python 套件。|

## 測試訊號與設定

- `audio_20261029180204.wav`：真實錄音，執行前檢查取樣率、聲道、PCM subtype、樣本數及長度，前後核對 SHA256。
- `synthetic_test.wav`：五秒；0–0.6 s 1 kHz、0.6–1.2 s 6 kHz、1.2–1.8 s 7.4 kHz、1.8–2.4 s 10 kHz、2.4–3.0 s 15 kHz、3.0–4.6 s 線性 200 Hz→20 kHz chirp、4.6–5.0 s 靜音中於 4.8 s 放單樣本脈衝。單音振幅 18000、脈衝 24000 PCM counts；音段淡入淡出各 10 ms。已有 synthetic 檔案應符合此段落定義，程式不會覆寫其內容。
- 讀檔固定 `soundfile.read(dtype='int16')`，不除以 32768、不正規化。必要轉型保留 PCM 尺度，輸出使用四捨五入（ties-to-even）及飽和裁切。
- 通帶與混疊以單音段前後排除 80 ms 的最小平方振幅估計，量測 PCM_16 最終輸出。小於一 PCM count 標記報告下限，不將此值視為真正阻帶性能。CSV 保留未截斷的振幅估計及下限旗標。
- 頻率掃描於記憶體產生 0.2 s 單音（500–20000 Hz，主間距 250 Hz，過渡帶補 100 Hz），取中央 0.1 s 分析；跳過精確 8/16 kHz 的退化取樣點。8 kHz 以上量測的是折返分量。
- FFT 採整段週期假設；Linear/Cubic 不額外濾波。Polyphase 固定 160/441，SciPy 預設 Kaiser beta=5、constant padding。函式庫邊界延拓不同，屬演算法設定；本程式不事後裁切或補零修正輸出長度。
- 五秒輸出須為 80000 samples。不整除的輸入長度若各函式庫原生長度不同，明確報錯；不暗中補齊。
- PSD：Hann 40 ms、50% overlap、density、無去趨勢；spectrogram：Hann 40 ms、hop 約 10 ms，單位 dB re 1 PCM²/Hz、每組六面板共用色階。44.1 kHz 的 hop 為 441 samples，16 kHz 為 160 samples。
- 計時包含必要轉型、降取樣、輸出量化與檢查；每組預熱 3 次、正式 30 次，排除檔案與繪圖。報告僅適用本機離線處理，不推論 Pi 的串流延遲。

## 結果

`results/five_methods/` 包含：

- `synthetic/wav/` 與 `recorded/wav/`：各五個 PCM_16 WAV。
- `synthetic/plots/`：01 完整波形、02 低／高頻 stem、03 通帶、04 混疊、05 頻率掃描、06 chirp 時頻、07 脈衝。
- `recorded/plots/`：01 完整波形、02 兩個局部 stem、08 PSD（含原始全頻）、09 時頻圖。
- `10_runtime.png`：兩音檔的五方法速度圖；所有 PNG 至少 200 DPI。
- `csv/`：passband、aliasing、frequency_response、runtime、runtime_samples、summary、recorded_psd，UTF-8 BOM 可用 Excel 開啟。
- `metadata.json`：套件版本、來源雜湊、輸入規格及測試設定。
- `experiment_report.md`：實際數據產生的結論、限制及 Raspberry Pi Zero 2 W 部署建議。

## 互動介面

上方切換人工／真實音檔，勾選原音及五方法。滾輪縮放、拖曳平移，總覽圖拖曳選取區間後按 Zoom selection；Full duration 復原全段。放大到 30 ms 以下顯示真實取樣點（原始圓點、輸出叉點），游標列出時間、PCM 振幅及各方法最近樣本的實際時間。下方切換疊加 PSD 或選定方法的 spectrogram，色階使用該音檔六種訊號共同範圍；分析以整段檔案為準。

## 原專案狀態

本次開始時資料夾只有原始錄音、`.venv/`、`results/` 及 `__pycache__/compare_resampling.cpython-314.pyc`，附件所列 Python 原始碼與 README 未在磁碟中。因此本次新增這些檔案，沒有可執行的舊原始碼／舊測試可供回歸比較；既有目錄與快取均保留。
