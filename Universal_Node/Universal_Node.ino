/************************************************************
 *                     AEGISSENSE
 *          Universal Disaster Sensor Node
 *
 * Board:
 *   Heltec WiFi LoRa 32 V3
 *
 * Sensors:
 *   MQ-2      -> GPIO 2
 *   MQ-3      -> GPIO 4
 *   MQ-4      -> GPIO 5
 *   MQ-7      -> GPIO 6
 *   MQ-135    -> GPIO 7
 *   DHT22     -> GPIO 1
 *
 *   Soil      -> DISABLED
 *   Rain      -> DISABLED
 *
 * MPU:
 *   SDA       -> GPIO 41
 *   SCL       -> GPIO 42
 *
 * OLED:
 *   SDA       -> GPIO 17
 *   SCL       -> GPIO 18
 *   RESET     -> GPIO 21
 *
 * VEXT:
 *   GPIO 36
 ************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "DHT.h"


// ==========================================================
//                      PIN CONFIGURATION
// ==========================================================

// ---------------- GAS SENSORS ----------------

#define MQ2_PIN       2
#define MQ3_PIN       4
#define MQ4_PIN       5
#define MQ7_PIN       6
#define MQ135_PIN     7


// ---------------- DHT22 ----------------

#define DHT_PIN       1
#define DHT_TYPE      DHT22


// ---------------- MPU ----------------

#define MPU_SDA       41
#define MPU_SCL       42
#define MPU_ADDR      0x68


// ---------------- OLED ----------------

#define OLED_SDA      17
#define OLED_SCL      18
#define OLED_RESET    21


// ---------------- VEXT ----------------

#define VEXT_PIN      36



// ==========================================================
//                        OBJECTS
// ==========================================================

DHT dht(
  DHT_PIN,
  DHT_TYPE
);


// Separate I2C bus for MPU
TwoWire MPUWire = TwoWire(1);


// IMPORTANT:
// Keep this constructor exactly like the original
// working OLED configuration.

SSD1306Wire oled(
  0x3C,
  400000,
  OLED_SDA,
  OLED_SCL,
  GEOMETRY_128_64,
  OLED_RESET
);



// ==========================================================
//                     SENSOR VARIABLES
// ==========================================================


// ---------------- DHT22 ----------------

float temperature = NAN;
float humidity = NAN;

bool dhtOK = false;


// DHT22 filtering history

float tempHistory[3] = {
  NAN,
  NAN,
  NAN
};

float humHistory[3] = {
  NAN,
  NAN,
  NAN
};

int dhtHistoryCount = 0;


// ---------------- MPU ----------------

float ax = 0;
float ay = 0;
float az = 0;

float gx = 0;
float gy = 0;
float gz = 0;

float imuTemp = 0;

bool imuOK = false;


// ---------------- MQ SENSORS ----------------

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



// ==========================================================
//                       TIMERS
// ==========================================================

unsigned long lastDHT = 0;
unsigned long lastDisplay = 0;
unsigned long lastSerial = 0;


// DHT22 should not be read too quickly

const unsigned long DHT_INTERVAL = 2500;


// Serial output

const unsigned long SERIAL_INTERVAL = 3000;


// OLED page interval

const unsigned long DISPLAY_INTERVAL = 3000;



// ==========================================================
//                    OLED PAGE CONTROL
// ==========================================================

int displayPage = 0;


// Pages:
// 0 = Environment
// 1 = IMU
// 2 = Gas 1
// 3 = Gas 2

const int TOTAL_PAGES = 4;



// ==========================================================
//                     MPU REGISTERS
// ==========================================================

#define MPU_WHO_AM_I      0x75
#define MPU_PWR_MGMT_1    0x6B
#define MPU_ACCEL_CONFIG  0x1C
#define MPU_GYRO_CONFIG   0x1B
#define MPU_ACCEL_XOUT_H  0x3B



// ==========================================================
//                     MPU WRITE
// ==========================================================

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



// ==========================================================
//                      MPU READ
// ==========================================================

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


  if (
    MPUWire.available()
  )
  {
    return MPUWire.read();
  }


  return 0xFF;
}



// ==========================================================
//                 MPU READ MULTIPLE BYTES
// ==========================================================

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


  if (
    received != length
  )
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



// ==========================================================
//                    INITIALIZE MPU
// ==========================================================

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


  // Wake MPU

  mpuWriteByte(
    MPU_PWR_MGMT_1,
    0x00
  );

  delay(100);


  // Accelerometer ±2g

  mpuWriteByte(
    MPU_ACCEL_CONFIG,
    0x00
  );


  // Gyroscope ±250 deg/s

  mpuWriteByte(
    MPU_GYRO_CONFIG,
    0x00
  );


  Serial.println(
    "MPU initialized."
  );


  return true;
}



// ==========================================================
//                      READ MPU
// ==========================================================

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

  ax =
    rawAx / 16384.0;

  ay =
    rawAy / 16384.0;

  az =
    rawAz / 16384.0;


  // Gyroscope

  gx =
    rawGx / 131.0;

  gy =
    rawGy / 131.0;

  gz =
    rawGz / 131.0;


  // MPU temperature

  imuTemp =
    (rawTemp / 333.87) + 21.0;


  return true;
}



// ==========================================================
//                    MEDIAN OF 3
// ==========================================================

float median3(
  float a,
  float b,
  float c
)
{
  if (a > b)
  {
    float temp = a;

    a = b;
    b = temp;
  }


  if (b > c)
  {
    float temp = b;

    b = c;
    c = temp;
  }


  if (a > b)
  {
    float temp = a;

    a = b;
    b = temp;
  }


  return b;
}



// ==========================================================
//                 VALIDATE DHT READING
// ==========================================================

bool validDHTReading(
  float temp,
  float hum
)
{
  // NaN check

  if (
    isnan(temp) ||
    isnan(hum)
  )
  {
    return false;
  }


  // DHT22 temperature limits

  if (
    temp < -40.0 ||
    temp > 80.0
  )
  {
    return false;
  }


  // Humidity limits

  if (
    hum < 0.0 ||
    hum > 100.0
  )
  {
    return false;
  }


  return true;
}



// ==========================================================
//                    READ DHT22
// ==========================================================

void readDHTSensor()
{
  float newTemperature =
    dht.readTemperature();


  float newHumidity =
    dht.readHumidity();


  // --------------------------------------------------------
  // INVALID READING
  // --------------------------------------------------------

  if (
    !validDHTReading(
      newTemperature,
      newHumidity
    )
  )
  {
    // Do NOT destroy the previous valid value.

    if (!dhtOK)
    {
      Serial.println(
        "[DHT22] Waiting for valid reading..."
      );
    }

    return;
  }


  // --------------------------------------------------------
  // FIRST VALID READING
  // --------------------------------------------------------

  if (
    dhtHistoryCount == 0
  )
  {
    temperature =
      newTemperature;

    humidity =
      newHumidity;


    tempHistory[0] =
      newTemperature;

    humHistory[0] =
      newHumidity;


    dhtHistoryCount = 1;

    dhtOK = true;

    return;
  }


  // --------------------------------------------------------
  // CHECK FOR ABNORMAL JUMP
  // --------------------------------------------------------

  float tempDifference =
    fabs(
      newTemperature -
      temperature
    );


  float humidityDifference =
    fabs(
      newHumidity -
      humidity
    );


  // Temperature spike protection

  if (
    tempDifference > 5.0
  )
  {
    Serial.println(
      "[DHT22] Temperature spike ignored"
    );

    return;
  }


  // Humidity spike protection

  if (
    humidityDifference > 25.0
  )
  {
    Serial.println(
      "[DHT22] Humidity spike ignored"
    );

    return;
  }


  // --------------------------------------------------------
  // SHIFT HISTORY
  // --------------------------------------------------------

  tempHistory[0] =
    tempHistory[1];

  tempHistory[1] =
    tempHistory[2];

  tempHistory[2] =
    newTemperature;


  humHistory[0] =
    humHistory[1];

  humHistory[1] =
    humHistory[2];

  humHistory[2] =
    newHumidity;


  if (
    dhtHistoryCount < 3
  )
  {
    dhtHistoryCount++;
  }


  // --------------------------------------------------------
  // MEDIAN FILTER
  // --------------------------------------------------------

  if (
    dhtHistoryCount >= 3
  )
  {
    temperature =
      median3(
        tempHistory[0],
        tempHistory[1],
        tempHistory[2]
      );


    humidity =
      median3(
        humHistory[0],
        humHistory[1],
        humHistory[2]
      );
  }
  else
  {
    temperature =
      newTemperature;

    humidity =
      newHumidity;
  }


  dhtOK = true;
}



// ==========================================================
//                    READ MQ SENSORS
// ==========================================================

void readMQSensors()
{
  mq2Raw =
    analogRead(
      MQ2_PIN
    );


  mq3Raw =
    analogRead(
      MQ3_PIN
    );


  mq4Raw =
    analogRead(
      MQ4_PIN
    );


  mq7Raw =
    analogRead(
      MQ7_PIN
    );


  mq135Raw =
    analogRead(
      MQ135_PIN
    );


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



// ==========================================================
//                    PRINT MQ DATA
// ==========================================================

void printMQData(
  const char *name,
  int raw,
  int mV
)
{
  Serial.print(name);

  Serial.print(
    " | RAW: "
  );

  Serial.print(raw);

  Serial.print(
    " | mV: "
  );

  Serial.println(mV);
}



// ==========================================================
//                    SERIAL OUTPUT
// ==========================================================

void printSerialData()
{
  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "        AEGISSENSE SENSOR DATA"
  );

  Serial.println(
    "========================================"
  );


  // ========================================================
  // DHT22
  // ========================================================

  Serial.println();

  Serial.println(
    "[DHT22]"
  );


  if (dhtOK)
  {
    Serial.print(
      "Temperature : "
    );

    Serial.print(
      temperature,
      1
    );

    Serial.println(
      " C"
    );


    Serial.print(
      "Humidity    : "
    );

    Serial.print(
      humidity,
      1
    );

    Serial.println(
      " %"
    );
  }
  else
  {
    Serial.println(
      "DHT22 ERROR"
    );
  }



  // ========================================================
  // IMU
  // ========================================================

  Serial.println();

  Serial.println(
    "[IMU]"
  );


  if (imuOK)
  {
    Serial.print(
      "Accel X : "
    );

    Serial.print(
      ax,
      3
    );

    Serial.println(
      " g"
    );


    Serial.print(
      "Accel Y : "
    );

    Serial.print(
      ay,
      3
    );

    Serial.println(
      " g"
    );


    Serial.print(
      "Accel Z : "
    );

    Serial.print(
      az,
      3
    );

    Serial.println(
      " g"
    );


    Serial.print(
      "Gyro X  : "
    );

    Serial.print(
      gx,
      2
    );

    Serial.println(
      " deg/s"
    );


    Serial.print(
      "Gyro Y  : "
    );

    Serial.print(
      gy,
      2
    );

    Serial.println(
      " deg/s"
    );


    Serial.print(
      "Gyro Z  : "
    );

    Serial.print(
      gz,
      2
    );

    Serial.println(
      " deg/s"
    );


    Serial.print(
      "IMU Temp: "
    );

    Serial.print(
      imuTemp,
      2
    );

    Serial.println(
      " C"
    );
  }
  else
  {
    Serial.println(
      "IMU ERROR"
    );
  }



  // ========================================================
  // GAS SENSORS
  // ========================================================

  Serial.println();

  Serial.println(
    "[GAS SENSORS]"
  );


  printMQData(
    "MQ2  ",
    mq2Raw,
    mq2mV
  );


  printMQData(
    "MQ3  ",
    mq3Raw,
    mq3mV
  );


  printMQData(
    "MQ4  ",
    mq4Raw,
    mq4mV
  );


  printMQData(
    "MQ7  ",
    mq7Raw,
    mq7mV
  );


  printMQData(
    "MQ135",
    mq135Raw,
    mq135mV
  );


  Serial.println(
    "========================================"
  );
}



// ==========================================================
//                  OLED PAGE 1
//                    ENVIRONMENT
// ==========================================================

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



// ==========================================================
//                  OLED PAGE 2
//                       IMU
// ==========================================================

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
      String(
        ax,
        2
      ) +
      " AY:" +
      String(
        ay,
        2
      )
    );


    oled.drawString(
      0,
      27,
      "AZ:" +
      String(
        az,
        2
      )
    );


    oled.drawString(
      0,
      40,
      "GX:" +
      String(
        gx,
        1
      )
    );


    oled.drawString(
      0,
      53,
      "GY:" +
      String(
        gy,
        1
      ) +
      " GZ:" +
      String(
        gz,
        1
      )
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



// ==========================================================
//                  OLED PAGE 3
//                  MQ2 / MQ3 / MQ4
// ==========================================================

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
    String(
      mq2Raw
    )
  );


  oled.drawString(
    0,
    31,
    "MQ3 : " +
    String(
      mq3Raw
    )
  );


  oled.drawString(
    0,
    46,
    "MQ4 : " +
    String(
      mq4Raw
    )
  );


  oled.drawString(
    0,
    59,
    "RAW ADC"
  );


  oled.display();
}



// ==========================================================
//                  OLED PAGE 4
//                  MQ7 / MQ135
// ==========================================================

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
    String(
      mq7Raw
    )
  );


  oled.drawString(
    0,
    34,
    "MQ135: " +
    String(
      mq135Raw
    )
  );


  oled.drawString(
    0,
    50,
    "RAW ADC"
  );


  oled.display();
}



// ==========================================================
//                     UPDATE OLED
// ==========================================================

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



// ==========================================================
//                         SETUP
// ==========================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(1000);


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "       AEGISSENSE STARTING..."
  );

  Serial.println(
    "========================================"
  );



  // ========================================================
  // ADC
  // ========================================================

  analogReadResolution(
    12
  );


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



  // ========================================================
  // VEXT
  //
  // THIS IS KEPT FROM THE ORIGINAL BRIGHT VERSION.
  // ========================================================

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



  // ========================================================
  // OLED
  //
  // ORIGINAL WORKING INITIALIZATION SEQUENCE
  // ========================================================

  Serial.println(
    "Initializing OLED..."
  );


  oled.init();


  // Maximum contrast

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



  // ========================================================
  // DHT22
  // ========================================================

  Serial.println(
    "Initializing DHT22..."
  );


  dht.begin();


  delay(2000);



  // ========================================================
  // MPU I2C
  // ========================================================

  Serial.println(
    "Initializing MPU..."
  );


  MPUWire.begin(
    MPU_SDA,
    MPU_SCL,
    400000
  );


  delay(100);



  // ========================================================
  // MPU
  // ========================================================

  imuOK =
    initMPU();



  // ========================================================
  // FIRST SENSOR READ
  // ========================================================

  readDHTSensor();

  readMQSensors();

  imuOK =
    readMPU();



  // ========================================================
  // STARTUP STATUS
  // ========================================================

  Serial.println();

  Serial.println(
    "========================================"
  );


  Serial.print(
    "IMU       : "
  );


  Serial.println(
    imuOK ?
    "OK" :
    "ERROR"
  );


  Serial.println(
    "MQ2       : READY"
  );


  Serial.println(
    "MQ3       : READY"
  );


  Serial.println(
    "MQ4       : READY"
  );


  Serial.println(
    "MQ7       : READY"
  );


  Serial.println(
    "MQ135     : READY"
  );


  Serial.println(
    "========================================"
  );


  Serial.println(
    "       AEGISSENSE READY"
  );


  Serial.println(
    "========================================"
  );


  delay(1000);
}



// ==========================================================
//                          LOOP
// ==========================================================

void loop()
{
  unsigned long currentMillis =
    millis();



  // ========================================================
  // DHT22
  // ========================================================

  if (
    currentMillis -
    lastDHT >=
    DHT_INTERVAL
  )
  {
    lastDHT =
      currentMillis;


    readDHTSensor();
  }



  // ========================================================
  // MPU
  // ========================================================

  imuOK =
    readMPU();



  // ========================================================
  // GAS SENSORS
  // ========================================================

  readMQSensors();



  // ========================================================
  // SERIAL
  // ========================================================

  if (
    currentMillis -
    lastSerial >=
    SERIAL_INTERVAL
  )
  {
    lastSerial =
      currentMillis;


    printSerialData();
  }



  // ========================================================
  // OLED
  // ========================================================

  if (
    currentMillis -
    lastDisplay >=
    DISPLAY_INTERVAL
  )
  {
    lastDisplay =
      currentMillis;


    updateOLED();
  }


  delay(50);
}