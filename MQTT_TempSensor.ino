
#define SKETCH_PRODUCT  "MQTT TempSensor"
#define SKETCH_VERSION  "2022-01-29"
#define SKETCH_ABOUT    "Publish Temp and Hum to MQTT"
#define SKETCH_FILE     __FILE__

//  ABOUT:
//  Read sensor data and publish to MQTT broker on regular intervals (1 min).
//  Provides a Web server to display sensor and meta data.
//
//  HARDWARE:
//  - D1 Mini V3.1.0, or D1 Mini Pro V2.0.0
//  - LOLIN SHT30 Shield V2.1.0 
//  - [Optional] Battery 
//
//  CONNECTIONS:
//  - SHT30 stacked to D1 Mini (Pro) or via i2c-cable (preferred)
//  - ON D1 Mini Pro V2.0.0: Solder (short) BAT-A0 pads to be able to read Vbat 
//
//  NOTE:
//  Sketch tested OK with pubsubclient=2.8.0 and esp8266=2.7.1 (with esp8266=2.5 tz setting will not compile)
//  Sketch *CRASHES* with pubsublient=2.8.0 and esp8266=3.0.2
//
//  HISTORY:
//  2022-01-18  First version
//  2022-01-19  Additions
//  2022-01-29  Cleaning up ... (New Arduino IDE 2.0 Beta used. Required to update ESP8266 board from ver 2.5 to 2.7.1 (!))
//
//  AUTHOR:
//  Jonas Byström, https://github.com/jonasbystrom
//

// -----------------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------------
//#include <WiFiManager.h>            // https://github.com/tzapu/WiFiManager    
#include <ESP8266WiFi.h>
#include "secrets.h"
const char *ssid                      = STASSID;            // defined in secrets.h
const char *password                  = STAPSK; 


// -----------------------------------------------------------------------------------
// NTP Time Server
// -----------------------------------------------------------------------------------
#include <time.h>                     // time() ctime()

/* Configuration of NTP */
#define MY_NTP_SERVER "se.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"      // Swedish summertime definition

/* Globals */
time_t now;                           // epoch (unix) time
tm tm;                                // time structure



// -----------------------------------------------------------------------------------
// Web server
// -----------------------------------------------------------------------------------
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
ESP8266WebServer server(80);  

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
char mqtt_client[40];                                   // will be set to  mac addr

int  mqttPublishCount                 = 0;
int  mqttConnectTries                 = 0;
int  mqttConnectCount                 = 0;
int  mqttConnectFailed                = 0;
int  wifiConnectTries                 = 0;
int  wifiConnectCount                 = 0;


// -----------------------------------------------------------------------------------
// MQTT Topics
// Set UNIT for each sample to make them unique
// The DEVICE is the (resulting) topic identifier in MQTT
// NAME is just a friendly name to easier recognize the unit is MQTT lists etc
// -----------------------------------------------------------------------------------
#ifndef device
  #define product                       "MQTT-Tempsensor"
  #define unit                          "1"
  #define device                        product "/" unit "/"    // "MQTT-Tempsensor/1/"
  #define name                          "Bedroom"               // set any "friendly name" of your choice
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
const char *topic_version             = device "version";
const char *topic_file                = device "file";
const char *topic_compiled            = device "compiled";
const char *topic_started             = device "started";
const char *topic_ip_address          = device "ip_address";
const char *topic_mac                 = device "mac";
const char *topic_ssid                = device "ssid";
const char *topic_rssi                = device "rssi";
const char *topic_wifi_connect_count  = device "wifi_connect_count";
const char *topic_mqtt_connect_count  = device "mqtt_connect_count";
const char *topic_mqtt_connect_tries  = device "mqtt_connect_tries";
const char *topic_mqtt_publish_count  = device "mqtt_publish_count";
const char *topic_uptime              = device "uptime";
const char *topic_time                = device "time";


// -----------------------------------------------------------------------------------
//   WEMOS SHT30 Temp and Humidity sensor
// -----------------------------------------------------------------------------------
#include <WEMOS_SHT3X.h>
SHT3X sht30(0x45);


// -----------------------------------------------------------------------------------
// OTA Includes
// -----------------------------------------------------------------------------------
#include <ArduinoOTA.h>


// -----------------------------------------------------------------------------------
// Task timer
// -----------------------------------------------------------------------------------
//#include <timer.h>     // worked with esp8266 2.7.1, does not work now esp8266 3.... (?)
#include <arduino-timer.h>
auto timer = timer_create_default();  // create a timer with default settings


// -----------------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------------
String startedTime;
long lastReconnectAttempt = 0;

float sht30_temp          = NAN;
float sht30_humidity      = NAN;
float Vbat                = NAN;


// -----------------------------------------------------------------------------------
void setup()
// -----------------------------------------------------------------------------------
{
  pinMode(LED_BUILTIN, OUTPUT);           // Initialize the LED_BUILTIN pin as an output
  digitalWrite (LED_BUILTIN, !HIGH);      // Led ON

  WiFi.mode(WIFI_STA);                    // explicitly set mode, esp defaults to STA+AP
  
  Serial.begin(115200);
  while (!Serial){}
  delay(1000);                            // D1 Mini sometimes need this extra delay ...
  Serial.println("\n");
  Serial.println("=====================================");
  Serial.println(SKETCH_PRODUCT);
  Serial.println(SKETCH_VERSION);
  Serial.println(SKETCH_ABOUT);
  Serial.println("=====================================");
  Serial.println("");
  Serial.println(String(name));
  Serial.println("Device: "+String(device));
  Serial.flush();

#ifdef EXCLUDE      
  //------------------------------------------------------------------
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe credentials for testing
  // wm.resetSettings();

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
#endif

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
  
  // Web server init -------------------------------
  if (MDNS.begin(device)) {
    Serial.println("MDNS responder started");
  }
  Serial.print("Starting HTTP server ... ");
  server.on("/", handleRoot);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("OK");
  
  // Initialize NTP --------------------------------
  Serial.print("Initializing NTP ... ");
  configTime(MY_TZ, MY_NTP_SERVER); 
  // by default, the NTP will be started after 60 secs
  Serial.println(" OK");

  // Connecting to the mqtt broker -----------------
  Serial.print("Connecting to MQTT broker ...");  
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(onMqttReceive);
  Serial.println(" OK");  

  // Tasks -----------------------------------------
  #define SECS 1000
  timer.every( 1*60 * SECS,   readAndPublishData);         // every 1 mins

  // OTA -------------------------------------------
  ArduinoOTA.setHostname (device);
  ArduinoOTA.begin();
  
  //delay(5000);
  do {                        // wait until we have time - potentially blocking
    time (&now);
  } while (now > 1000); 
  startedTime = dateString() + ", " + timeString();  
  Serial.println("Started: " + startedTime);
  
  digitalWrite (LED_BUILTIN, !LOW);      // Led OFF
}


// -----------------------------------------------------------------------------------
void loop()
// -----------------------------------------------------------------------------------
{
  timer.tick();                                 // poll task timer

  if (!client.connected()) {                    // if not connected to mqtt broker ...
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {    // wait 5 secs between tries
      lastReconnectAttempt = now;
      mqttConnectTries++;
      // Attempt to reconnect
      if (mqttReconnect()) {                    // try to reconnect
        lastReconnectAttempt = 0;
        mqttConnectCount++;
      }
    }
  } else {
    // MQTT Client connected
    client.loop();                              // mqtt poll
  }

  server.handleClient();                        // web server poll
  MDNS.update();

  ArduinoOTA.handle();                          // OTA handling poll     
}


// -----------------------------------------------------------------------------------
boolean mqttReconnect() 
{
  strcpy (mqtt_client, WiFi.macAddress().c_str());      // set client ID to MAC addr, to ensure unique at Broker
  int res = client.connect(mqtt_client, mqtt_username, mqtt_password);
  if (res) {
    // subscribe to all/any wanted topics here ...
    // you should re-subscribe after each new (re-)connection
    // client.subscribe(topic_whatever_a);        
    // client.subscribe(topic_whatever_b);
    // client.subscribe(topic_...);        
  }
  return client.connected();
}


// -----------------------------------------------------------------------------------
bool readAndPublishData (void *)
// -----------------------------------------------------------------------------------
{ 
  digitalWrite (LED_BUILTIN, !HIGH);      // Led ON
  
  readSensorData();
  publishData();

  digitalWrite (LED_BUILTIN, !LOW);      // Led OFF
}


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

  client.publish(topic_product, SKETCH_PRODUCT);  
  Serial.print  (topic_product); Serial.print (":       "); Serial.println (SKETCH_PRODUCT); 
  
  client.publish(topic_version, SKETCH_VERSION);  
  Serial.print  (topic_version); Serial.print (":    "); Serial.println (SKETCH_VERSION); 

  client.publish(topic_file, SKETCH_FILE);  
  Serial.print  (topic_file); Serial.print (":       "); Serial.println (SKETCH_FILE); 

  client.publish(topic_compiled, __DATE__);  
  Serial.print (topic_compiled); Serial.print (":   "); Serial.println (__DATE__); 

  client.publish(topic_started, startedTime.c_str());  
  Serial.print (topic_started); Serial.print (":    "); Serial.println (startedTime.c_str()); 

  sprintf (str, "%d seconds", millis()/1000);
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

  strcpy (str, timeString().c_str());
  client.publish(topic_time, str);  
  Serial.print(topic_time); Serial.print(":       "); Serial.println(str);  
  
  sprintf (str, "%d", mqttConnectCount);
  client.publish(topic_mqtt_connect_count, str);  
  Serial.print(topic_mqtt_connect_count); Serial.print(": "); Serial.println(str);  

  sprintf (str, "%d", mqttConnectTries);
  client.publish(topic_mqtt_connect_tries, str);  
  Serial.print(topic_mqtt_connect_tries); Serial.print(": "); Serial.println(str);  

  mqttPublishCount++;
  sprintf (str, "%d", mqttPublishCount);
  client.publish(topic_mqtt_publish_count, str);  
  Serial.print(topic_mqtt_publish_count); Serial.print(": "); Serial.println(str);  
}


// Callback for subscribed topics
// -----------------------------------------------------------------------------------
void onMqttReceive(char *topic, byte *payload, unsigned int length) {
  Serial.print("MQTT message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
      Serial.print((char) payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}


// -----------------------------------------------------------------------------------
void handleRoot() 
// -----------------------------------------------------------------------------------
{
  Serial.println ("Webserver handleRoot");
  
  String s;
  //
  // table design example: https://www.w3schools.com/css/css_table.asp
  //
  s = "<!DOCTYPE html><html>";
  s += "<style> \
    h1 {text-align:center; font-family:verdana, arial, sans-serif; font-size:30px;} \
    table.center { \
      margin-left:auto; \ 
      margin-right:auto; \
    } \
    #topics { \
      font-family: Arial, Helvetica, sans-serif; \
      border-collapse: collapse; \
      w_idth: 100%; \
    } \
    #topics td, #topics th { \
      b_order: 1px solid #ddd; \
      padding: 6px; \
      font-size:0.9em; \
    } \
    #topics tr:nth-child(even){background-color: #f2f2f2;} \
    #topics tr:hover {background-color: #ddd;} \
    #topics th { \
      padding-top: 6px; \
      padding-bottom: 6px; \
      text-align: left; \
      background-color: #04AA6D; \
      color: white; \
    } ";
  s += "</style><head>";
  s += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>";
  s += "<meta name='viewport' content='width=320'>";
  s += "<title>" +String(device)+ "</title>";
  s += "</head>";
  s += "<body><h1>" + String(device) + "</h1>";
  s += "<table class=center id=topics>";
  s += "<tr><th> <b>Topic</b>   </th> <th> Value                                             </th></tr>";  
  s += "<tr><td> <b>Name</b>    </td> <td>    " + String(name) +                            "</td></tr>";
  s += "<tr><td> <b>Temp</b>    </td> <td> <b>" + String( sht30_temp, 1 ) + " &deg;C </b>    </td></tr>";
  s += "<tr><td> <b>Hum</b>     </td> <td> <b>" + String( sht30_humidity, 1 ) + " %  </b>    </td></tr>";
  s += "<tr><td> <b>Vbat</b>    </td> <td> <b>" + String( Vbat ) + " V </b>                  </td></tr>";
  s += "<tr><td> <b>Product</b> </td> <td>    " + String(SKETCH_PRODUCT) +                  "</td></tr>";
  s += "<tr><td> <b>Date</b>    </td> <td>    " + String(SKETCH_VERSION) +                  "</td></tr>";
  s += "<tr><td> <b>About</b>   </td> <td>    " + String(SKETCH_ABOUT) +                    "</td></tr>";
  s += "<tr><td> <b>Device</b>  </td> <td>    " + String(device) +                          "</td></tr>";
  s += "<tr><td> File           </td> <td>    " + String(__FILE__) +                        "</td></tr>";
  s += "<tr><td> Compiled       </td> <td>    " + String(__DATE__)  +                       "</td></tr>";
  s += "<tr><td> Started        </td> <td>    " + String( startedTime )  +                  "</td></tr>";
  s += "<tr><td> Now            </td> <td>    " + dateString() + ", " + timeString() +      "</td></tr>";
  s += "<tr><td> Wifi Network   </td> <td>    " + String(WiFi.SSID())  +                    "</td></tr>";
  s += "<tr><td> Signal level   </td> <td>    " + String(WiFi.RSSI()) + " dBm"  +           "</td></tr>";
  s += "<tr><td> IP address     </td> <td>    " + WiFi.localIP().toString()  +              "</td></tr>";
  s += "<tr><td> OTA            </td> <td>    " + String("OTA IDE enabled:")+String(device)+"</td></tr>"; 
  s += "<tr><td> <u>MQTT</u>    </td> <td>                                                   </td></tr>";
  s += "<tr><td> Connect Count  </td> <td>    " + String (mqttConnectCount)  +              "</td></tr>";
  s += "<tr><td> Connect Tries  </td> <td>    " + String (mqttConnectTries)  +              "</td></tr>";
  s += "<tr><td> Publish Count  </td> <td>    " + String (mqttPublishCount)  +              "</td></tr>";
  s += "<tr><td>                </td> <td>                                                   </td></tr>";
  s += "</table></boby></html>";
  
  server.send(200, "text/html", s);
  Serial.println(s);
}

// -----------------------------------------------------------------------------------
void handle_NotFound()
{
  Serial.println ("404 - Not Found");
  server.send(404, "text/plain", "Not found");
}


// -----------------------------------------------------------------------------------
// Get DateString    as YYYY-MM-DD
// -----------------------------------------------------------------------------------
String dateString() 
{
  time(&now);
  localtime_r(&now, &tm);
                                            // strftime formats: https://www.cplusplus.com/reference/ctime/strftime/
  char buf[80];
  strftime (buf, 80, "%Y-%m-%d", &tm);      // can also use shorter: "%F"
  return String(buf);
}

// -----------------------------------------------------------------------------------
// Get TimeString as HH:MM:SS
// -----------------------------------------------------------------------------------
String timeString() 
{
  time(&now);
  localtime_r(&now, &tm);

  char buf[80];
  strftime (buf, 80, "%H:%M:%S", &tm);
  return String(buf);
}  
