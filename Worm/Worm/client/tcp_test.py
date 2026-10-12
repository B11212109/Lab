import ssl
import numpy as np
import httpx

SERVER_URL = "https://worm.server.local:8000/upload"

# 1. 使用 Python 標準庫建立 SSL 上下文，避開 httpx 的 Bug
ssl_context = ssl.create_default_context(cafile="./cert/ca.crt")
# 載入客戶端憑證與私鑰
ssl_context.load_cert_chain(certfile="./cert/client.crt", keyfile="./cert/client.key")

# 2. 建立 32KB 的合法 float32 陣列
audio_data = np.zeros(8192, dtype=np.float32)
raw_bytes = audio_data.tobytes()

# 3. 帶上你的 server.py 需要的 Header (剛才伺服器報錯有提到 x_dtype)
headers = {
    "Content-Type": "application/octet-stream",
    # ⚠️ 請根據你的 server.py 要求填寫，例如：
    "x-dtype": "float32",
    # "x-sample-rate": "16000",
}

try:
    # 關鍵：只傳 verify=ssl_context，不要再傳 cert 參數
    with httpx.Client(verify=ssl_context) as client:
        response = client.post(SERVER_URL, content=raw_bytes, headers=headers)
        print("連線成功！狀態碼:", response.status_code)
        print("伺服器回應:", response.text)
except Exception as e:
    print("連線失敗:", repr(e))
