# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino sketch that reads air quality data from two I2C sensors:
- **SPS30** (Sensirion) — Particulate matter sensor (PM1.0, PM2.5, PM4.0, PM10.0, typical particle size)
- **SCD30** (Sensirion via SparkFun) — CO2, temperature, and humidity sensor

Both sensors communicate over I2C. The sketch outputs readings to Serial at 115200 baud.

## Build & Upload

This is an Arduino `.ino` project. Compile and upload using:
- **Arduino IDE**: Open `sps30_scd30.ino`, select board/port, upload
- **Arduino CLI**: `arduino-cli compile --fqbn <board> . && arduino-cli upload -p <port> --fqbn <board> .`

## Required Libraries

- `sps30` — Sensirion SPS30 driver (provides `sps30.h`, `sensirion_i2c_init()`)
- `SparkFun_SCD30_Arduino_Library` — SparkFun SCD30 driver

## Architecture

Single-file sketch (`sps30_scd30.ino`) with a standard Arduino `setup()`/`loop()` pattern:
- `setup()`: Initializes I2C, probes both sensors, configures SPS30 auto-clean interval (4 days), starts initial SPS30 measurement
- `loop()`: Starts SPS30 fan, waits for data ready, reads PM values, stops fan, then reads SCD30 CO2/temp/humidity. Repeats every ~5 seconds (2s fan + 3s delay)

The SPS30 fan is started/stopped each loop iteration to reduce wear, with a 2-second spin-up before reading.
