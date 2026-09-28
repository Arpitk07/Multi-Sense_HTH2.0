#include <Arduino.h>
#include <TinyGPSPlus.h>

// =====================================================
// PIN DEFINITIONS
// =====================================================

// Analog sensors
#define PH_PIN       34
#define TDS_PIN      35
#define RAIN_PIN     32
#define SOIL_PIN     33

// GPS UART2
#define GPS_RX       16   // ESP32 RX <- GPS TX
#define GPS_TX       17   // ESP32 TX -> GPS RX

// =====================================================
// GPS
// =====================================================

TinyGPSPlus gps;
HardwareSerial GPS_Serial(2);

// =====================================================
// ADC
// =====================================================

#define ADC_MAX  4095.0
#define ADC_VREF 3.3

// =====================================================
// CALIBRATION VALUES
// CHANGE THESE AFTER TESTING
// =====================================================

// Soil moisture:
// Replace these after measuring your sensor
int SOIL_DRY = 4095;
int SOIL_WET = 1500;

// Rain sensor:
// Usually higher value = dry
// and lower value = wet.
int RAIN_DRY = 4095;
int RAIN_WET = 1000;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // ADC configuration
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // GPS
  GPS_Serial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  Serial.println();
  Serial.println("==========================================");
  Serial.println(" ESP32 ENVIRONMENT MONITORING NODE");
  Serial.println("==========================================");

  Serial.println("pH          -> GPIO 34");
  Serial.println("TDS         -> GPIO 35");
  Serial.println("Rain        -> GPIO 32");
  Serial.println("Soil        -> GPIO 33");
  Serial.println("GPS RX/TX   -> GPIO 16/17");

  Serial.println("==========================================");
  Serial.println();

  delay(1000);
}

// =====================================================
// READ AVERAGE ADC
// =====================================================

int readAverage(int pin) {

  long sum = 0;

  const int samples = 10;

  for (int i = 0; i < samples; i++) {

    sum += analogRead(pin);

    delay(5);
  }

  return sum / samples;
}

// =====================================================
// CONVERT ADC TO VOLTAGE
// =====================================================

float adcToVoltage(int adc) {

  return (adc / ADC_MAX) * ADC_VREF;
}

// =====================================================
// CONVERT SOIL ADC TO PERCENTAGE
// =====================================================

int soilPercentage(int value) {

  int percentage = map(
    value,
    SOIL_DRY,
    SOIL_WET,
    0,
    100
  );

  percentage = constrain(
    percentage,
    0,
    100
  );

  return percentage;
}

// =====================================================
// CONVERT RAIN ADC TO PERCENTAGE
// =====================================================

int rainPercentage(int value) {

  int percentage = map(
    value,
    RAIN_DRY,
    RAIN_WET,
    0,
    100
  );

  percentage = constrain(
    percentage,
    0,
    100
  );

  return percentage;
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // =================================================
  // GPS
  // =================================================

  while (GPS_Serial.available()) {

    gps.encode(
      GPS_Serial.read()
    );
  }

  // =================================================
  // READ SENSORS
  // =================================================

  int phRaw   = readAverage(PH_PIN);
  int tdsRaw  = readAverage(TDS_PIN);
  int rainRaw = readAverage(RAIN_PIN);
  int soilRaw = readAverage(SOIL_PIN);

  // =================================================
  // VOLTAGES
  // =================================================

  float phVoltage =
    adcToVoltage(phRaw);

  float tdsVoltage =
    adcToVoltage(tdsRaw);

  float rainVoltage =
    adcToVoltage(rainRaw);

  float soilVoltage =
    adcToVoltage(soilRaw);

  // =================================================
  // PERCENTAGES
  // =================================================

  int rainPercent =
    rainPercentage(rainRaw);

  int soilPercent =
    soilPercentage(soilRaw);

  // =================================================
  // PRINT
  // =================================================

  Serial.println();
  Serial.println("==========================================");

  // -------------------------------------------------
  // pH
  // -------------------------------------------------

  Serial.println("[ pH SENSOR ]");

  Serial.print("ADC       : ");
  Serial.println(phRaw);

  Serial.print("Voltage   : ");
  Serial.print(phVoltage, 3);
  Serial.println(" V");

  // -------------------------------------------------
  // TDS
  // -------------------------------------------------

  Serial.println();
  Serial.println("[ TDS SENSOR ]");

  Serial.print("ADC       : ");
  Serial.println(tdsRaw);

  Serial.print("Voltage   : ");
  Serial.print(tdsVoltage, 3);
  Serial.println(" V");

  // -------------------------------------------------
  // RAIN
  // -------------------------------------------------

  Serial.println();
  Serial.println("[ RAIN SENSOR ]");

  Serial.print("ADC       : ");
  Serial.println(rainRaw);

  Serial.print("Voltage   : ");
  Serial.print(rainVoltage, 3);
  Serial.println(" V");

  Serial.print("Rain Level: ");
  Serial.print(rainPercent);
  Serial.println(" %");

  // -------------------------------------------------
  // SOIL
  // -------------------------------------------------

  Serial.println();
  Serial.println("[ SOIL MOISTURE ]");

  Serial.print("ADC       : ");
  Serial.println(soilRaw);

  Serial.print("Voltage   : ");
  Serial.print(soilVoltage, 3);
  Serial.println(" V");

  Serial.print("Moisture  : ");
  Serial.print(soilPercent);
  Serial.println(" %");

  // -------------------------------------------------
  // GPS
  // -------------------------------------------------

  Serial.println();
  Serial.println("[ GPS ]");

  if (gps.location.isValid()) {

    Serial.println("Status    : FIXED");

    Serial.print("Latitude  : ");
    Serial.println(
      gps.location.lat(),
      6
    );

    Serial.print("Longitude : ");
    Serial.println(
      gps.location.lng(),
      6
    );

    Serial.print("Altitude  : ");
    Serial.print(
      gps.altitude.meters()
    );
    Serial.println(" m");

    Serial.print("Satellites: ");
    Serial.println(
      gps.satellites.value()
    );

    Serial.print("HDOP      : ");
    Serial.println(
      gps.hdop.hdop()
    );

  } else {

    Serial.println("Status    : SEARCHING");

    Serial.print("Satellites: ");
    Serial.println(
      gps.satellites.value()
    );
  }

  Serial.println("==========================================");

  delay(2000);
}