# 1. 產生自簽 CA（根憑證機構）
openssl ecparam -name prime256v1 -genkey -noout -out ca.key
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/CN=WormPrivateCA" \
    -addext "basicConstraints = critical, CA:TRUE" \
    -addext "keyUsage = critical, digitalSignature, keyCertSign, cRLSign"

# 2. 產生伺服器（Server）私鑰與簽署請求
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=WormServerCA"

# 建立伺服器 SAN 設定檔（避免驗證失敗，請換成伺服器的真實 IP 或主機名稱）
cat > server_ext.cnf << EOF
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
# sudo nano /etc/hosts
# ```
# 192.168.1.100 worm.server.local
# ```
DNS.1 = WormSoundServer
# IP.2 = 127.0.0.1
EOF

# 由 CA 簽署伺服器憑證
openssl x509 -req -days 3650 -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -extfile server_ext.cnf

# 3. 產生客戶端（Client）私鑰與憑證
openssl ecparam -name prime256v1 -genkey -noout -out client.key
openssl req -new -key client.key -out client.csr -subj "/CN=WormClient"
openssl x509 -req -days 3650 -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt

# 建立客戶端擴展設定檔
cat > client_ext.cnf << EOF
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

# 由 CA 簽署客戶端憑證
openssl x509 -req -days 3650 -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -extfile client_ext.cnf
