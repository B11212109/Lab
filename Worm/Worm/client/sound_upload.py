import sys
import httpx
import queue
import threading
import time
import ssl

import numpy as np
import sounddevice as sd

from pathlib import Path

# ===== 參數設定 =====
DEVICE_ID = 0  # 填入第三步查到的 USB 音效卡編號；若設為 None 則使用系統預設
SAMPLE_RATE = 44100  # 取樣率（常見支援：16000, 44100, 48000）
CHANNELS = 1  # 聲道數（USB 麥克風通常為 1 (單聲道) 或 2 (雙聲道)）
BLOCK_SIZE = 4096  # 每次回呼處理的取樣點數（越小延遲越低，但 CPU 負擔略增）
TARGET_SAMPLES = 229376  # 你真正想要存檔的長度

CERT_DIR = Path(__file__).parent / "cert"
SERVER_URL = "https://WormSoundServer:8000/upload"

# 1. 建立一個執行緒安全的佇列（Queue）
# maxsize 限制佇列長度，避免 Worker 處理太慢導致記憶體被塞爆
audio_queue = queue.Queue(maxsize=100)

# 控制程式退出的 Event
stop_event = threading.Event()

last_call_time = 0


def send_raw_numpy(client: httpx.Client, audio_array: np.ndarray, sample_rate: int):
    # 將 array 直接轉成二進位 byte
    raw_bytes = audio_array.tobytes()

    headers = {
        "Content-Type": "application/octet-stream",
        "x-samplerate": str(sample_rate),
        "x-dtype": str(audio_array.dtype),
    }

    response = client.post(SERVER_URL, content=raw_bytes, headers=headers)
    try:
        print("Send:", response.json())
    except Exception as e:
        print("Error:", e)


def worker_thread(client: httpx.Client):
    print("[Worker] 處理執行緒已啟動...")
    buffer = []
    current_samples = 0

    while not stop_event.is_set():
        try:
            chunk = audio_queue.get(timeout=0.5)
            buffer.append(chunk)
            current_samples += len(chunk)

            # 當累積滿 32768 個點（約 0.74 秒）才進行存檔
            if current_samples >= TARGET_SAMPLES:
                full_audio = np.concatenate(buffer)[:TARGET_SAMPLES]

                # 【存檔或發送】
                send_raw_numpy(client, full_audio, SAMPLE_RATE)

                # 重置 buffer
                buffer = []
                current_samples = 0

            audio_queue.task_done()
        except queue.Empty:
            continue

    print("[Worker] 處理執行緒已結束。")


def audio_callback(indata, frames, time_info, status):
    """
    音效卡每蒐集到 BLOCK_SIZE 個音訊樣本，就會自動呼叫此函式一次
    indata: NumPy 陣列，形狀為 (frames, channels)，資料型態預設為 float32 (-1.0 ~ 1.0)
    """
    if status:
        print(f"狀態警示: {status}", file=sys.stderr)

    # print(
    #     f"音訊回呼: frames={frames}, channels={indata.shape[1]}, dtype={indata.dtype}",
    # )

    # 1. 取得音訊數據 (NumPy Array)
    audio_data = indata[:, 0]  # 若為單聲道取第 0 軌

    # 壓縮在這裡做

    # 2. 將音訊數據傳送到伺服器
    audio_queue.put_nowait(audio_data.copy())


def main():
    print("開始持續監聽 USB 音訊... 按 Ctrl+C 結束。")

    ca_cert = str(CERT_DIR / "ca.crt")
    client_cert = (str(CERT_DIR / "client.crt"), str(CERT_DIR / "client.key"))
    # 1. 使用 Python 標準庫建立 SSL 上下文，避開 httpx 的 Bug
    ssl_context = ssl.create_default_context(cafile=ca_cert)
    # 載入客戶端憑證與私鑰
    ssl_context.load_cert_chain(certfile=client_cert[0], keyfile=client_cert[1])
    client = httpx.Client(timeout=10.0, verify=ssl_context)
    #client = httpx.Client(timeout=10.0)

    worker = threading.Thread(target=worker_thread, args=(client,), daemon=True)
    worker.start()

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
                if (worker.is_alive() is False) and (not audio_queue.empty()):
                    print("[ERROR] Worker thread has stopped unexpectedly.")
                    break
                sd.sleep(1000)

    except KeyboardInterrupt:
        print("\n已手動停止監聽。")
    except Exception as e:
        print(f"\n發生錯誤: {e}")
    finally:
        # 通知 Worker 執行緒結束
        stop_event.set()
        worker.join()
        client.close()


if __name__ == "__main__":
    main()
