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

  // Start SPS30 measurement
  error = sps.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
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

  delay(2000);
}

void loop() {

  delay(2000);

  // Read SPS30
  uint16_t dataReadyFlag = 0;
  error = sps.readDataReadyFlag(dataReadyFlag);
  if (error != NO_ERROR) {
    Serial.print("SPS30 error reading data-ready flag: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else if (dataReadyFlag) {
    uint16_t mc1p0 = 0, mc2p5 = 0, mc4p0 = 0, mc10p0 = 0;
    uint16_t nc0p5 = 0, nc1p0 = 0, nc2p5 = 0, nc4p0 = 0, nc10p0 = 0;
    uint16_t typicalParticleSize = 0;

    error = sps.readMeasurementValuesUint16(mc1p0, mc2p5, mc4p0, mc10p0,
                                             nc0p5, nc1p0, nc2p5, nc4p0,
                                             nc10p0, typicalParticleSize);
    if (error != NO_ERROR) {
      Serial.print("SPS30 error reading measurement: ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
    } else {
      Serial.print("PM  1.0: ");
      Serial.println(mc1p0);
      Serial.print("PM  2.5: ");
      Serial.println(mc2p5);
      Serial.print("PM  4.0: ");
      Serial.println(mc4p0);
      Serial.print("PM 10.0: ");
      Serial.println(mc10p0);
      Serial.print("Typical particle size: ");
      Serial.println(typicalParticleSize);
    }
  } else {
    Serial.println("SPS30 data not ready");
  }

  // Read SCD30
  if (airSensor.dataAvailable()) {
    Serial.print("co2(ppm):");
    Serial.print(airSensor.getCO2());
    Serial.print(" temp(C):");
    Serial.print(airSensor.getTemperature(), 1);
    Serial.print(" humidity(%):");
    Serial.print(airSensor.getHumidity(), 1);
    Serial.println();
  } else {
    Serial.println("SCD30 waiting for data");
  }

  Serial.println();
}
