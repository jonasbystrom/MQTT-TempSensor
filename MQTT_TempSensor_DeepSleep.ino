
#define SKETCH_PRODUCT  "MQTT TempSensor DeepSleep"
#define SKETCH_VERSION  "2022-01-29"
#define SKETCH_ABOUT    "Publish Temp and Hum to MQTT - Deep Sleep version"
#define SKETCH_FILE     __FILE__

//  ABOUT:
//  DEEP SLEEP version suitable for Battery operation.
//
//  Read sensor data and publish to MQTT broker on regular intervals (5 min) and goes to Deep Sleep in between.
//  No Web server, or OTA, is supported (for the Deep Sleep version).
//  Aimed for BATTERY opration. Expect a life time of around 2 months for a 2200 mAh battery.
//
//  (It would be further possible to prolong lifetime by shorten sensor active/reading time. 
//  I have not bothered in this code, but see my ESP-Now repository on how to read SHT30 faster.).
//
//  HARDWARE:
//  - D1 Mini Pro V2.0.0 (D1 Mini V3.1.0)
//  - LOLIN SHT30 Shield V2.1.0 
//  - [Optional, Recommended] Battery 
//
//  CONNECTIONS:
//  - SHT30 stacked to D1 Mini (Pro) or via i2c-cable 
//  - ON D1 Mini Pro V2.0.0: Solder (short) BAT-A0 pads to be able to read Vbat 
//
//  NOTE:
//  Sketch tested OK with pubsubclient=2.8.0 and esp8266=2.7.1 (with esp8266=2.5 tz setting will not compile)
//  Sketch *CRASHES* with pubsublient=2.8.0 and esp8266=3.0.2
//
//  HISTORY:
//  2022-01-29  First version. 
//  
//  AUTHOR:
//  Jonas Byström, https://github.com/jonasbystrom
//

// -----------------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------------
#include <WiFiManager.h>            // https://github.com/tzapu/WiFiManager    
//#include <ESP8266WiFi.h>
#include "secrets.h"


// -----------------------------------------------------------------------------------
// NTP Time Server
// -----------------------------------------------------------------------------------
// _It is a bit "luxury" to retrieve time to time-stamp a sensor reading and publish - 
// in a DEEP SLEEP sensor which is often assumed to run on batteries. 
// Feel free to comment out all timing handling._

#include <time.h>                     // time() ctime()

/* Configuration of NTP */
#define MY_NTP_SERVER "se.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"      // Swedish summertime definition

/* Globals */
time_t now;                           // epoch (unix) time
tm tm;                                // time structure



// -----------------------------------------------------------------------------------
// MQTT Broker
// -----------------------------------------------------------------------------------
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);
 
const char *mqtt_broker               = "raspberrypi";  //"192.168.1.165";
const int mqtt_port                   = 1883;
const char *mqtt_username             = "";
const char *mqtt_password             = "";
char mqtt_client[80];                                   // will be set to  mac addr

int  mqttPublishCount                 = 0;
int  mqttConnectTries                 = 0;
int  mqttConnectCount                 = 0;
int  mqttConnectFailed                = 0;
int  wifiConnectTries                 = 0;
int  wifiConnectCount                 = 0;


// -----------------------------------------------------------------------------------
// MQTT Topics
// Set UNIT for each device sample to make them unique
// The DEVICE is the (resulting) topic identifier in MQTT
// NAME is just a friendly name to easier recognize the unit is MQTT lists etc
// -----------------------------------------------------------------------------------
#ifndef device
  #define product                       "MQTT-Tempsensor-DS"
  #define unit                          "1"
  #define device                        product "/" unit "/"      // "MQTT-Tempsensor-DS/1/"
  #define name                          "Balcony"                 // set any "friendly name" of your choice
#endif
// -----------------------------------------------------------------------------------


// Topics
// Data
const char *topic_temp                = device "temp";
const char *topic_hum                 = device "hum";
const char *topic_vbat                = device "vbat";
// Meta
const char *topic_name                = device "name";
const char *topic_product             = device "product";
const char *topic_file                = device "file";
const char *topic_version             = device "version";
const char *topic_compiled            = device "compiled";
//const char *topic_started             = device "started"; // time started
const char *topic_ip_address          = device "ip_address";
const char *topic_mac                 = device "mac";
const char *topic_ssid                = device "ssid";
const char *topic_rssi                = device "rssi";
const char *topic_wifi_connect_count  = device "wifi_connect_count";
const char *topic_mqtt_connect_count  = device "mqtt_connect_count";
const char *topic_mqtt_connect_tries  = device "mqtt_connect_tries";
const char *topic_mqtt_publish_count  = device "mqtt_publish_count";
const char *topic_uptime              = device "uptime";    // uptime in ms
const char *topic_time                = device "time";      // time now


// -----------------------------------------------------------------------------------
//   WEMOS SHT30 Temp and Humidity sensor
// -----------------------------------------------------------------------------------
#include <WEMOS_SHT3X.h>
SHT3X sht30(0x45);


// -----------------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------------
//String startedTime;
long lastReconnectAttempt = 0;

float sht30_temp          = NAN;
float sht30_humidity      = NAN;
float Vbat                = NAN;

// Read Temp, Hum and VBAT
// -----------------------------------------------------------------------------------
void readSensorData()
{ 
  if (sht30.get() == 0) {
    // read temp sensor
    sht30_temp = sht30.cTemp;
    sht30_humidity = sht30.humidity;    
  } else {
    Serial.println("*** Error reading temp sensor! : ");
  }
  // read battery level (BAT-A0 pad on MCU board must be shorted(=soldered) to connect bat level to A0.)
  Vbat = 4.5*analogRead(A0)/1023.0;  
}


// -----------------------------------------------------------------------------------
void publishData ()
// -----------------------------------------------------------------------------------
{ 
  char str[80];

  Serial.println ("\nPublishing data to MQTT broker:");  

  sprintf (str, "%.1f", sht30_temp); 
  client.publish(topic_temp, str, true);  
  Serial.print (topic_temp); Serial.print (":       "); Serial.print (sht30_temp);  Serial.println("°C");

  sprintf (str, "%.1f", sht30_humidity); 
  client.publish(topic_hum, str, true);  
  Serial.print (topic_hum); Serial.print  (":        "); Serial.print (sht30_humidity); Serial.println("%");
 
  sprintf (str, "%.2f", Vbat); 
  client.publish(topic_vbat, str, true);  
  Serial.print (topic_vbat); Serial.print  (":       "); Serial.print (Vbat); Serial.println("V");  
  
  client.publish(topic_name, name);  
  Serial.print  (topic_name); Serial.print (":       "); Serial.println (name); 

  strcpy (str, timeString().c_str());
  client.publish(topic_time, str);  
  Serial.print(topic_time); Serial.print(":       "); Serial.println(str);  
  
  client.publish(topic_product, SKETCH_PRODUCT);  
  Serial.print  (topic_product); Serial.print (":       "); Serial.println (SKETCH_PRODUCT); 
  
  client.publish(topic_version, SKETCH_VERSION);  
  Serial.print  (topic_version); Serial.print (":    "); Serial.println (SKETCH_VERSION); 

  client.publish(topic_file, SKETCH_FILE);  
  Serial.print  (topic_file); Serial.print (":       "); Serial.println (SKETCH_FILE); 

  client.publish(topic_compiled, __DATE__);  
  Serial.print (topic_compiled); Serial.print (":   "); Serial.println (__DATE__); 

 // client.publish(topic_started, startedTime.c_str());  
 // Serial.print (topic_started); Serial.print (":    "); Serial.println (startedTime.c_str()); 

  sprintf (str, "%d", millis());
  client.publish(topic_uptime, str);  
  Serial.print (topic_uptime); Serial.print (":     "); Serial.println (str); 

  strcpy(str, WiFi.SSID().c_str());
  client.publish(topic_ssid, str);  
  Serial.print (topic_ssid); Serial.print (":       "); Serial.println (str); 

  sprintf (str, "%d", WiFi.RSSI());
  client.publish(topic_rssi, str);  
  Serial.print (topic_rssi); Serial.print (":       "); Serial.println (str); 

  client.publish(topic_ip_address, WiFi.localIP().toString().c_str());  
  Serial.print (topic_ip_address); Serial.print (": "); Serial.println (WiFi.localIP().toString().c_str()); 

  client.publish(topic_mac, WiFi.macAddress().c_str());  
  Serial.print (topic_mac); Serial.print (":        "); Serial.println (WiFi.macAddress().c_str()); 

  sprintf (str, "%d", mqttConnectCount);
  client.publish(topic_mqtt_connect_count, str);  
  Serial.print(topic_mqtt_connect_count); Serial.print(": "); Serial.println(str);  

  sprintf (str, "%d", mqttConnectTries);
  client.publish(topic_mqtt_connect_tries, str);  
  Serial.print(topic_mqtt_connect_tries); Serial.print(": "); Serial.println(str);  
 
}

// -----------------------------------------------------------------------------------
void setup()
// -----------------------------------------------------------------------------------
{
  pinMode(LED_BUILTIN, OUTPUT);           // Initialize the LED_BUILTIN pin as an output
  digitalWrite (LED_BUILTIN, !HIGH);      // Led ON

  WiFi.mode(WIFI_STA);                    // explicitly set mode, esp defaults to STA+AP
  
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("=====================================");
  Serial.println(SKETCH_PRODUCT);
  Serial.println(SKETCH_VERSION);
  Serial.println(SKETCH_ABOUT);
  Serial.println("=====================================");
  Serial.println("");
  Serial.println("Name:   "+String(name));
  Serial.println("Device: "+String(device));
  Serial.flush();

  //------------------------------------------------------------------
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  if(!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
  }
  //------------------------------------------------------------------


#ifdef EXCLUDE
  // connecting to a WiFi network ------------------
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" OK");
  Serial.print("Connected to: ");
  Serial.print(WiFi.SSID() + ", IP: ");
  Serial.println(WiFi.localIP());
#endif

  // Initialize NTP --------------------------------
  Serial.print("Initializing NTP ... ");
  configTime(MY_TZ, MY_NTP_SERVER);
  Serial.println(" OK");

  // Connecting to the mqtt broker -----------------
  Serial.print("Connecting to MQTT broker ...");  
  client.setServer(mqtt_broker, mqtt_port);
  Serial.println(" OK");  

  // Connect to MQTT broker ------------------------
  mqttConnectTries = 0;
  while (!client.connected()) {
    mqttConnectTries++;
    strcpy (mqtt_client, WiFi.macAddress().c_str());      // set client ID to MAC addr, to ensure unique at Broker
    Serial.printf("The client %s connects to the public mqtt broker\n", mqtt_client);
    if (client.connect(mqtt_client, mqtt_username, mqtt_password)) {
      Serial.println("RPi Mosquitto mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
    if (mqttConnectTries >= 20) {
      // cant connect - goto sleep ...
      Serial.println("\nCan't connect to MQTT broker ... EXIT - SLEEP ");
      return; // and goto sleep - quick'n'dirty.
    }
  }

  Serial.println();

  // Read sensor data and Publish
  mqttConnectCount = 1;
  readSensorData();
  publishData();
  client.loop();                          // mqtt poll
  delay(10);

  digitalWrite (LED_BUILTIN, !LOW);       // Led OFF
}


// -----------------------------------------------------------------------------------
void loop()
// -----------------------------------------------------------------------------------
{           
  // Deep Sleep ----------------------------------
  int sleepSecs = 5*60; //SLEEP_SECS + ((uint8_t)RANDOM_REG32/16);    // add random time to avoid traffic jam collisions
  #ifdef DEBUG_SENSOR    
    Serial.printf("Up for %i ms, going to sleep for %i secs...\n", millis(), sleepSecs); 
  #endif
  ESP.deepSleep(sleepSecs * 1000000, RF_NO_CAL);
  delay(1000);
}



// -----------------------------------------------------------------------------------
// Get TimeString as HH:MM:SS
// -----------------------------------------------------------------------------------
String timeString() 
{
  while (time(&now) == -1) {}       // wait until we have time - potentially blocking
  localtime_r(&now, &tm);

  char buf[80];
  strftime (buf, 80, "%H:%M:%S", &tm);
  return String(buf);
}  
