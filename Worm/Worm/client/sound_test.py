import sys
import numpy as np
import sounddevice as sd

# ===== 參數設定 =====
DEVICE_ID = 0  # 填入第三步查到的 USB 音效卡編號；若設為 None 則使用系統預設
SAMPLE_RATE = 44100  # 取樣率（常見支援：16000, 44100, 48000）
CHANNELS = 1  # 聲道數（USB 麥克風通常為 1 (單聲道) 或 2 (雙聲道)）
BLOCK_SIZE = 65536  # 每次回呼處理的取樣點數（越小延遲越低，但 CPU 負擔略增）


def audio_callback(indata, frames, time_info, status):
    """
    音效卡每蒐集到 BLOCK_SIZE 個音訊樣本，就會自動呼叫此函式一次
    indata: NumPy 陣列，形狀為 (frames, channels)，資料型態預設為 float32 (-1.0 ~ 1.0)
    """
    if status:
        print(f"狀態警示: {status}", file=sys.stderr)

    # 1. 取得音訊數據 (NumPy Array)
    audio_data = indata[:, 0]  # 若為單聲道取第 0 軌

    # 2. 範例：計算即時音量 (RMS - Root Mean Square)
    rms = np.sqrt(np.mean(audio_data**2))

    # 用簡單的文字條印出音量強度
    volume_meter = "#" * int(rms * 100)
    print(f"\r當前音量: [{volume_meter:<50}] {rms:.4f}", end="")

    # 【在這裡加入你的處理邏輯】
    # 例如：傳入即時語音辨識、存入 Queue、偵測噪音門檻觸發拍照等


def main():
    print("開始持續監聽 USB 音訊... 按 Ctrl+C 結束。")
    try:
        # 建立非同步輸入音訊串流
        with sd.InputStream(
            device=DEVICE_ID,
            channels=CHANNELS,
            samplerate=SAMPLE_RATE,
            blocksize=BLOCK_SIZE,
            callback=audio_callback,
        ):
            # 保持主執行緒運作
            while True:
                sd.sleep(1000)

    except KeyboardInterrupt:
        print("\n已手動停止監聽。")
    except Exception as e:
        print(f"\n發生錯誤: {e}")


if __name__ == "__main__":
    main()
