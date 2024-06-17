# ESPweather
A weather clock for an ESP32 with a TM1637 7-segment display.

This is primarily driven by [Bremme's SevenSegment TM1637 Arduino Library](https://github.com/bremme/arduino-tm1637).

## Prerequisites

Make sure to set your Wifi credentials in the code, as well as your OpenWeatherMap URL with key and location.

This uses Pin 2, 13, and T8 as a button pin, but you can set these to any pin you like.