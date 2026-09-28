import http.server
import ssl


# ============================================================
# CONFIGURATION
# ============================================================

HOST = "0.0.0.0"
PORT = 8443

CERT_FILE = "cert.pem"
KEY_FILE = "key.pem"


# ============================================================
# HTTPS SERVER
# ============================================================

server_address = (
    HOST,
    PORT
)

httpd = http.server.HTTPServer(
    server_address,
    http.server.SimpleHTTPRequestHandler
)


# ============================================================
# TLS CONFIGURATION
# ============================================================

context = ssl.SSLContext(
    ssl.PROTOCOL_TLS_SERVER
)

context.load_cert_chain(
    certfile=CERT_FILE,
    keyfile=KEY_FILE
)


httpd.socket = context.wrap_socket(
    httpd.socket,
    server_side=True
)


# ============================================================
# START SERVER
# ============================================================

print("=" * 50)
print(" HYDRORACK HTTPS PHONE SERVER")
print("=" * 50)

print(
    f"HTTPS port: {PORT}"
)

print(
    "Dashboard:"
)

print(
    f"https://<PI_IP>:{PORT}/phone.html"
)

print("=" * 50)
print("Press CTRL+C to stop")
print("=" * 50)


try:

    httpd.serve_forever()

except KeyboardInterrupt:

    print()
    print("HTTPS SERVER STOPPED")

finally:

    httpd.server_close()