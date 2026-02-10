#include <sps30.h>
#include <Wire.h>

#include "SparkFun_SCD30_Arduino_Library.h"  //Click here to get the library: http://librarymanager/All#SparkFun_SCD30

SCD30 airSensor;


int16_t ret;
uint8_t auto_clean_days = 4;
uint32_t auto_clean;
struct sps30_measurement m;
char serial[SPS30_MAX_SERIAL_LEN];
uint16_t data_ready;

void setup() {
  Serial.begin(115200);

  //scd init
  Wire.begin();

  if (airSensor.begin() == false) {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }

  //start sps init
  sensirion_i2c_init();
  while (sps30_probe() != 0) {
    Serial.print("SPS sensor probing failed\n");
    delay(500);
  }
  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);  // Used to drive the fan for pre-defined sequence every X days
  if (ret) {
    Serial.print("error setting the auto-clean interval: ");
    Serial.println(ret);
  }
  ret = sps30_start_measurement();  // Configures device ready for read every 1 second
  if (ret < 0) {
    Serial.print("error starting measurement\n");
  }
  delay(1000);
}

void loop() {

  sps30_start_measurement();  // Start of loop start fan to flow air past laser sensor
  delay(2000);                //Wait 1 second while fan is active before read data

  do {
    ret = sps30_read_data_ready(&data_ready);  // Reads the last data from the sensor
    if (ret < 0) {
      Serial.print("error reading data-ready flag: ");
      Serial.println(ret);
    } else if (!data_ready)
      Serial.print("data not ready, no new measurement available\n");
    else
      break;
    delay(100); /* retry in 100ms */
  } while (1);
  ret = sps30_read_measurement(&m);  // Ask SPS30 for measurments over I2C, returns 10 sets of data

  Serial.print("PM  1.0: ");
  Serial.println(m.mc_1p0);
  Serial.print("PM  2.5: ");
  Serial.println(m.mc_2p5);
  Serial.print("PM  4.0: ");
  Serial.println(m.mc_4p0);
  Serial.print("PM 10.0: ");
  Serial.println(m.mc_10p0);
  //Serial.print("NC  0.5: ");
  //Serial.println(m.nc_0p5);
  //Serial.print("NC  1.0: ");
  //Serial.println(m.nc_1p0);
  //Serial.print("NC  2.5: ");
  //Serial.println(m.nc_2p5);
  //Serial.print("NC  4.0: ");
  // Serial.println(m.nc_4p0);
  // Serial.print("NC 10.0: ");
  // Serial.println(m.nc_10p0);
  Serial.print("Typical partical size: ");
  Serial.println(m.typical_particle_size);
  Serial.println();

  sps30_stop_measurement();  //Disables Fan before 11 seconds gap

  if (airSensor.dataAvailable()) {
    Serial.print("co2(ppm):");
    Serial.print(airSensor.getCO2());

    Serial.print(" temp(C):");
    Serial.print(airSensor.getTemperature(), 1);

    Serial.print(" humidity(%):");
    Serial.print(airSensor.getHumidity(), 1);

    Serial.println();
  } else
    Serial.println("Waiting for new data");

  Serial.println();
  delay(3000);
}
