# MQTT TempSensor
Temperature sensor (SHT30) running on a D1 Mini Pro V2.0.0 (esp82666) publishing sensor data to local MQTT broker (mosquitto).
- Publishing sensor data regularly to MQTT Broker
- Providig a web server to read sensor data thru a web browser.

This version does not use ESP-Now or Deep Sleep. It is instead a more standard version running stable on USB 5V.
It is fully possible to modify to run in Deep Sleep and roughly run ca 2 months on a standard battery. 
