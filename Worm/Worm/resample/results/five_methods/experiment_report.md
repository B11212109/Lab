# 音訊降取樣五種方法比較實驗


## 實驗目的與環境

以人工訊號及真實錄音，實測 44.1 kHz → 16 kHz、mono、PCM_16 的振幅保留、混疊及離線運算時間。
平台：Windows-11-10.0.26200-SP0；Python：3.14.7 (main, Aug 14 2026, 15:39:58) [MSC v.1944 64 bit (AMD64)]。
套件版本：{"numpy": "2.5.4", "scipy": "1.18.1", "soundfile": "0.14.0", "soxr": "1.1.0", "matplotlib": "3.11.2", "pyqtgraph": "0.14.0", "PySide6-Essentials": "6.12.0"}
完整輸入規格、SHA256、段落與量測設定見 metadata.json。未對 PCM 做音量正規化。

## 方法設定與資料型別

- Linear：numpy.interp; native float64 result cast to float32; no anti-alias filter；包裝函式輸入／輸出介面 float32 / float32，檔案輸出 int16。
- FFT：scipy.signal.resample; window=None; periodic extension；包裝函式輸入／輸出介面 float32 / float32，檔案輸出 int16。
- Cubic：CubicSpline; native float64 coefficients/result cast to float32; not-a-knot; no anti-alias filter；包裝函式輸入／輸出介面 float32 / float32，檔案輸出 int16。
- Polyphase：resample_poly; up=160 down=441; window=(kaiser,5.0); padtype=constant；包裝函式輸入／輸出介面 float32 / float32，檔案輸出 int16。
- SoXR_HQ：soxr.resample; quality=HQ; libsoxr internal precision not inferred from interface；包裝函式輸入／輸出介面 int16 / int16，檔案輸出 int16。
SoXR 的 int16 介面不代表其內部以整數運算；內部精度未量測。其他方法記錄的是可觀察陣列型別，並非所有底層暫存器的精度。
Linear 與 Cubic 雖以 float32 音訊輸入，指定的 numpy.interp / CubicSpline 仍產生 float64 原生結果（CubicSpline 係數亦為 float64），本程式再轉回 float32。時間座標及獨立品質量測保留 float64；不能稱為全程純 float32。此轉型包含於計時。
浮點輸出以 np.rint（ties-to-even）四捨五入後飽和裁切。SoXR 已由函式庫執行 int16 量化。

## 人工訊號量測

每段前後排除 80 ms（淡入淡出各 10 ms）；以正弦、餘弦及 DC 最小平方擬合估計振幅。所有品質指標基於實際 PCM_16 輸出。
報告下限採 1 PCM count / 輸入振幅，約 -85 dB。低於此值只列為 ≤ 下限，不能解讀為濾波器真正阻帶極限。這是保守報告門檻，不是統計信賴區間。

|方法|1 kHz (dB)|6 kHz (dB)|7.4 kHz (dB)|10→6 kHz (dB)|15→1 kHz (dB)|
|---|---:|---:|---:|---:|---:|
|Linear|-0.01|-0.53|-0.81|-1.50|-3.44|
|FFT|0.00|-0.00|0.00|≤ -85.11|≤ -85.11|
|Cubic|-0.00|-0.01|-0.02|-0.08|-0.65|
|Polyphase|0.01|-0.01|-1.36|-57.26|≤ -85.11|
|SoXR_HQ|0.00|-0.00|-0.02|≤ -85.11|≤ -85.11|

- Linear：三個通帶點最大絕對偏離 0.81 dB；兩個混疊點中較大的折返分量為 -1.50 dB（受報告下限限制）。
- FFT：三個通帶點最大絕對偏離 0.00 dB；兩個混疊點中較大的折返分量為 -85.11 dB（受報告下限限制）。
- Cubic：三個通帶點最大絕對偏離 0.02 dB；兩個混疊點中較大的折返分量為 -0.08 dB（受報告下限限制）。
- Polyphase：三個通帶點最大絕對偏離 1.36 dB；兩個混疊點中較大的折返分量為 -57.26 dB（受報告下限限制）。
- SoXR_HQ：三個通帶點最大絕對偏離 0.02 dB；兩個混疊點中較大的折返分量為 -85.11 dB（受報告下限限制）。
7.4 kHz 接近輸出 Nyquist；其衰減與濾波器過渡帶有關，不能單獨認定為實作錯誤。

## 真實錄音

Welch PSD 使用 Hann 40 ms 視窗、50% overlap、density、無去趨勢。不同取樣率的窗長以相同秒數設定。
原始錄音估計有 0.4350% 頻譜功率位於 8 kHz 以上（Welch 網格分界，含頻譜洩漏）。

- Original：0–8 kHz 積分 PSD = 326286.1056 PCM²。
- Linear：0–8 kHz 積分 PSD = 326511.8422 PCM²。
- FFT：0–8 kHz 積分 PSD = 326286.3995 PCM²。
- Cubic：0–8 kHz 積分 PSD = 327068.2595 PCM²。
- Polyphase：0–8 kHz 積分 PSD = 326710.7408 PCM²。
- SoXR_HQ：0–8 kHz 積分 PSD = 326286.0258 PCM²。
真實錄音沒有理想 16 kHz 真值；頻譜差異無法獨立證明聽覺優劣，也不能區分所有低頻原有成分與折返成分。

## 運算時間

每方法每音檔預熱 3 次、正式至少 30 次。計時涵蓋轉型、降取樣、有限值檢查、捨入、裁切及長度檢查，排除讀寫與繪圖。順序固定，背景負載與快取可能影響結果；不代表即時串流延遲。

|音檔|方法|Mean ms|Median ms|Std ms|P95 ms|RTF|
|---|---|---:|---:|---:|---:|---:|
|synthetic|Linear|2.599|2.595|0.019|2.638|0.000520|
|synthetic|FFT|2.096|2.052|0.157|2.317|0.000419|
|synthetic|Cubic|20.406|20.025|1.997|22.770|0.004081|
|synthetic|Polyphase|2.217|2.188|0.090|2.373|0.000443|
|synthetic|SoXR_HQ|0.866|0.850|0.048|0.971|0.000173|
|recorded|Linear|2.526|2.510|0.114|2.743|0.000505|
|recorded|FFT|2.434|2.333|0.328|3.048|0.000487|
|recorded|Cubic|18.576|18.560|0.315|19.162|0.003715|
|recorded|Polyphase|2.257|2.230|0.084|2.384|0.000451|
|recorded|SoXR_HQ|0.924|0.913|0.050|1.011|0.000185|

本次 synthetic 中位數最快為 SoXR_HQ（0.850 ms）；此結論僅限本機與本次量測。
本次 recorded 中位數最快為 SoXR_HQ（0.913 ms）；此結論僅限本機與本次量測。

## 圖表解讀

1. 完整波形：共同時間與振幅尺度；可看大尺度變化，不能單靠縮圖判定高頻品質。
2. 局部 stem：人工 1 kHz、7.4 kHz 段；圓點為原樣本，叉點為輸出。錄音同時段不假定為單頻。
3. 通帶：接近 0 dB 表示單頻振幅保留，截止附近需考慮過渡帶。
4. 混疊：10→6、15→1 kHz；越負表示這兩個測試點的折返分量越小，≤ floor 不是精確抑制值。
5. 頻率掃描：500–20000 Hz 的獨立單音；8 kHz 以上量測折返頻率，不是正常通帶。精確 DC/Nyquist 折返點不採樣，避免相位退化。
6. Chirp：共用 0–8 kHz 及色階；原始超過 8 kHz 的軌跡離開視窗，輸出若折返會重新出現在低頻。
7. 脈衝：原始脈衝置於 4.8 s（兩個網格皆對齊），比較振鈴、展寬及時間位置；峰值大小不是品質排名。
8. 真實錄音 PSD：上圖共同 0–8 kHz，下圖原始全頻帶，單位為 dB re 1 PCM²/Hz。
9. 真實錄音時頻圖：Hann 40 ms、hop 10 ms、共同絕對色階；觀察時間特徵及頻帶變化。
10. 執行時間：兩音檔各方法平均值與標準差誤差棒，精確中位數及 P95 見 CSV。

## 優勢、限制與部署建議

- Linear、Cubic 是未加抗混疊濾波的插值基準；不能只因速度快或波形貼近原樣本就認定適合降取樣。
- FFT 使用整段週期延拓，長度與邊界會影響暫態及成本；這不是串流實作。
- Polyphase 使用 SciPy 預設 Kaiser FIR，保留可重現的 160/441 比例；截止附近衰減需配合需求評估。
- SoXR HQ 使用函式庫高品質設定，int16 介面方便；此處不推論其未公開的中間資料型別，也不預先宣稱它全面勝出。
- 本次未執行主觀聆聽、串流分塊、能耗或跨平台測試；單頻振幅與 PSD 不足以完整排名音質。
- Raspberry Pi Zero 2 W 應在目標裝置重跑，固定電源／散熱／CPU governor，量測峰值記憶體、P95、串流緩衝延遲與長時間穩定性。桌面 RTF 不可直接推算 ARM 效能。
- 依可接受的混疊與通帶誤差先篩選，再用目標機實測速度選擇；如需串流應另測具狀態的分塊 API。

## 驗證與重現

所有 WAV 在寫入後重新讀取，確認 16 kHz / mono / PCM_16 / 正確樣本數，並與記憶體輸出完全一致。沒有為掩蓋錯誤而裁切或補零。
來源錄音在流程前後比較 SHA256。CSV 與圖表共用同一組記憶體量測值。runtime_samples.csv 保存全部計時樣本。
結果限於本次輸入、套件版本及硬體；精確環境與來源雜湊見 metadata.json。
