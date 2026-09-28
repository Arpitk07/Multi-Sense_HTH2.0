HydroRack Acoustic Sensor Node

A Raspberry Pi–based wireless acoustic sensing node for the HydroRack disaster-response system. The node uses a smartphone as a wireless microphone, streams live audio to a Raspberry Pi over a local hotspot network, and visualizes the incoming audio waveform on an SSD1306 OLED display.

The system is designed as a foundation for edge-based distress and acoustic-event detection in disaster-response scenarios.

🚨 Overview

HydroRack Acoustic Sensor is designed to provide an inexpensive, portable audio sensing node that can later be integrated with other HydroRack perception systems such as:

👤 Human/person detection
📷 Computer vision
🔊 Scream/distress detection
🗣️ Voice/help detection
💥 Impact/acoustic-event detection
🧠 Edge AI
📡 Multi-node disaster monitoring

The current implementation focuses on establishing a reliable real-time audio streaming and visualization pipeline.

                  📱 Smartphone
              ┌──────────────────┐
              │ Built-in          │
              │ Microphone        │
              │                   │
              │ HydroRack Web UI  │
              └────────┬─────────┘
                       │
                  Phone Hotspot
                       │
                       ▼
              ┌──────────────────┐
              │ Raspberry Pi 4B  │
              │                  │
              │ HTTPS :8443      │
              │ WSS   :8765      │
              │                  │
              │ Audio Processing │
              └────────┬─────────┘
                       │
                       ▼
                ┌──────────────┐
                │ SSD1306 OLED │
                │   128 × 64   │
                │              │
                │ Live Waveform│
                └──────────────┘
✨ Features
Current
📱 Smartphone microphone as wireless audio source
📶 Phone hotspot networking
🍓 Raspberry Pi 4B audio server
🔐 HTTPS dashboard
🔒 Secure WebSocket (WSS) audio streaming
🎙️ Real-time microphone streaming
📊 Live waveform visualization
🖥️ SSD1306 128×64 OLED interface
🔌 I²C OLED communication
🐍 Python-based backend
⚡ Lightweight Raspberry Pi implementation
📴 No cloud connection required
Planned
🎵 True 16 kHz audio resampling
🎚️ Audio preprocessing
🔊 Noise reduction
📈 Log-Mel spectrogram generation
🧠 Edge AI acoustic classification
🚨 Scream/distress detection
🗣️ "Help" / shout detection
💥 Impact detection
👶 Crying detection
⏱️ Temporal event smoothing
📡 Integration with the HydroRack sensor network
📷 Multimodal fusion with camera-based detection
🧠 Integration with Radxa Cubie A7A / edge processing node
🧰 Hardware
Component	Purpose
Raspberry Pi 4B	Main audio processing node
SSD1306 128×64 OLED	Local status + waveform display
Smartphone	Wireless microphone
Smartphone hotspot	Local network
USB-C / Micro-USB power	Raspberry Pi power
🔌 OLED Wiring

The SSD1306 communicates with the Raspberry Pi using I²C.

SSD1306	Raspberry Pi
VCC	3.3V — Pin 1
GND	GND — Pin 6
SDA	GPIO2 — Pin 3
SCL	GPIO3 — Pin 5

OLED I²C address:

0x3C

Verify with:

i2cdetect -y 1

Expected:

30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- --
🌐 Network Architecture

The smartphone acts as the hotspot.

              📱 Smartphone
              Hotspot / Gateway
              10.92.106.222
                    │
          ┌─────────┴─────────┐
          │                   │
          ▼                   ▼
    🍓 Raspberry Pi         💻 Laptop
    10.92.106.202           SSH / Dev

The laptop is only used for development and SSH access.

The final sensing system only requires:

📱 Phone
   +
🍓 Raspberry Pi
   +
📺 OLED
🔗 Communication

Two services are used.

HTTPS Dashboard
Port: 8443

The phone opens:

https://10.92.106.202:8443/phone.html
Secure WebSocket Audio
Port: 8765

The browser connects using:

wss://10.92.106.202:8765

Audio is transmitted as binary PCM data.

📁 Project Structure
phone_audio_oled/
│
├── server.py
├── https_server.py
├── phone.html
│
├── cert.pem
├── key.pem
│
├── oled_test.py
│
├── .venv/
│
└── README.md

cert.pem and key.pem are local TLS credentials and should not be committed to a public repository.

Add them to .gitignore.

🐍 Software Stack
Python 3.11
NumPy
WebSockets
Luma.OLED
Luma.Core
Pillow
SMBus2
HTML5
JavaScript
Web Audio API
WebSocket API
TLS/HTTPS

Main Python packages:

numpy
websockets
luma.oled
luma.core
Pillow
smbus2
⚙️ Installation
1. Clone the repository
git clone <YOUR_REPOSITORY_URL>
cd phone_audio_oled
2. Create a virtual environment
python3.11 -m venv .venv

Activate it:

source .venv/bin/activate
3. Install dependencies
pip install numpy websockets luma.oled luma.core Pillow smbus2
🔧 Enable I²C

On Raspberry Pi:

sudo raspi-config

Go to:

Interface Options
    → I2C
        → Enable

Reboot if necessary.

Then verify:

i2cdetect -y 1

The OLED should appear at:

0x3C
🖥️ OLED Test

Before running the complete system:

python oled_test.py

The OLED should display the HydroRack startup screen.

🔐 TLS Certificate

The current prototype uses a local self-signed certificate.

Generate one with:

openssl req -x509 \
-newkey rsa:2048 \
-keyout key.pem \
-out cert.pem \
-days 365 \
-nodes \
-subj "/CN=gesturebot.local"

For production deployment, use a properly trusted certificate or an appropriate local CA.

🚀 Running the System

Two services are required.

Terminal 1 — HTTPS Dashboard
cd ~/phone_audio_oled
source .venv/bin/activate
python https_server.py

The server runs on:

https://0.0.0.0:8443
Terminal 2 — Audio Server
cd ~/phone_audio_oled
source .venv/bin/activate
python server.py

The server listens on:

0.0.0.0:8765
📱 Connect the Phone

Connect the Raspberry Pi to the phone's hotspot.

Then open on the same phone:

https://10.92.106.202:8443/phone.html

Press:

START MICROPHONE

Allow microphone access.

The phone will then stream audio to:

wss://10.92.106.202:8765
📺 OLED Interface

The OLED provides local feedback without requiring the dashboard.

Current interface includes:

┌────────────────────────┐
│ HYDRORACK  ● LIVE      │
│ ACOUSTIC SENSOR        │
│                        │
│     ╱╲    ╱╲           │
│ ___╱  ╲__╱  ╲____      │
│   ╲╱╲╱╲╱╲╱╲╱╲╱       │
│                        │
│ MIC ●   16K   STREAM   │
└────────────────────────┘

The waveform is generated from the most recent received audio samples.

🧠 Planned AI Pipeline

The current project establishes the communication and sensing layer.

The next stage is acoustic intelligence:

Smartphone Microphone
        │
        ▼
   Audio Stream
        │
        ▼
   Preprocessing
        │
        ├── Noise Reduction
        ├── Normalization
        └── Resampling → 16 kHz
        │
        ▼
   1–2 Second Window
        │
        ▼
 Log-Mel Spectrogram
        │
        ▼
 Lightweight CNN / ONNX
        │
        ▼
 Acoustic Classification
        │
        ├── Background
        ├── Speech
        ├── Shout / Help
        ├── Scream
        ├── Crying
        └── Impact
        │
        ▼
 Temporal Smoothing
        │
        ▼
   HydroRack Event

The goal is to detect acoustic distress events, rather than making the audio model alone claim that a person or victim has been detected.

🔮 Future HydroRack Integration

The acoustic node is intended to become one sensing modality within a larger disaster-response system.

                    HYDRORACK
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
    📷 Vision      🔊 Acoustic     🌡️ Sensors
        │              │              │
        ▼              ▼              ▼
     Person         Scream          Gas
     Detection      Detection       Flood
     Tracking       Voice           Vibration
        │              │              │
        └──────────────┼──────────────┘
                       ▼
                 Edge AI Fusion
                       │
                       ▼
                Disaster Intelligence

Future versions can combine acoustic and visual evidence to improve situational awareness.

🛠️ Troubleshooting
OLED not detected

Run:

i2cdetect -y 1

Check for:

3c

Verify:

SDA → GPIO2
SCL → GPIO3
VCC → 3.3V
GND → GND
Port 8765 already in use

Run:

sudo fuser -k 8765/tcp

Then:

python server.py

Check:

sudo ss -ltnp | grep ':8765'
Dashboard doesn't open

Check the Pi IP:

hostname -I

Then use:

https://<PI_IP>:8443/phone.html
Microphone doesn't work

Make sure:

The page is loaded over HTTPS.
Microphone permission is granted.
The WebSocket uses:
wss://

not:

ws://
🔒 Security

This project currently uses a local self-signed TLS certificate for prototype deployment.

Do not commit:

cert.pem
key.pem

Add:

.venv/
__pycache__/
*.pyc
cert.pem
key.pem
.env
📌 Project Status
Component	Status
Raspberry Pi audio server	✅ Working
Phone hotspot networking	✅ Working
HTTPS dashboard	✅ Working
WSS audio streaming	✅ Working
Smartphone microphone	✅ Working
SSD1306 OLED	✅ Working
Live waveform	✅ Working
Audio preprocessing	🔄 Next
16 kHz resampling	🔄 Planned
Log-Mel features	🔄 Planned
Distress classifier	🔄 Planned
Scream detection	🔄 Planned
Multimodal fusion	🔄 Planned
