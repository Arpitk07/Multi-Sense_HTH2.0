#include <Arduino.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID = "Arpit";
const char* WIFI_PASSWORD = "1234567890";

const char* RADXA_URL =
  "http://192.168.50.2:5000/data";

// ============================================================
// GAS SENSORS
// ============================================================

#define MQ2_PIN       2
#define MQ3_PIN       4
#define MQ4_PIN       5
#define MQ7_PIN       6
#define MQ135_PIN     7

// ============================================================
// DHT22
// ============================================================

#define DHT_PIN       1
#define DHT_TYPE      DHT22

DHT dht(
  DHT_PIN,
  DHT_TYPE
);

// ============================================================
// MPU6050
// ============================================================

#define MPU_SDA       41
#define MPU_SCL       42
#define MPU_ADDR      0x68

TwoWire MPUWire = TwoWire(1);

// ============================================================
// OLED
// ============================================================

#define OLED_SDA      17
#define OLED_SCL      18
#define OLED_RESET    21

SSD1306Wire oled(
  0x3C,
  400000,
  OLED_SDA,
  OLED_SCL,
  GEOMETRY_128_64,
  OLED_RESET
);

// ============================================================
// VEXT
// ============================================================

#define VEXT_PIN      36

// ============================================================
// SENSOR VARIABLES
// ============================================================

// DHT22
float temperature = NAN;
float humidity = NAN;

bool dhtOK = false;

// MPU
float ax = 0;
float ay = 0;
float az = 0;

float gx = 0;
float gy = 0;
float gz = 0;

float imuTemp = 0;

bool imuOK = false;

// MQ
int mq2Raw = 0;
int mq3Raw = 0;
int mq4Raw = 0;
int mq7Raw = 0;
int mq135Raw = 0;

int mq2mV = 0;
int mq3mV = 0;
int mq4mV = 0;
int mq7mV = 0;
int mq135mV = 0;

// ============================================================
// TIMERS
// ============================================================

unsigned long lastDHT = 0;
unsigned long lastSend = 0;
unsigned long lastDisplay = 0;

const unsigned long DHT_INTERVAL = 2500;
const unsigned long SEND_INTERVAL = 3000;
const unsigned long DISPLAY_INTERVAL = 3000;

// ============================================================
// OLED PAGE
// ============================================================

int displayPage = 0;

const int TOTAL_PAGES = 4;

// ============================================================
// MPU REGISTERS
// ============================================================

#define MPU_WHO_AM_I      0x75
#define MPU_PWR_MGMT_1    0x6B
#define MPU_ACCEL_CONFIG  0x1C
#define MPU_GYRO_CONFIG   0x1B
#define MPU_ACCEL_XOUT_H  0x3B

// ============================================================
// WIFI
// ============================================================

void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 30
  )
  {
    delay(500);
    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Radxa: ");
    Serial.println(RADXA_URL);
  }
  else
  {
    Serial.println("Wi-Fi connection failed!");
  }
}

// ============================================================
// MPU WRITE
// ============================================================

void mpuWriteByte(
  uint8_t reg,
  uint8_t value
)
{
  MPUWire.beginTransmission(
    MPU_ADDR
  );

  MPUWire.write(reg);
  MPUWire.write(value);

  MPUWire.endTransmission();
}

// ============================================================
// MPU READ BYTE
// ============================================================

uint8_t mpuReadByte(
  uint8_t reg
)
{
  MPUWire.beginTransmission(
    MPU_ADDR
  );

  MPUWire.write(reg);

  if (
    MPUWire.endTransmission(false) != 0
  )
  {
    return 0xFF;
  }

  MPUWire.requestFrom(
    MPU_ADDR,
    (uint8_t)1
  );

  if (MPUWire.available())
  {
    return MPUWire.read();
  }

  return 0xFF;
}

// ============================================================
// MPU READ MULTIPLE
// ============================================================

bool mpuReadBytes(
  uint8_t reg,
  uint8_t *buffer,
  uint8_t length
)
{
  MPUWire.beginTransmission(
    MPU_ADDR
  );

  MPUWire.write(reg);

  if (
    MPUWire.endTransmission(false) != 0
  )
  {
    return false;
  }

  uint8_t received =
    MPUWire.requestFrom(
      MPU_ADDR,
      length
    );

  if (received != length)
  {
    return false;
  }

  for (
    uint8_t i = 0;
    i < length;
    i++
  )
  {
    buffer[i] =
      MPUWire.read();
  }

  return true;
}

// ============================================================
// INITIALIZE MPU
// ============================================================

bool initMPU()
{
  uint8_t whoami =
    mpuReadByte(
      MPU_WHO_AM_I
    );

  Serial.print(
    "MPU WHO_AM_I = 0x"
  );

  Serial.println(
    whoami,
    HEX
  );

  if (
    whoami != 0x68 &&
    whoami != 0x70
  )
  {
    Serial.println(
      "MPU NOT DETECTED!"
    );

    return false;
  }

  mpuWriteByte(
    MPU_PWR_MGMT_1,
    0x00
  );

  delay(100);

  mpuWriteByte(
    MPU_ACCEL_CONFIG,
    0x00
  );

  mpuWriteByte(
    MPU_GYRO_CONFIG,
    0x00
  );

  Serial.println(
    "MPU initialized."
  );

  return true;
}

// ============================================================
// READ MPU
// ============================================================

bool readMPU()
{
  uint8_t data[14];

  if (
    !mpuReadBytes(
      MPU_ACCEL_XOUT_H,
      data,
      14
    )
  )
  {
    return false;
  }

  int16_t rawAx =
    (int16_t)(
      (data[0] << 8) |
      data[1]
    );

  int16_t rawAy =
    (int16_t)(
      (data[2] << 8) |
      data[3]
    );

  int16_t rawAz =
    (int16_t)(
      (data[4] << 8) |
      data[5]
    );

  int16_t rawTemp =
    (int16_t)(
      (data[6] << 8) |
      data[7]
    );

  int16_t rawGx =
    (int16_t)(
      (data[8] << 8) |
      data[9]
    );

  int16_t rawGy =
    (int16_t)(
      (data[10] << 8) |
      data[11]
    );

  int16_t rawGz =
    (int16_t)(
      (data[12] << 8) |
      data[13]
    );

  // Accelerometer
  ax = rawAx / 16384.0;
  ay = rawAy / 16384.0;
  az = rawAz / 16384.0;

  // Gyroscope
  gx = rawGx / 131.0;
  gy = rawGy / 131.0;
  gz = rawGz / 131.0;

  // Temperature
  imuTemp =
    (rawTemp / 333.87) + 21.0;

  return true;
}

// ============================================================
// READ DHT22
// ============================================================

void readDHTSensor()
{
  float newTemperature =
    dht.readTemperature();

  float newHumidity =
    dht.readHumidity();

  if (
    !isnan(newTemperature) &&
    !isnan(newHumidity)
  )
  {
    temperature =
      newTemperature;

    humidity =
      newHumidity;

    dhtOK = true;
  }
}

// ============================================================
// READ MQ SENSORS
// ============================================================

void readMQSensors()
{
  mq2Raw =
    analogRead(MQ2_PIN);

  mq3Raw =
    analogRead(MQ3_PIN);

  mq4Raw =
    analogRead(MQ4_PIN);

  mq7Raw =
    analogRead(MQ7_PIN);

  mq135Raw =
    analogRead(MQ135_PIN);

  mq2mV =
    analogReadMilliVolts(
      MQ2_PIN
    );

  mq3mV =
    analogReadMilliVolts(
      MQ3_PIN
    );

  mq4mV =
    analogReadMilliVolts(
      MQ4_PIN
    );

  mq7mV =
    analogReadMilliVolts(
      MQ7_PIN
    );

  mq135mV =
    analogReadMilliVolts(
      MQ135_PIN
    );
}

// ============================================================
// OLED PAGE 1
// ENVIRONMENT
// ============================================================

void showEnvironmentPage()
{
  oled.clear();

  oled.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  oled.setFont(
    ArialMT_Plain_10
  );

  oled.drawString(
    0,
    0,
    "AEGISSENSE - ENV"
  );

  if (dhtOK)
  {
    oled.drawString(
      0,
      18,
      "Temp: " +
      String(
        temperature,
        1
      ) +
      " C"
    );

    oled.drawString(
      0,
      34,
      "Hum : " +
      String(
        humidity,
        1
      ) +
      " %"
    );
  }
  else
  {
    oled.drawString(
      0,
      25,
      "DHT22: ERROR"
    );
  }

  oled.display();
}

// ============================================================
// OLED PAGE 2
// IMU
// ============================================================

void showIMUPage()
{
  oled.clear();

  oled.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  oled.setFont(
    ArialMT_Plain_10
  );

  oled.drawString(
    0,
    0,
    "AEGISSENSE - IMU"
  );

  if (imuOK)
  {
    oled.drawString(
      0,
      14,
      "AX:" +
      String(ax, 2) +
      " AY:" +
      String(ay, 2)
    );

    oled.drawString(
      0,
      27,
      "AZ:" +
      String(az, 2)
    );

    oled.drawString(
      0,
      40,
      "GX:" +
      String(gx, 1)
    );

    oled.drawString(
      0,
      53,
      "GY:" +
      String(gy, 1) +
      " GZ:" +
      String(gz, 1)
    );
  }
  else
  {
    oled.drawString(
      0,
      25,
      "IMU ERROR"
    );
  }

  oled.display();
}

// ============================================================
// OLED PAGE 3
// MQ2 / MQ3 / MQ4
// ============================================================

void showMQPage1()
{
  oled.clear();

  oled.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  oled.setFont(
    ArialMT_Plain_10
  );

  oled.drawString(
    0,
    0,
    "AEGISSENSE - GAS 1"
  );

  oled.drawString(
    0,
    16,
    "MQ2 : " +
    String(mq2Raw)
  );

  oled.drawString(
    0,
    31,
    "MQ3 : " +
    String(mq3Raw)
  );

  oled.drawString(
    0,
    46,
    "MQ4 : " +
    String(mq4Raw)
  );

  oled.drawString(
    0,
    59,
    "RAW ADC"
  );

  oled.display();
}

// ============================================================
// OLED PAGE 4
// MQ7 / MQ135
// ============================================================

void showMQPage2()
{
  oled.clear();

  oled.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  oled.setFont(
    ArialMT_Plain_10
  );

  oled.drawString(
    0,
    0,
    "AEGISSENSE - GAS 2"
  );

  oled.drawString(
    0,
    18,
    "MQ7 : " +
    String(mq7Raw)
  );

  oled.drawString(
    0,
    34,
    "MQ135: " +
    String(mq135Raw)
  );

  oled.drawString(
    0,
    50,
    "RAW ADC"
  );

  oled.display();
}

// ============================================================
// UPDATE OLED
// ============================================================

void updateOLED()
{
  switch (displayPage)
  {
    case 0:
      showEnvironmentPage();
      break;

    case 1:
      showIMUPage();
      break;

    case 2:
      showMQPage1();
      break;

    case 3:
      showMQPage2();
      break;
  }

  displayPage++;

  if (
    displayPage >= TOTAL_PAGES
  )
  {
    displayPage = 0;
  }
}

// ============================================================
// SEND DATA TO RADXA
// ============================================================

void sendDataToRadxa()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println(
        "Cannot send data - Wi-Fi disconnected."
      );

      return;
    }
  }

  HTTPClient http;

  http.begin(RADXA_URL);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  String tempString =
    isnan(temperature)
      ? "null"
      : String(
          temperature,
          1
        );

  String humidityString =
    isnan(humidity)
      ? "null"
      : String(
          humidity,
          1
        );

  // ==========================================================
  // JSON
  // ==========================================================

  String json = "{";

  json +=
    "\"node\":\"NODE_2\",";

  json +=
    "\"temperature\":" +
    tempString +
    ",";

  json +=
    "\"humidity\":" +
    humidityString +
    ",";

  json +=
    "\"mq2_raw\":" +
    String(mq2Raw) +
    ",";

  json +=
    "\"mq2_mv\":" +
    String(mq2mV) +
    ",";

  json +=
    "\"mq3_raw\":" +
    String(mq3Raw) +
    ",";

  json +=
    "\"mq3_mv\":" +
    String(mq3mV) +
    ",";

  json +=
    "\"mq4_raw\":" +
    String(mq4Raw) +
    ",";

  json +=
    "\"mq4_mv\":" +
    String(mq4mV) +
    ",";

  json +=
    "\"mq7_raw\":" +
    String(mq7Raw) +
    ",";

  json +=
    "\"mq7_mv\":" +
    String(mq7mV) +
    ",";

  json +=
    "\"mq135_raw\":" +
    String(mq135Raw) +
    ",";

  json +=
    "\"mq135_mv\":" +
    String(mq135mV) +
    ",";

  // MPU
  json +=
    "\"ax\":" +
    String(ax, 3) +
    ",";

  json +=
    "\"ay\":" +
    String(ay, 3) +
    ",";

  json +=
    "\"az\":" +
    String(az, 3) +
    ",";

  json +=
    "\"gx\":" +
    String(gx, 2) +
    ",";

  json +=
    "\"gy\":" +
    String(gy, 2) +
    ",";

  json +=
    "\"gz\":" +
    String(gz, 2) +
    ",";

  json +=
    "\"imu_temperature\":" +
    String(imuTemp, 2);

  json += "}";

  // ==========================================================
  // SEND
  // ==========================================================

  Serial.println();
  Serial.println(
    "=========================================="
  );

  Serial.println(
    "SENDING NODE 2 DATA"
  );

  Serial.println(
    "=========================================="
  );

  Serial.println(json);

  int httpCode =
    http.POST(json);

  Serial.print(
    "HTTP Response: "
  );

  Serial.println(
    httpCode
  );

  if (httpCode > 0)
  {
    String response =
      http.getString();

    Serial.print(
      "Radxa Response: "
    );

    Serial.println(
      response
    );
  }
  else
  {
    Serial.print(
      "HTTP Error: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );
  }

  Serial.println(
    "=========================================="
  );

  http.end();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "=========================================="
  );

  Serial.println(
    "       AEGISSENSE NODE 2"
  );

  Serial.println(
    "=========================================="
  );

  // ==========================================================
  // ADC
  // ==========================================================

  analogReadResolution(12);

  analogSetPinAttenuation(
    MQ2_PIN,
    ADC_11db
  );

  analogSetPinAttenuation(
    MQ3_PIN,
    ADC_11db
  );

  analogSetPinAttenuation(
    MQ4_PIN,
    ADC_11db
  );

  analogSetPinAttenuation(
    MQ7_PIN,
    ADC_11db
  );

  analogSetPinAttenuation(
    MQ135_PIN,
    ADC_11db
  );

  // ==========================================================
  // VEXT
  // ==========================================================

  pinMode(
    VEXT_PIN,
    OUTPUT
  );

  // VEXT active LOW
  digitalWrite(
    VEXT_PIN,
    LOW
  );

  delay(100);

  // ==========================================================
  // OLED
  // ==========================================================

  Serial.println(
    "Initializing OLED..."
  );

  oled.init();

  oled.setContrast(
    255
  );

  oled.clear();

  oled.setTextAlignment(
    TEXT_ALIGN_CENTER
  );

  oled.setFont(
    ArialMT_Plain_16
  );

  oled.drawString(
    64,
    20,
    "AEGISSENSE"
  );

  oled.setFont(
    ArialMT_Plain_10
  );

  oled.drawString(
    64,
    42,
    "Universal Sensor Node"
  );

  oled.display();

  delay(1500);

  // ==========================================================
  // DHT22
  // ==========================================================

  Serial.println(
    "Initializing DHT22..."
  );

  dht.begin();

  delay(2000);

  // ==========================================================
  // MPU
  // ==========================================================

  Serial.println(
    "Initializing MPU..."
  );

  MPUWire.begin(
    MPU_SDA,
    MPU_SCL,
    400000
  );

  delay(100);

  imuOK =
    initMPU();

  // ==========================================================
  // FIRST READ
  // ==========================================================

  readDHTSensor();

  readMQSensors();

  imuOK =
    readMPU();

  // ==========================================================
  // WIFI
  // ==========================================================

  connectWiFi();

  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();
  Serial.println(
    "=========================================="
  );

  Serial.println(
    "       AEGISSENSE NODE 2 READY"
  );

  Serial.println(
    "=========================================="
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long currentMillis =
    millis();

  // ==========================================================
  // DHT22
  // ==========================================================

  if (
    currentMillis - lastDHT >=
    DHT_INTERVAL
  )
  {
    lastDHT =
      currentMillis;

    readDHTSensor();
  }

  // ==========================================================
  // MPU
  // ==========================================================

  imuOK =
    readMPU();

  // ==========================================================
  // MQ
  // ==========================================================

  readMQSensors();

  // ==========================================================
  // SEND TO RADXA
  // ==========================================================

  if (
    currentMillis - lastSend >=
    SEND_INTERVAL
  )
  {
    lastSend =
      currentMillis;

    sendDataToRadxa();
  }

  // ==========================================================
  // OLED
  // ==========================================================

  if (
    currentMillis - lastDisplay >=
    DISPLAY_INTERVAL
  )
  {
    lastDisplay =
      currentMillis;

    updateOLED();
  }

  delay(50);
}