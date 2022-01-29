# MQTT TempSensor
Temperature sensor (SHT30) running on a D1 Mini Pro V2.0.0 (esp82666) publishing sensor data to local MQTT broker (mosquitto). I have provided 2 versions, one "normal" and one operating in Deep Sleep better suiting for battery operations.

## MQTT-TempSensor:
- 5V USB version.
- Publishing sensor data regularly (every 1 min) to MQTT Broker
- Providig a web server to read sensor data thru a web browser.
- Supports OTA to download new sketch over Wifi from Arduino IDE

## MQTT-TempSensor-DeepSleep:
- Battery version.
- Publishing sensor data regularly (every 5 mins) to MQTT Broker.
- Goes to Deep Sleep between readings (5 mins). And can therefore:
- No support of Web server
- No support of OTA

This temp sensor does not use ESP-Now (see other repositiory for such functionality) but instead send sensor data to a (local) MQTT Broker over WiFi. It is tested with Mosquitto MQTT Broker on a local Raspberry PI connected to the same LAN on Ethernet.

Code is designed for Arduino IDE and assumes ESP8266 MCU. For ESP32, wifi libraries and methods have to be updated accordingly.

_Note. For Arduino IDE, you must store the skecth code in different folders to comply with the Arduino rule "folder and sketch must have same name"._
