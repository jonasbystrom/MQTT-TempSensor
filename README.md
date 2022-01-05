# WiFi_MQTT_Reconnect
ESP8266 example code for a working WiFi and MQTT strategy.

A demonstration code with a strategy to connect and reconnect to WiFi and MQTT. This version will restart ESP if can't connect to either WiFi or MQTT after a certain time or retries. Useable in cases when a MQTT connection is required and no off-line mode is needed.

Principles are; connect and re-connect on connection losses. If (re-)connections can't succeed, restart the ESP.
