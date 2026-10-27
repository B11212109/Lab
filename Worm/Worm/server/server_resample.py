import datetime
import asyncio
import uvicorn
import ssl
import numpy as np
import soundfile as sf
import soxr

from fastapi import FastAPI, Header, HTTPException, Request
from pathlib import Path

app = FastAPI()
SAVE_DIR = Path("./received_files")
SAVE_DIR.mkdir(exist_ok=True)

# ===== 音訊設定：Raspberry Pi 44.1 kHz -> Windows 16 kHz =====
INPUT_SAMPLE_RATE = 44100
OUTPUT_SAMPLE_RATE = 16000
TARGET_SAMPLES = 82160  # 每個 WAV 5.135 秒

# 一台 Raspberry Pi 持續依序上傳時，共用同一組重取樣器與緩衝區。
# 注意：保持單一 Uvicorn worker；伺服器重新啟動會清空此狀態。
resampler = soxr.ResampleStream(
    INPUT_SAMPLE_RATE,
    OUTPUT_SAMPLE_RATE,
    1,
    dtype="float32",
    quality="HQ",
)
audio_buffer = np.empty(0, dtype=np.float32)
audio_lock = asyncio.Lock()
file_index = 0


@app.post("/upload")
async def upload_raw_audio(
    request: Request,
    x_samplerate: int = Header(44100),
    x_dtype: str = Header("float32"),
):
    global audio_buffer, file_index

    # 1. 接收原始 Byte 串流
    raw_data = await request.body()
    if not raw_data:
        raise HTTPException(status_code=400, detail="Empty payload")

    # 2. 此串流僅接受 44.1 kHz、float32、單聲道資料
    if x_samplerate != INPUT_SAMPLE_RATE or x_dtype != "float32":
        raise HTTPException(
            status_code=422,
            detail="Expected 44100 Hz mono float32 input",
        )
    if len(raw_data) % np.dtype("float32").itemsize != 0:
        raise HTTPException(status_code=400, detail="Invalid float32 byte length")

    # 3. bytes -> NumPy float32 陣列
    audio_array = np.frombuffer(raw_data, dtype=np.float32)

    # 4. 上鎖，確保共用的重取樣器與 Buffer 不被同時修改
    async with audio_lock:
        # 保留跨 HTTP 批次的重取樣濾波器狀態
        audio_16k = resampler.resample_chunk(audio_array, last=False)

        # 新樣本銜接到原有的剩餘樣本後面
        audio_buffer = np.concatenate((audio_buffer, audio_16k))
        saved_files = []

        # 5. 每滿 82160 點，輸出一個 32-bit Float WAV
        while len(audio_buffer) >= TARGET_SAMPLES:
            segment = audio_buffer[:TARGET_SAMPLES].copy()

            file_index += 1
            timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
            output_filename = f"audio_{timestamp}_{file_index:06d}.wav"
            output_path = SAVE_DIR / output_filename

            sf.write(output_path, segment, OUTPUT_SAMPLE_RATE, subtype="FLOAT")
            saved_files.append(output_filename)

            # 保留尚未用到的音訊，等待下一次 HTTP 上傳
            audio_buffer = audio_buffer[TARGET_SAMPLES:]

        return {
            "status": "ok",
            "received_samples": int(audio_array.size),
            "resampled_samples": int(audio_16k.size),
            "saved_files": saved_files,
            "buffered_samples": int(audio_buffer.size),
        }


if __name__ == "__main__":
    CERT_DIR = Path(__file__).parent / "cert"
    # 原本 HTTPS/mTLS 設定保持不變
    uvicorn.run(
        app,
        host="192.168.51.102",  # 後續根據伺服器 IP 修改
        port=8000,
        access_log=False,
        ssl_keyfile=str(CERT_DIR / "server.key"),
        ssl_certfile=str(CERT_DIR / "server.crt"),
        ssl_ca_certs=str(CERT_DIR / "ca.crt"),
        ssl_cert_reqs=ssl.CERT_REQUIRED,
    )
