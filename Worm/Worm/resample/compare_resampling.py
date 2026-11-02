"""Run the complete five-method experiment using measured PCM_16 outputs."""
import argparse
import csv
import importlib.metadata
import json
import platform
import sys
from datetime import datetime, timezone, timedelta
from time import perf_counter_ns
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf
from audio_core import (ROOT, RESULTS, METHODS, SETTINGS, FS_IN, FS_OUT, SEGMENTS,
                        AMPLITUDE, IMPULSE_TIME, read_input, convert, raw_resample,
                        pcm16, measure, psd, spectrogram, db_power, sha256)
from generate_test_audio import generate, fade

COLORS = dict(zip(('Original',)+METHODS, plt.get_cmap('tab10').colors[:6]))


def write_csv(path, rows):
    with path.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def save(fig, path):
    fig.savefig(path, dpi=220, bbox_inches='tight')
    plt.close(fig)


def channels(x, outputs):
    return [('Original', x, FS_IN)]+[(m, outputs[m], FS_OUT) for m in METHODS]


def waveform_plots(kind, x, outputs, directory):
    data = channels(x, outputs)
    limit = 1.05*max(max(abs(a.astype(float))) for _, a, _ in data)
    fig, axes = plt.subplots(6, 1, figsize=(12, 10), sharex=True, sharey=True)
    for ax, (name, a, fs) in zip(axes, data):
        ax.plot(np.arange(len(a))/fs, a, lw=.45, color=COLORS[name], label=name)
        ax.set(ylabel='PCM Amplitude', ylim=(-limit, limit))
        ax.legend(loc='upper right')
    axes[-1].set_xlabel('Time (s)')
    fig.suptitle(f'01 | {kind}: full waveform, physical sample times')
    fig.tight_layout()
    save(fig, directory/'01_full_waveform.png')
    # For the recorded file these are time selections, not assumed tone labels.
    for label, start in [('low', .25), ('high', 1.45)]:
        fig, axes = plt.subplots(5, 1, figsize=(12, 10), sharex=True, sharey=True)
        end = start+.002
        for ax, m in zip(axes, METHODS):
            for name, a, fs, marker in [('Original', x, FS_IN, 'o'), (m, outputs[m], FS_OUT, 'x')]:
                idx = np.arange(int(start*fs), int(end*fs))
                t = idx/fs
                ax.vlines(t, 0, a[idx], color=COLORS[name], alpha=.35)
                ax.plot(t, a[idx], marker=marker, ms=4, lw=.7,
                        color=COLORS[name], label=f'{name} ({fs} Hz)')
            ax.set_ylabel('PCM Amplitude')
            ax.legend(loc='upper right')
        axes[-1].set_xlabel('Time (s)')
        subtitle = ('1 kHz' if label == 'low' else '7.4 kHz') if kind == 'synthetic' else 'recording time selection'
        fig.suptitle(f'02 | {kind}: {subtitle}; original circles / output crosses')
        fig.tight_layout()
        save(fig, directory/f'02_stem_{label}.png')


def grouped(rows, frequencies, title, path):
    fig, ax = plt.subplots(figsize=(11, 5))
    for j, m in enumerate(METHODS):
        rr = [next(r for r in rows if r['method'] == m and r['input_frequency_hz'] == f) for f in frequencies]
        pos = np.arange(len(frequencies))+(j-2)*.15
        ax.bar(pos, [r['relative_db'] for r in rr], .15, label=m, color=COLORS[m])
        for p, r in zip(pos, rr):
            if r['below_floor']:
                ax.text(p, r['relative_db'], '≤ floor', rotation=90, fontsize=7, va='bottom', ha='center')
    ax.set(xticks=np.arange(len(frequencies)), xticklabels=[f'{f/1000:g}' for f in frequencies],
           xlabel='Input Frequency (kHz)', ylabel='Relative Amplitude (dB)')
    fig.suptitle(title)
    ax.axhline(0, color='black', lw=.6)
    ax.legend(ncol=5, loc='lower center', bbox_to_anchor=(.5, 1.01))
    fig.text(.1, .01, 'Measured after PCM_16 quantization. Reporting floor: 1 PCM count relative to measured input.')
    fig.tight_layout(rect=(0, .04, 1, .94))
    save(fig, path)


def spectrogram_plot(x, outputs, start, end, title, path):
    fig, axes = plt.subplots(6, 1, figsize=(12, 12), sharex=True, sharey=True)
    specs = []
    for name, a, fs in channels(x, outputs):
        f, t, p = spectrogram(a[round(start*fs):round(end*fs)], fs)
        specs.append((name, f, t+start, db_power(p)))
    # One common absolute PCM^2/Hz scale across all panels in this figure.
    vmax = max(float(z.max()) for _, _, _, z in specs)
    vmin = vmax-100
    for ax, (name, f, t, z) in zip(axes, specs):
        mesh = ax.pcolormesh(t, f/1000, z, shading='auto', vmin=vmin, vmax=vmax, cmap='magma')
        ax.set(ylim=(0, 8), ylabel='Frequency (kHz)', xlim=(start, end))
        ax.text(.01, .82, name, color='white', transform=ax.transAxes)
    axes[-1].set_xlabel('Time (s)')
    fig.suptitle(title+'\nHann 40 ms; hop 10 ms; shared color scale; view 0–8 kHz')
    fig.subplots_adjust(top=.92, bottom=.06, left=.1, right=.86, hspace=.15)
    fig.colorbar(mesh, ax=axes.tolist(), cax=fig.add_axes((.89, .12, .02, .73)), label='PSD (dB re 1 PCM²/Hz)')
    save(fig, path)


def impulse_plot(x, outputs, directory):
    fig, axes = plt.subplots(6, 1, figsize=(12, 10), sharex=True, sharey=True)
    for ax, (name, a, fs) in zip(axes, channels(x, outputs)):
        i = np.arange(round((IMPULSE_TIME-.002)*fs), round((IMPULSE_TIME+.002)*fs))
        ax.plot((i/fs-IMPULSE_TIME)*1000, a[i], '.-', ms=3, lw=.8, color=COLORS[name], label=name)
        ax.axvline(0, color='gray', ls='--', lw=.6)
        ax.set_ylabel('PCM Amplitude')
        ax.legend()
    axes[-1].set_xlabel('Time relative to impulse (ms)')
    fig.suptitle('07 | Impulse: peak height alone is not a quality metric')
    fig.tight_layout()
    save(fig, directory/'07_impulse.png')


def spectrum_plot(x, outputs, directory):
    fig, axes = plt.subplots(2, 1, figsize=(12, 8))
    rows = []
    for name, a, fs in channels(x, outputs):
        f, p = psd(a, fs)
        axes[0].plot(f/1000, db_power(p), label=name, color=COLORS[name], lw=.9)
        df = f[1]-f[0]
        rows.append(dict(method=name, power_0_8k_pcm2=float(np.sum(p[f<8000])*df),
                         power_above_8k_pcm2=float(np.sum(p[f>=8000])*df)))
        if name == 'Original':
            axes[1].plot(f/1000, db_power(p), label='Original 44.1 kHz', color=COLORS[name])
    axes[0].set_xlim(0, 8)
    axes[1].axvline(8, color='red', ls='--', label='Output Nyquist: 8 kHz')
    axes[1].set_xlim(0, FS_IN/2000)
    for ax in axes:
        ax.set(xlabel='Frequency (kHz)', ylabel='PSD (dB re 1 PCM²/Hz)')
        ax.legend()
        ax.grid(alpha=.25)
    fig.suptitle('08 | Recording PSD: Hann 40 ms, 50% overlap, density, no detrending')
    fig.tight_layout()
    save(fig, directory/'08_recorded_psd.png')
    return rows


def benchmark(x, kind, repeats):
    rows, samples = [], []
    for m in METHODS:
        for _ in range(3):
            convert(x, m)
        times = []
        for trial in range(repeats):
            before = perf_counter_ns()
            convert(x, m)
            elapsed = (perf_counter_ns()-before)/1e6
            times.append(elapsed)
            samples.append(dict(dataset=kind, method=m, trial=trial+1, elapsed_ms=elapsed))
        native = raw_resample(x, m, native=True)
        rows.append(dict(dataset=kind, method=m, mean_ms=np.mean(times), median_ms=np.median(times),
                         std_ms=np.std(times, ddof=1), p95_ms=np.percentile(times, 95),
                         rtf=np.mean(times)/1000/(len(x)/FS_IN), repeats=repeats, warmups=3,
                         input_dtype=str(x.dtype), algorithm_input_dtype=SETTINGS[m][0],
                         native_output_dtype=str(native.dtype), resampling_output_dtype=SETTINGS[m][1], output_dtype='int16',
                         internal_precision=('libsoxr implementation; not inferred from int16 API' if m=='SoXR_HQ'
                                             else ('float64 native interpolation; cast to float32' if m in ('Linear','Cubic')
                                                   else SETTINGS[m][1]+' arrays; library intermediates not instrumented'))))
    return rows, samples


def report(root, metadata, passband, aliasing, runtimes, summaries, spectral):
    lines = ['# 音訊降取樣五種方法比較實驗', '',
             '## 實驗目的與環境',
             '以人工訊號及真實錄音，實測 44.1 kHz → 16 kHz、mono、PCM_16 的振幅保留、混疊及離線運算時間。',
             f"平台：{metadata['platform']}；Python：{metadata['python']}。",
             '套件版本：'+json.dumps(metadata['versions'], ensure_ascii=False),
             '完整輸入規格、SHA256、段落與量測設定見 metadata.json。未對 PCM 做音量正規化。', '',
             '## 方法設定與資料型別']
    for m in METHODS:
        lines.append(f'- {m}：{SETTINGS[m][2]}；包裝函式輸入／輸出介面 {SETTINGS[m][0]} / {SETTINGS[m][1]}，檔案輸出 int16。')
    lines += ['SoXR 的 int16 介面不代表其內部以整數運算；內部精度未量測。其他方法記錄的是可觀察陣列型別，並非所有底層暫存器的精度。',
              'Linear 與 Cubic 雖以 float32 音訊輸入，指定的 numpy.interp / CubicSpline 仍產生 float64 原生結果（CubicSpline 係數亦為 float64），本程式再轉回 float32。時間座標及獨立品質量測保留 float64；不能稱為全程純 float32。此轉型包含於計時。',
              '浮點輸出以 np.rint（ties-to-even）四捨五入後飽和裁切。SoXR 已由函式庫執行 int16 量化。', '',
              '## 人工訊號量測',
              '每段前後排除 80 ms（淡入淡出各 10 ms）；以正弦、餘弦及 DC 最小平方擬合估計振幅。所有品質指標基於實際 PCM_16 輸出。',
              '報告下限採 1 PCM count / 輸入振幅，約 -85 dB。低於此值只列為 ≤ 下限，不能解讀為濾波器真正阻帶極限。這是保守報告門檻，不是統計信賴區間。',
              '|方法|1 kHz (dB)|6 kHz (dB)|7.4 kHz (dB)|10→6 kHz (dB)|15→1 kHz (dB)|',
              '|---|---:|---:|---:|---:|---:|']
    for m in METHODS:
        rr = [r for r in passband+aliasing if r['method']==m]
        vals = [('≤ ' if r['below_floor'] else '')+f"{r['relative_db']:.2f}" for r in rr]
        lines.append('|'+m+'|'+'|'.join(vals)+'|')
    for m in METHODS:
        aa = [r for r in aliasing if r['method']==m]
        pp = [r for r in passband if r['method']==m]
        lines.append(f"- {m}：三個通帶點最大絕對偏離 {max(abs(r['relative_db']) for r in pp):.2f} dB；兩個混疊點中較大的折返分量為 {max(r['relative_db'] for r in aa):.2f} dB（受報告下限限制）。")
    lines += ['7.4 kHz 接近輸出 Nyquist；其衰減與濾波器過渡帶有關，不能單獨認定為實作錯誤。', '',
              '## 真實錄音',
              'Welch PSD 使用 Hann 40 ms 視窗、50% overlap、density、無去趨勢。不同取樣率的窗長以相同秒數設定。']
    orig = spectral[0]
    fraction = orig['power_above_8k_pcm2']/(orig['power_0_8k_pcm2']+orig['power_above_8k_pcm2'])
    lines.append(f'原始錄音估計有 {100*fraction:.4f}% 頻譜功率位於 8 kHz 以上（Welch 網格分界，含頻譜洩漏）。')
    for row in spectral:
        lines.append(f"- {row['method']}：0–8 kHz 積分 PSD = {row['power_0_8k_pcm2']:.4f} PCM²。")
    lines += ['真實錄音沒有理想 16 kHz 真值；頻譜差異無法獨立證明聽覺優劣，也不能區分所有低頻原有成分與折返成分。', '',
              '## 運算時間',
              '每方法每音檔預熱 3 次、正式至少 30 次。計時涵蓋轉型、降取樣、有限值檢查、捨入、裁切及長度檢查，排除讀寫與繪圖。順序固定，背景負載與快取可能影響結果；不代表即時串流延遲。',
              '|音檔|方法|Mean ms|Median ms|Std ms|P95 ms|RTF|',
              '|---|---|---:|---:|---:|---:|---:|']
    for r in runtimes:
        lines.append(f"|{r['dataset']}|{r['method']}|{r['mean_ms']:.3f}|{r['median_ms']:.3f}|{r['std_ms']:.3f}|{r['p95_ms']:.3f}|{r['rtf']:.6f}|")
    for kind in ('synthetic', 'recorded'):
        best = min((r for r in runtimes if r['dataset']==kind), key=lambda r:r['median_ms'])
        lines.append(f"本次 {kind} 中位數最快為 {best['method']}（{best['median_ms']:.3f} ms）；此結論僅限本機與本次量測。")
    lines += ['', '## 圖表解讀',
              '1. 完整波形：共同時間與振幅尺度；可看大尺度變化，不能單靠縮圖判定高頻品質。',
              '2. 局部 stem：人工 1 kHz、7.4 kHz 段；圓點為原樣本，叉點為輸出。錄音同時段不假定為單頻。',
              '3. 通帶：接近 0 dB 表示單頻振幅保留，截止附近需考慮過渡帶。',
              '4. 混疊：10→6、15→1 kHz；越負表示這兩個測試點的折返分量越小，≤ floor 不是精確抑制值。',
              '5. 頻率掃描：500–20000 Hz 的獨立單音；8 kHz 以上量測折返頻率，不是正常通帶。精確 DC/Nyquist 折返點不採樣，避免相位退化。',
              '6. Chirp：共用 0–8 kHz 及色階；原始超過 8 kHz 的軌跡離開視窗，輸出若折返會重新出現在低頻。',
              '7. 脈衝：原始脈衝置於 4.8 s（兩個網格皆對齊），比較振鈴、展寬及時間位置；峰值大小不是品質排名。',
              '8. 真實錄音 PSD：上圖共同 0–8 kHz，下圖原始全頻帶，單位為 dB re 1 PCM²/Hz。',
              '9. 真實錄音時頻圖：Hann 40 ms、hop 10 ms、共同絕對色階；觀察時間特徵及頻帶變化。',
              '10. 執行時間：兩音檔各方法平均值與標準差誤差棒，精確中位數及 P95 見 CSV。', '',
              '## 優勢、限制與部署建議',
              '- Linear、Cubic 是未加抗混疊濾波的插值基準；不能只因速度快或波形貼近原樣本就認定適合降取樣。',
              '- FFT 使用整段週期延拓，長度與邊界會影響暫態及成本；這不是串流實作。',
              '- Polyphase 使用 SciPy 預設 Kaiser FIR，保留可重現的 160/441 比例；截止附近衰減需配合需求評估。',
              '- SoXR HQ 使用函式庫高品質設定，int16 介面方便；此處不推論其未公開的中間資料型別，也不預先宣稱它全面勝出。',
              '- 本次未執行主觀聆聽、串流分塊、能耗或跨平台測試；單頻振幅與 PSD 不足以完整排名音質。',
              '- Raspberry Pi Zero 2 W 應在目標裝置重跑，固定電源／散熱／CPU governor，量測峰值記憶體、P95、串流緩衝延遲與長時間穩定性。桌面 RTF 不可直接推算 ARM 效能。',
              '- 依可接受的混疊與通帶誤差先篩選，再用目標機實測速度選擇；如需串流應另測具狀態的分塊 API。', '',
              '## 驗證與重現',
              '所有 WAV 在寫入後重新讀取，確認 16 kHz / mono / PCM_16 / 正確樣本數，並與記憶體輸出完全一致。沒有為掩蓋錯誤而裁切或補零。',
              '來源錄音在流程前後比較 SHA256。CSV 與圖表共用同一組記憶體量測值。runtime_samples.csv 保存全部計時樣本。',
              '結果限於本次輸入、套件版本及硬體；精確環境與來源雜湊見 metadata.json。']
    formatted = []
    for i, line in enumerate(lines):
        previous = lines[i-1] if i else ''
        if previous and (previous.startswith('#') or
                         (line.startswith('|') and not previous.startswith('|')) or
                         (previous.startswith('|') and not line.startswith('|')) or
                         (line.startswith('- ') and not previous.startswith('- '))):
            formatted.append('')
        formatted.append(line)
    (root/'experiment_report.md').write_text('\n'.join(formatted)+'\n', encoding='utf-8')


def run(repeats=30):
    if repeats < 30:
        raise ValueError('At least 30 timed trials required')
    generate()
    csvdir = RESULTS/'csv'
    csvdir.mkdir(parents=True, exist_ok=True)
    datasets, metadata = {}, dict(platform=platform.platform(), processor=platform.processor(),
                                  python=sys.version, versions={p:importlib.metadata.version(p) for p in
                                  ['numpy','scipy','soundfile','soxr','matplotlib','pyqtgraph','PySide6-Essentials']}, inputs={})
    metadata['run_time_asia_taipei'] = datetime.now(timezone(timedelta(hours=8))).isoformat()
    metadata['synthetic_segments'] = [dict(start_s=a, end_s=b, frequency_hz=f) for a,b,f in SEGMENTS]+[
        dict(start_s=3, end_s=4.6, chirp_start_hz=200, chirp_end_hz=20000),
        dict(start_s=4.6, end_s=5, impulse_time_s=IMPULSE_TIME, impulse_amplitude=24000)]
    metadata.update(tone_amplitude_pcm=AMPLITUDE, fade_ms=10, tone_boundary_exclusion_ms=80,
                    amplitude_floor_pcm=1, psd_window_ms=40, spectrogram_hop_ms=10,
                    quality_measurement='quantized PCM_16 outputs', methods=SETTINGS)
    runtime, timings, summary = [], [], []
    for kind, path in [('synthetic', ROOT/'synthetic_test.wav'), ('recorded', ROOT/'audio_20261029180204.wav')]:
        x, info = read_input(path)
        if kind=='synthetic' and len(x)!=5*FS_IN:
            raise ValueError('Synthetic test must be exactly 5 seconds')
        metadata['inputs'][kind] = info
        outdir = RESULTS/kind
        for sub in ('wav', 'plots'):
            (outdir/sub).mkdir(parents=True, exist_ok=True)
        outputs = {}
        for m in METHODS:
            y = convert(x, m)
            outputs[m] = y
            wav = outdir/'wav'/f'{m}.wav'
            sf.write(wav, y, FS_OUT, subtype='PCM_16')
            check = sf.info(wav)
            reread, fs = sf.read(wav, dtype='int16')
            if (fs, check.channels, check.subtype, len(reread)) != (FS_OUT, 1, 'PCM_16', round(len(x)*FS_OUT/FS_IN)) or not np.array_equal(y, reread):
                raise AssertionError(f'Output verification failed: {wav}')
            summary.append(dict(dataset=kind, method=m, samples=len(y), samplerate=fs, channels=1,
                                subtype=check.subtype, duration_s=len(y)/fs,
                                peak_pcm=int(np.max(np.abs(y.astype(np.int32)))),
                                rail_sample_count=int(np.count_nonzero((y==-32768)|(y==32767)))))
        datasets[kind] = (x, outputs)
        rr, ss = benchmark(x, kind, repeats)
        runtime.extend(rr)
        timings.extend(ss)
        waveform_plots(kind, x, outputs, outdir/'plots')
        print(f'{kind}: verified five WAVs and waveform plots', flush=True)
    x, outputs = datasets['synthetic']
    passband, aliasing = [], []
    for start, end, frequency in SEGMENTS:
        for m in METHODS:
            row = dict(method=m, input_frequency_hz=frequency, start_s=start+.08, end_s=end-.08,
                       **measure(x, outputs[m], frequency, start+.08, end-.08))
            (passband if frequency<8000 else aliasing).append(row)
    # Keep report columns grouped by frequency in their original order.
    sp = RESULTS/'synthetic'/'plots'
    grouped(passband, [1000,6000,7400], '03 | Passband amplitude retention', sp/'03_passband.png')
    grouped(aliasing, [10000,15000], '04 | Aliased amplitude: 10→6 kHz and 15→1 kHz', sp/'04_aliasing.png')
    response = []
    frequencies = np.unique(np.r_[np.arange(500,20001,250), np.arange(7100,8901,100)])
    # Exact multiples of output Nyquist have phase-degenerate samples.
    frequencies = frequencies[frequencies % 8000 != 0]
    for frequency in frequencies:
        tone = pcm16(fade(AMPLITUDE*np.sin(2*np.pi*frequency*np.arange(FS_IN//5)/FS_IN)))
        for m in METHODS:
            response.append(dict(method=m, input_frequency_hz=int(frequency),
                                 **measure(tone, convert(tone,m), frequency, .05, .15)))
    fig, ax = plt.subplots(figsize=(12,6))
    for m in METHODS:
        rr = [r for r in response if r['method']==m]
        ax.plot([r['input_frequency_hz']/1000 for r in rr], [r['relative_db'] for r in rr], label=m)
    ax.axvline(8, color='black', ls='--', label='Output Nyquist')
    ax.axvspan(8,20, color='gray', alpha=.1)
    ax.set(xlabel='Input Frequency (kHz)', ylabel='Relative Amplitude (dB)',
           title='05 | Single-tone response: above 8 kHz = folded component, not retained bandwidth')
    ax.text(.02, .04, 'PCM_16 outputs; conservative 1-count floor; exact DC/Nyquist folds omitted', transform=ax.transAxes)
    ax.legend()
    ax.grid(alpha=.25)
    save(fig, sp/'05_frequency_response.png')
    spectrogram_plot(x, outputs, 3, 4.6, '06 | Synthetic chirp, 200 Hz → 20 kHz', sp/'06_chirp_spectrogram.png')
    impulse_plot(x, outputs, sp)
    x, outputs = datasets['recorded']
    rp = RESULTS/'recorded'/'plots'
    spectral = spectrum_plot(x, outputs, rp)
    spectrogram_plot(x, outputs, 0, len(x)/FS_IN, '09 | Recorded audio', rp/'09_recorded_spectrogram.png')
    fig, ax = plt.subplots(figsize=(11,5))
    for j, m in enumerate(METHODS):
        rr = [r for r in runtime if r['method']==m]
        ax.bar(np.arange(2)+(j-2)*.15, [r['mean_ms'] for r in rr], .15,
               yerr=[r['std_ms'] for r in rr], capsize=2, label=m)
    ax.set(xticks=[0,1], xticklabels=['Synthetic','Recorded'], xlabel='Dataset',
           ylabel='Elapsed time (ms)', title=f'10 | Conversion runtime: mean ± sample SD, {repeats} trials after 3 warmups')
    ax.legend()
    save(fig, RESULTS/'10_runtime.png')
    for filename, rows in [('passband',passband),('aliasing',aliasing),('frequency_response',response),
                            ('runtime',runtime),('runtime_samples',timings),('summary',summary),('recorded_psd',spectral)]:
        write_csv(csvdir/f'{filename}.csv', rows)
    for info in metadata['inputs'].values():
        if sha256(info['path']) != info['sha256']:
            raise AssertionError('Source changed during experiment')
    (RESULTS/'metadata.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding='utf-8')
    report(RESULTS, metadata, passband, aliasing, runtime, summary, spectral)
    print(f'Complete: {RESULTS}', flush=True)


if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repeats', type=int, default=30)
    run(parser.parse_args().repeats)
