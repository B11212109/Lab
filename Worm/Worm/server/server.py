import datetime
import uvicorn
import ssl
import numpy as np
import soundfile as sf

from fastapi import FastAPI, Header, HTTPException, Request
from pathlib import Path

app = FastAPI()
SAVE_DIR = Path("./received_files")
SAVE_DIR.mkdir(exist_ok=True)


@app.post("/upload")
async def upload_raw_audio(
    request: Request,
    x_samplerate: int = Header(16000),
    x_dtype: str = Header("float32"),
):
    # 1. 接收原始 Byte 串流
    raw_data = await request.body()
    if not raw_data:
        raise HTTPException(status_code=400, detail="Empty payload")

    # 2. 用 NumPy 還原成陣列
    audio_array = np.frombuffer(raw_data, dtype=np.dtype(x_dtype))

    # [可選] 在這裡你可以直接把 audio_array 餵給 Whisper / AI 模型做運算
    # 解壓縮

    # 3. 伺服器將陣列轉存為音訊檔
    output_filename = (
        f"audio_{int(datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))}.wav"
    )
    output_path = SAVE_DIR / output_filename
    if not SAVE_DIR.exists():
        SAVE_DIR.mkdir(parents=True, exist_ok=True)
    sf.write(output_path, audio_array, x_samplerate)

    return {"status": "ok", "saved_as": output_filename, "samples": len(audio_array)}


if __name__ == "__main__":
    CERT_DIR = Path(__file__).parent / "cert"
    # 啟動伺服器，監聽 8000 port
    uvicorn.run(
        app,
        host="192.168.51.253",
        port=8000,
        access_log=False,
        ssl_keyfile=str(CERT_DIR / "server.key"),
        ssl_certfile=str(CERT_DIR / "server.crt"),
        ssl_ca_certs=str(CERT_DIR / "ca.crt"),
        ssl_cert_reqs=ssl.CERT_REQUIRED,
    )
