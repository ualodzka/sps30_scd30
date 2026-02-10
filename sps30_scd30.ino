#include <Arduino.h>
#include <SensirionI2cSps30.h>
#include <Wire.h>

#include "SparkFun_SCD30_Arduino_Library.h"

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensirionI2cSps30 sps;
SCD30 airSensor;

static char errorMessage[64];
static int16_t error;
uint32_t auto_clean_interval = 4 * 24 * 3600;  // 4 days in seconds

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Wire.begin();
  Wire.setWireTimeout(25000);  // 25ms timeout (default 1ms is too short for SPS30)

  // SPS30 init
  sps.begin(Wire, SPS30_I2C_ADDR_69);

  // Reset SPS30
  sps.deviceReset();
  delay(100);

  // Read serial number to verify I2C communication
  int8_t serialNumber[32] = {0};
  error = sps.readSerialNumber(serialNumber, 32);
  if (error != NO_ERROR) {
    Serial.print("SPS30 error reading serial: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else {
    Serial.print("SPS30 serial: ");
    Serial.println((const char*)serialNumber);
  }

  // Read firmware version
  uint8_t fwMajor, fwMinor;
  error = sps.readFirmwareVersion(fwMajor, fwMinor);
  if (error == NO_ERROR) {
    Serial.print("SPS30 firmware: ");
    Serial.print(fwMajor);
    Serial.print(".");
    Serial.println(fwMinor);
  } else {
    Serial.print("SPS30 error reading firmware version: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  // Set auto-cleaning interval
  error = sps.writeAutoCleaningInterval(auto_clean_interval);
  if (error != NO_ERROR) {
    Serial.print("SPS30 error setting auto-clean interval: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  // Start SPS30 measurement
  error = sps.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (error != NO_ERROR) {
    Serial.print("SPS30 error starting measurement: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else {
    Serial.println("SPS30 measurement started");
  }

  // SCD30 init
  if (airSensor.begin() == false) {
    Serial.println("SCD30 not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }
  Serial.println("SCD30 detected");

  Serial.println("DATA_START");
  Serial.println("pm25,co2,temp,humidity");

  delay(2000);
}

void loop() {

  delay(2000);

  float pm25 = -1;
  int co2 = -1;
  float temp = -1, humidity = -1;

  // Read SPS30
  uint16_t dataReadyFlag = 0;
  error = sps.readDataReadyFlag(dataReadyFlag);
  if (error != NO_ERROR) {
    Serial.print("# SPS30 error reading data-ready flag: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else if (dataReadyFlag) {
    float mc1p0 = 0, mc2p5 = 0, mc4p0 = 0, mc10p0 = 0;
    float nc0p5 = 0, nc1p0 = 0, nc2p5 = 0, nc4p0 = 0, nc10p0 = 0;
    float typicalParticleSize = 0;

    error = sps.readMeasurementValuesFloat(mc1p0, mc2p5, mc4p0, mc10p0,
                                            nc0p5, nc1p0, nc2p5, nc4p0,
                                            nc10p0, typicalParticleSize);
    if (error != NO_ERROR) {
      Serial.print("# SPS30 error reading measurement: ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
    } else {
      pm25 = mc2p5;
    }
  } else {
    Serial.println("# SPS30 data not ready");
  }

  // Read SCD30
  if (airSensor.dataAvailable()) {
    co2 = airSensor.getCO2();
    temp = airSensor.getTemperature();
    humidity = airSensor.getHumidity();
  } else {
    Serial.println("# SCD30 waiting for data");
  }

  // Output CSV line (only when at least one sensor has data)
  if (pm25 >= 0 || co2 >= 0) {
    if (pm25 >= 0) Serial.print(pm25);
    Serial.print(",");
    if (co2 >= 0) Serial.print(co2);
    Serial.print(",");
    if (temp >= 0) Serial.print(temp, 1);
    Serial.print(",");
    if (humidity >= 0) Serial.print(humidity, 1);
    Serial.println();
  }
}
