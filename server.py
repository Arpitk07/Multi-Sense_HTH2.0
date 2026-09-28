import asyncio
import ssl
import threading
import time

import numpy as np
import websockets

from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from PIL import Image, ImageDraw, ImageFont


# ============================================================
# CONFIGURATION
# ============================================================

HOST = "0.0.0.0"
PORT = 8765

SAMPLE_RATE = 16000

OLED_WIDTH = 128
OLED_HEIGHT = 64

latest_audio = np.zeros(512, dtype=np.int16)
phone_connected = False

data_lock = threading.Lock()


# ============================================================
# OLED INITIALIZATION
# ============================================================

serial = i2c(
    port=1,
    address=0x3C
)

oled = ssd1306(serial)


# ============================================================
# FONT
# ============================================================

try:
    font = ImageFont.truetype(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        9
    )
except Exception:
    font = ImageFont.load_default()


# ============================================================
# AUDIO PROCESSING
# ============================================================

def process_audio(data):
    global latest_audio

    if len(data) < 2:
        return

    try:
        samples = np.frombuffer(
            data,
            dtype=np.int16
        )
    except Exception as e:
        print("AUDIO PARSE ERROR:", e)
        return

    if len(samples) == 0:
        return

    with data_lock:

        if len(samples) >= 512:

            latest_audio = samples[-512:].copy()

        else:

            latest_audio = np.concatenate(
                (
                    latest_audio[len(samples):],
                    samples
                )
            )


# ============================================================
# WEBSOCKET HANDLER
# ============================================================

async def phone_handler(websocket):

    global phone_connected

    phone_connected = True

    print()
    print("================================")
    print("PHONE CONNECTED")
    print("================================")

    try:

        async for message in websocket:

            if isinstance(message, bytes):

                process_audio(message)

    except websockets.exceptions.ConnectionClosed:

        print()
        print("PHONE DISCONNECTED")

    except Exception as e:

        print()
        print("WEBSOCKET ERROR:")
        print(e)

    finally:

        phone_connected = False


# ============================================================
# OLED DISPLAY
# ============================================================

def draw_oled():

    image = Image.new(
        "1",
        (
            OLED_WIDTH,
            OLED_HEIGHT
        )
    )

    draw = ImageDraw.Draw(image)

    # --------------------------------------------------------
    # HEADER
    # --------------------------------------------------------

    draw.text(
        (2, 0),
        "HYDRORACK",
        font=font,
        fill=255
    )

    if phone_connected:

        draw.text(
            (88, 0),
            "● LIVE",
            font=font,
            fill=255
        )

    else:

        draw.text(
            (76, 0),
            "○ WAIT",
            font=font,
            fill=255
        )

    # --------------------------------------------------------
    # SEPARATOR
    # --------------------------------------------------------

    draw.line(
        (0, 10, 127, 10),
        fill=255
    )

    # --------------------------------------------------------
    # SUBTITLE
    # --------------------------------------------------------

    draw.text(
        (2, 12),
        "ACOUSTIC SENSOR",
        font=font,
        fill=255
    )

    # --------------------------------------------------------
    # AUDIO DATA
    # --------------------------------------------------------

    with data_lock:

        samples = latest_audio.copy()

    # --------------------------------------------------------
    # WAVEFORM AREA
    # --------------------------------------------------------

    waveform_top = 23
    waveform_bottom = 53

    center_y = (
        waveform_top +
        waveform_bottom
    ) // 2

    waveform_height = (
        waveform_bottom -
        waveform_top
    ) // 2

    if len(samples) > 1:

        samples = samples.astype(
            np.float32
        )

        # Remove DC offset
        samples -= np.mean(samples)

        # ----------------------------------------------------
        # Downsample to 128 OLED columns
        # ----------------------------------------------------

        indices = np.linspace(
            0,
            len(samples) - 1,
            OLED_WIDTH
        ).astype(int)

        waveform = samples[indices]

        # ----------------------------------------------------
        # Automatic amplitude scaling
        # ----------------------------------------------------

        peak = np.max(
            np.abs(waveform)
        )

        if peak > 100:

            normalized = (
                waveform / peak
            )

            normalized *= 0.90

        else:

            normalized = (
                waveform / 32768.0
            )

        # ----------------------------------------------------
        # Smooth waveform
        # ----------------------------------------------------

        if len(normalized) >= 3:

            smoothed = np.copy(
                normalized
            )

            smoothed[1:-1] = (
                normalized[:-2] * 0.25
                +
                normalized[1:-1] * 0.50
                +
                normalized[2:] * 0.25
            )

            normalized = smoothed

        # ----------------------------------------------------
        # Draw waveform
        # ----------------------------------------------------

        points = []

        for x, value in enumerate(normalized):

            value = float(value)

            value = max(
                -1.0,
                min(1.0, value)
            )

            y = int(
                center_y
                -
                value * waveform_height
            )

            y = max(
                waveform_top,
                min(
                    waveform_bottom,
                    y
                )
            )

            points.append(
                (x, y)
            )

        if len(points) > 1:

            draw.line(
                points,
                fill=255,
                width=1
            )

    # --------------------------------------------------------
    # FOOTER
    # --------------------------------------------------------

    draw.line(
        (0, 55, 127, 55),
        fill=255
    )

    if phone_connected:

        draw.text(
            (2, 57),
            "MIC ●",
            font=font,
            fill=255
        )

        draw.text(
            (43, 57),
            "16K",
            font=font,
            fill=255
        )

        draw.text(
            (76, 57),
            "STREAM",
            font=font,
            fill=255
        )

    else:

        draw.text(
            (2, 57),
            "MIC ○",
            font=font,
            fill=255
        )

        draw.text(
            (43, 57),
            "16K",
            font=font,
            fill=255
        )

        draw.text(
            (76, 57),
            "WAITING",
            font=font,
            fill=255
        )

    # --------------------------------------------------------
    # SEND TO OLED
    # --------------------------------------------------------

    oled.display(image)


# ============================================================
# OLED THREAD
# ============================================================

def oled_loop():

    print("OLED DISPLAY STARTED")

    while True:

        try:

            draw_oled()

            time.sleep(0.05)

        except Exception as e:

            print(
                "OLED ERROR:",
                e
            )

            time.sleep(1)


# ============================================================
# WEBSOCKET SERVER
# ============================================================

async def main():

    ssl_context = ssl.SSLContext(
        ssl.PROTOCOL_TLS_SERVER
    )

    ssl_context.load_cert_chain(
        certfile="cert.pem",
        keyfile="key.pem"
    )

    print()
    print("=" * 50)
    print("       HYDRORACK AUDIO SERVER")
    print("=" * 50)

    print(
        f"Listening on 0.0.0.0:{PORT}"
    )

    print(
        f"Sample rate: {SAMPLE_RATE} Hz"
    )

    print(
        "Secure WebSocket: WSS"
    )

    print(
        "Waiting for phone..."
    )

    print("=" * 50)
    print()

    async with websockets.serve(
        phone_handler,
        HOST,
        PORT,
        max_size=None,
        ssl=ssl_context
    ):

        await asyncio.Future()


# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":

    oled_thread = threading.Thread(
        target=oled_loop,
        daemon=True
    )

    oled_thread.start()

    asyncio.run(
        main()
    )