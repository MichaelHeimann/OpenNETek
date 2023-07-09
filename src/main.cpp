#include <FS.h>
#include <string.h>

#ifdef ESP8266
#else
#include <SPIFFS.h>
#endif

#include <NETSGPClient.h>
#include <Arduino.h>


#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#include <esp_wifi.h>
#endif


#include <WiFiClient.h>

#include <ESPAsync_WiFiManager.h>

#ifdef ESP8266
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif



#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include <AsyncElegantOTA.h>


#ifdef ESP8266
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif


// to disable brownout detection
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"


#define LED 2
#define DEBUGING 0
#define WEBSERIAL 0

#if WEBSERIAL == 1
#include <WebSerial.h>
#define webdebug(x) WebSerial.print(x) 
#define webdebugln(x) WebSerial.println(x)
#else
#define webdebug(x) 
#define webdebugln(x)
#endif

#if DEBUGING == 1
// using Serial for debugging output only when DEBUG is 1
#define debug(x) Serial.print(x) 
#define debugln(x) Serial.println(x)
#define debugln2(x,y) Serial.println(x,y)
#else
#define debug(x)
#define debugln(x)
#define debugln2(x,y)
#endif

//Options start

bool ledpinon = false;

// Add your MQTT Broker Data:
char mqtt_server[40];     //mqtt Broker ip
uint16_t mqtt_port;       //mqtt Broker port
char mqtt_user[32];       //mqtt Username
char mqtt_password[32];   //mqtt Password
char mqtt_topic[20];      //mqtt main topic of this Inverter

uint32_t inverterID = 0; // will contain the identifier of your inverter (see label on inverter, but be aware that its in hex!)

// Hostname of the ESP itself
char hostname[20];

// WiFi variables for config HTTP POST.
String ssid;
String pass;

// for the loop to not run as fast as possible but every X seconds
uint32_t lastSendMillis = 0;


// Proper Wifi Managment starts here. 
// Copied with small changes from https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiIPv6/WiFiIPv6.ino
// Kudos!

static volatile bool wifi_connected = false;

void wifiOnDisconnect(){
    debugln("STA Disconnected");
    delay(1000);
    WiFi.begin(ssid.c_str(), pass.c_str());
}


void WiFiEvent(WiFiEvent_t event){
    
    #ifdef ESP8266
      switch(event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            wifi_connected = false;
            wifiOnDisconnect();
            break;
        default:
            break;
      }
    #else
    switch(event) {

    

        case ARDUINO_EVENT_WIFI_AP_START:
            //can set ap hostname here
            WiFi.softAPsetHostname(hostname);
            //enable ap ipv6 here
            WiFi.softAPenableIpV6();
            break;

        case ARDUINO_EVENT_WIFI_STA_START:
            //set sta hostname here
            WiFi.setHostname(hostname);
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            //enable sta ipv6 here
            debugln("Connected to Wifi.");
            WiFi.enableIpV6();
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            debug("STA IPv6: ");
            debugln(WiFi.localIPv6());
            wifi_connected = true; // IPv6 is sufficient to be "connected"
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            debug("AP IPv6: ");
            debugln(WiFi.softAPIPv6());
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            debug("STA IPv4: ");
            debugln(WiFi.localIP());
            wifi_connected = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifi_connected = false;
            wifiOnDisconnect();
            break;
        default:
            break;
    }
    #endif
    
}


// Proper Wifi Managment functions end here


//options end

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

// Parameter für Config POST
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "mqtt-server";
const char* PARAM_INPUT_4 = "mqtt-port";
const char* PARAM_INPUT_5 = "mqtt-username";
const char* PARAM_INPUT_6 = "mqtt-password";
const char* PARAM_INPUT_7 = "inverter-id";
const char* PARAM_INPUT_8 = "reset";


WiFiClient espClient; /// WiFiClient for MQTT
PubSubClient mqtt(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//variables for mqtt
float temperature = 0;
float dcVoltage1 = 0;
float dcCurrent1 = 0;
float dcPower1 = 0;
float acVoltage1 = 0;
float acCurrent1 = 0;
float acPower1 = 0;
float temperature1 = 0;
float totalGeneratedPower1 = 0;
float state1 = 0;

// Return OpenNETek/<inverterID>/<parameter
char* get_mqtt_topic(const char* subdir){
  //memset(&subdir[0], 0, sizeof(subdir));

  char* topic_tmp = new char[50];
  strncpy(topic_tmp, mqtt_topic, sizeof(mqtt_topic));
  strncat(topic_tmp, subdir, sizeof(subdir));

  return topic_tmp;
}

#ifdef ESP32WROOM
        // Code to be compiled for ESP32WROOM32
        constexpr const uint8_t RX_PIN = 16; /// RX pin
        constexpr const uint8_t TX_PIN = 17; /// TX pin
        #define clientSerial Serial2
#endif

#ifdef ESP32C3
        // Code to be compiled for ESP32C3
        constexpr const uint8_t RX_PIN = 6; /// RX pin 
        constexpr const uint8_t TX_PIN = 7; /// TX pin
        #define clientSerial Serial1 /// ESP32C3 does not have Serial2. Serial1 should work on GPIO6 and 7.
#endif

#ifdef ESP8266
        // Code to be compiled for ESP8266
        // Using Serial.swap to use D7(RX) and D8(TX) for UART0.
        // This effectively puts the only available usable UART to Ports D7 and D8 after boot. 
        // So Serial can be used for clean Application Usage. Debugging output on Serial during boot is done on D9 (RX) and D10 (TX).
        // This makes Debugging AND being connected to live Hardware hard/impossible since they have to use the same UART.
        #define clientSerial Serial/// ESP8266  
#endif


constexpr const uint8_t PROG_PIN = 4; /// Programming enable pin of RF module (not needed when replacing LC12S with ESP)

NETSGPClient pvclient(clientSerial, PROG_PIN); /// NETSGPClient instance

void callback(char* topic, byte* message, unsigned int length) {
  debug("Message arrived on topic: ");
  debug(topic);
  debug(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    debug((char)message[i]);
    messageTemp += (char)message[i];
  }
  debugln();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == get_mqtt_topic("/led")) {
    debug("Changing output to ");
    if(messageTemp == "on"){
      debugln("on");
      digitalWrite(LED, HIGH);
    }
    else if(messageTemp == "off"){
      debugln("off");
      digitalWrite(LED, LOW);
    }
  }
  if (String(topic) == get_mqtt_topic("/powerlevel")) {
    debug("Trying to set Powerlevel to ");
    debugln(messageTemp);
    int powerlevel;
    NETSGPClient::PowerGrade pg;
    powerlevel = strtol(messageTemp.c_str(),NULL,10);
    if (powerlevel >= 0 && powerlevel <=100){
      pg = static_cast<NETSGPClient::PowerGrade>(powerlevel); // convert int "powerlevel" to PowerGrade "pg"
      if (pvclient.setPowerGrade(inverterID,pg)){
        debugln("Successfully set PowerGrade.");
      }
      else {
        debugln("Could not set PowerGrade.");
      }
    } else {
        debug(powerlevel);
        debugln( "ist kein gültiger Powerlevel in Prozent (0 - 100)");
    }
  }
  if (String(topic) == get_mqtt_topic("/activate")) {
    if(messageTemp == "on"){
      debugln("Activating Inverter.");
      pvclient.activate(inverterID,true);
    }
    else if(messageTemp == "off"){
      debugln("Deactivating Inverter.");
      pvclient.activate(inverterID,false);
    }
  }
  if (String(topic) == get_mqtt_topic("/reboot")) {
    if(messageTemp == "on"){
      debugln("Rebooting Inverter.");
      pvclient.reboot(inverterID);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    debug("Attempting MQTT connection...");
    webdebug("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(hostname)) {
      debugln("connected");
      webdebug("Attempting MQTT connection...");
      // Subscribe
      mqtt.subscribe(get_mqtt_topic("/led"));
      mqtt.subscribe(get_mqtt_topic("/powerlevel"));
      mqtt.subscribe(get_mqtt_topic("/activate"));
      mqtt.subscribe(get_mqtt_topic("/reboot"));
    } else {
      debug("failed, rc=");
      webdebug("failed, rc=");

      debug(mqtt.state());
      webdebug(mqtt.state());
      debugln(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void recvMsg(uint8_t *data, size_t len){
  webdebugln("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  webdebugln(d);
  if (d == "ON"){
    digitalWrite(LED, HIGH);
  }
  if (d=="OFF"){
    digitalWrite(LED, LOW);
  }
}



// Start WifiManager Config Portal. unsigned long as a Parameter, zero for no timeout.
void do_wifi_config(const unsigned long &timeout){
  bool config_changed = false;
  
  
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESPAsyncWiFiManager ESPAsyncwifiManager(&webServer, &dnsServer);
  // Use this to personalize DHCP hostname (RFC952 conformed)
  
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWebServer tempwebServer(80);


  #ifdef ESP8266
  //DNSServer dnsServer;
  ESPAsync_WiFiManager wifiManager(&tempwebServer, NULL);
  #else
  ESPAsync_WiFiManager wifiManager(&tempwebServer, NULL);
  #endif


  // uncomment to delete WiFi settings after each Reset.
  wifiManager.resetSettings();
  wifiManager.setDebugOutput(true);

  ESPAsync_WMParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  ESPAsync_WMParameter custom_mqtt_port("port", "mqtt port", "1883", 6);
  ESPAsync_WMParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  ESPAsync_WMParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 32);
  ESPAsync_WMParameter custom_inverterID("InverterID", "InverterID",String(inverterID, 16).c_str() , 9); // as printed on the label of the inverter which is in Hex  
  
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_inverterID);
  
  if (timeout > 0){
    // timeout shall be set
    wifiManager.setConfigPortalTimeout(timeout);
    

    // This is an "if" and not a "while" to let the timeout stop WiFiManager and continue. 
    // Usecase: WiFi-config correct but not reachable right now.
    // This leaves the once working WiFi config configured and will reconnect when the APs come back.
    if (!wifiManager.startConfigPortal("OpenNETek", "opennetek!")) {
      // returns false when timeout is reached or connect to given Wifi didn't work.
      debugln("Renewing configuration wasn't successful, either because it couldn't connect to the given WiFi or because the timeout was reached.");
      }
    } else {
      // timeout is 0
      // Initial Configuration starts WiFiManager with timeout 0, a working Wifi config is mandatory.
      while (!wifiManager.autoConnect("OpenNETek", "opennetek!")) {
        debugln("Could not connect to specified SSID. Launching WifiManager again.");
        config_changed = true;
        }
    }

  if (ssid == wifiManager.getSSID() && pass == wifiManager.getPW()) {
      // didn't get new Wifi credentials
      debugln("Saved credentials are the same as the set from WifiManager");
    }
    else {
      // got new Wifi config
      debugln("Saved credentials are different from the set from WifiManager");
      config_changed = true;
      ssid = wifiManager.getSSID();
      pass = wifiManager.getPW();
    }
  

  strcpy (mqtt_server, custom_mqtt_server.getValue() );
  mqtt_port = strtol(custom_mqtt_port.getValue(),NULL,10);
  strcpy (mqtt_user, custom_mqtt_user.getValue() );
  strcpy (mqtt_password, custom_mqtt_password.getValue() );
  inverterID = strtol(custom_inverterID.getValue(),NULL,16);
  debug("saving inverterID as ");
  debugln2(inverterID,16);
  // didn't help with releasing the socket on port 80
  // wifiManager.server.release();

  // save the custom parameters to FS
  debugln("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["wifi_ssid"] = ssid;
  json["wifi_pass"] = pass;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_password"] = mqtt_password;
  json["custom_inverterID"] = inverterID;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    debugln("failed to open config file for writing");
  }
  if(DEBUGING){
    json.printTo(Serial);
  }
  json.printTo(configFile);
  configFile.close();
  debugln("config.json saved - restarting with new config.");

  //end save
  // wanted to only restart if config_changed, but need to
  // restart to free up port 80 and avoid AsyncTCP.cpp:1268] begin(): bind error: -8
  ESP.restart();

}





String processor(const String& var){
  {
  debugln("processor called");

  if(var == "DC_Voltage"){
    return String(dcVoltage1);
    }
  if(var == "DC_Current"){
    return String(dcCurrent1);
    }
  if(var == "DC_Power"){
    return String(dcPower1);
    }
  if(var == "AC_Voltage"){
    return String(acVoltage1);
    }
  if(var == "AC_Current"){
    return String(acCurrent1);
    }
  if(var == "AC_Power"){
    return String(acPower1);
    }
  if(var == "Temperature"){
    return String(temperature1);
    }
  if(var == "Power_gen_total"){
    return String(totalGeneratedPower1);
    }
  if(var == "Status"){
    return String(state1);
    }

  //default catchall
  return String();
  }
}

String config_processor(const String& var){
  {
  //Serial.println("Inverter function returned");
  if(var == "SSID"){
    return String(WiFi.SSID());
    }
  if(var == "MQTT-SERVER"){
    return String(mqtt_server);
    }
  if(var == "MQTT-PORT"){
    return String(mqtt_port, 10);
    }
  if(var == "MQTT-USERNAME"){
    return String(mqtt_user);
    }
  if(var == "INVERTER-ID"){
    return String(inverterID, 16);
    }

  //default catchall
  return String();
  }
}


const char config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
  <head>
    <title>OpenNETek Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v6.2.1/css/all.css" integrity="sha384-twcuYPV86B3vvpwNhWJuaLdUSLF9+ttgM2A6M870UYXrOsxKfER2MKox5cirApyA" crossorigin="anonymous">  
    <link rel="icon" href="data:,">
    <style>
      #config-table { width:100%%; display: table; }
      #table-body { display: table-row-group; }
      .table-row { display: table-row; }
      .table-cell { width:40%%; display: table-cell;}
      html {font-family: Arial; display: inline-block; text-align: center;}
      p { font-size: 1.2rem;}
      body {  margin: 0;}
      .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; margin-bottom: -0.5rem; }
      .menu { overflow: hidden; background-color: #90C8C4; color: white; font-size: 1rem; }
      .content { padding: 20px; }
      .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
      .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
      .reading { font-size: 1.4rem; }
    </style>
  </head>
  <body>
  <div class="topnav">
    <h1>Configuration</h1>
  </div>
  <div class="menu">
    <a href="/"><i class="fa-solid fa-circle-info" style="color:#e1e437;"></i></a>
  </div>
  <div class="content">
    <form action="/config" method="POST">
      <div class="cards">
        <div class="card">
          <p>
          <div id="config-table">
            <div id="table-body">
              <div class="table-row">
                <div class="table-cell">
                  <label for="ssid">SSID</label>
                </div>
                <div class="table-cell">
                  <input type="text" id="ssid" name="ssid" value="%SSID%"><br>
                </div>
              </div>
              <div class="table-row">
                <div class="table-cell">
                  <label for="pass">Password</label>
                </div>
                <div class="table-cell">
                  <input type="password" id="pass" name="pass" value="nochange"><br>
                </div>
              </div>
            </div>
          </div>
          </p>
        </div>
        <div class="card">
          <p>
          <div id="config-table">
            <div id="table-body">
              <div class="table-row">
                <div class="table-cell">
                  <label for="mqtt-server">mqtt-server</label>
                </div>
                <div class="table-cell">
                  <input type="text" id="mqtt-server" name="mqtt-server" value="%MQTT-SERVER%" maxlength="39"><br>
                </div>
              </div>
              <div class="table-row">
                <div class="table-cell">
                  <label for="mqtt-port">mqtt-port</label>
                </div>
                <div class="table-cell">
                  <input type="number" id="mqtt-port" name="mqtt-port" value="%MQTT-PORT%" min="1" max="65535"><br>
                </div>
              </div>
              <div class="table-row">
                <div class="table-cell">
                  <label for="mqtt-username">mqtt-username</label>
                </div>
                <div class="table-cell">
                  <input type="text" id="mqtt-username" name="mqtt-username" value="%MQTT-USERNAME%" maxlength="31"><br>
                </div>
              </div>
              <div class="table-row">
                <div class="table-cell">
                  <label for="mqtt-password">mqtt-password</label>
                </div>
                <div class="table-cell">
                  <input type="password" id="mqtt-password" name="mqtt-password" maxlength="31"><br>
                </div>
              </div>
            </div>
          </div>
          </p>
        </div>
        <div class="card">
          <p>
          <div id="config-table">
            <div id="table-body">
              <div class="table-row">
                <div class="table-cell">
                  <label for="inverter-id">Inverter-ID</label>
                </div>
                <div class="table-cell">
                  <input type="text" id="inverter-id" name="inverter-id" value="%INVERTER-ID%"><br>
                </div>
              </div>
            </div>
          </div>
          </p>
        </div>
      </div>
    <p>
    <input type="submit" value="Save">
    </p>
    <p>
    <input onclick="return confirm('Are you sure you want to do a factory reset?');" type="submit" name="reset" value="Factory Reset">
    </p>
    </form>
  </div>
</body>
</html>)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>OpenNETek</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="20">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v6.2.1/css/all.css" integrity="sha384-twcuYPV86B3vvpwNhWJuaLdUSLF9+ttgM2A6M870UYXrOsxKfER2MKox5cirApyA" crossorigin="anonymous">  
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; margin-bottom: -0.5rem; }
    .menu { overflow: hidden; background-color: #90C8C4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>OpenNETek 4.0</h1>
  </div>
    <div class="menu">
    <a href="/config"><i class="fa-solid fa-gear" style="color:#e1e437;"></i></a>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p><i class="fa-solid fa-solar-panel" style="color:#e1e437;"></i> DC_Voltage</p><p><span class="reading"><span id="DC_Voltage">%DC_Voltage%</span> V</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-solar-panel" style="color:#e1e437;"></i> DC_Current</p><p><span class="reading"><span id="DC_Current">%DC_Current%</span> A</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-solar-panel" style="color:#e1e437;"></i> DC_Power</p><p><span class="reading"><span id="DC_Power">%DC_Power%</span> W</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-plug-circle-bolt" style="color:#059e8a;"></i> AC_Voltage</p><p><span class="reading"><span id="AC_Voltage">%AC_Voltage%</span> V</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-plug-circle-bolt" style="color:#059e8a;"></i> AC_Current</p><p><span class="reading"><span id="AC_Current">%AC_Current%</span> A</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-plug-circle-bolt" style="color:#059e8a;"></i> AC_Power</p><p><span class="reading"><span id="AC_Power">%AC_Power%</span> W</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-thermometer-half" style="color:#000000;"></i> Temperature</p><p><span class="reading"><span id="Temperature">%Temperature%</span> C</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-badge-check" style="color:#000000;"></i> Power gen total</p><p><span class="reading"><span id="Power_gen_total">%Power_gen_total%</span> kWh</span></p>
      </div>
      <div class="card">
        <p><i class="fa-solid fa-star" style="color:#000000;"></i> Status</p><p><span class="reading"><span id="Status">%Status%</span> </span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('DC_Voltage', function(e) {
  console.log("DC_Voltage", e.data);
  document.getElementById("DC_Voltage").innerHTML = e.data;
 }, false);
 
 source.addEventListener('DC_Current', function(e) {
  console.log("DC_Current", e.data);
  document.getElementById("DC_Current").innerHTML = e.data;
 }, false);
 
 source.addEventListener('DC_Power', function(e) {
  console.log("DC_Power", e.data);
  document.getElementById("DC_Power").innerHTML = e.data;
 }, false);

 source.addEventListener('AC_Voltage', function(e) {
  console.log("AC_Voltage", e.data);
  document.getElementById("AC_Voltage").innerHTML = e.data;
 }, false);

 source.addEventListener('AC_Current', function(e) {
  console.log("AC_Current", e.data);
  document.getElementById("AC_Current").innerHTML = e.data;
 }, false);

 source.addEventListener('AC_Power', function(e) {
  console.log("AC_Power", e.data);
  document.getElementById("AC_Power").innerHTML = e.data;
 }, false);

 source.addEventListener('Temperature', function(e) {
  console.log("Temperature", e.data);
  document.getElementById("Temperature").innerHTML = e.data;
 }, false);

 source.addEventListener('Power_gen_total', function(e) {
  console.log("Power_gen_total", e.data);
  document.getElementById("Power_gen_total").innerHTML = e.data;
 }, false);

 source.addEventListener('Status', function(e) {
  console.log("Status", e.data);
  document.getElementById("Status").innerHTML = e.data;
 }, false);

}
</script>
</body>
</html>)rawliteral";


void setup()
{
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector //didn't fix it
  // set GPIO9 to high so that brownout doesn't lead to bootloader mode
  //gpio_set_level(GPIO_NUM_9, 1); //didn't fix it

  #if (DEBUGING == 1) 
    pinMode(LED, OUTPUT);
  #endif

  Serial.begin(9600);
  #ifdef ESP8266
  #endif
  debugln("Setup starting ...");

  #ifdef ESP8266
  clientSerial.begin(9600, SERIAL_8N1);
  // Code to be compiled for ESP8266
  // Using Serial.swap to use D7(RX) and D8(TX) for UART0.
  // This effectively makes Bootup output of Serial end up on D9(RX) and D10(TX)
  // So Serial can be used for Application Usage afterwards. But not for Debug Output.
  Serial.swap();
    #if (DEBUGING == 1) 
      // For Debugging we need the Serial Output on the default Pins (which is also USB)
    Serial.swap();
    #endif
  #else
  clientSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  #endif
  
  #ifdef ESP32WROOM
    // saving Power on ESP32 WROOM since startup was unreliable without that
    setCpuFrequencyMhz(160);
  #endif
    #ifdef ESP32C3
    // saving Power on ESP32C3
    setCpuFrequencyMhz(80);
  #endif
  
  #ifdef ESP8266
    // prevent: 'esp_err_t' was not declared in this scope during ESP8266 build
  #else
    esp_err_t err;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == !ESP_OK) {
      debugln("ERROR: Could not prevent persistant storage of Wifi Credentials in NVS.");
    }
  #endif

  // Config for all bug wifi-ssid and wifi-password is saved in filesystem
  // If no config file is there, wifimanager is started to create one and restart the ESP.
  // So: Wifi will not connect if the config file is missing
  
  //clean FS, for testing
  //SPIFFS.format();

  debugln("mounting FS...");
  
  if (SPIFFS.begin()) {
    debugln("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      debugln("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        debugln("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (DEBUGING){
          json.printTo(Serial);
        }
        if (json.success()) {
          debugln("\nparsed json");
          ssid = json["wifi_ssid"].as<char*>();
          debug("wifi SSID = ");
          debugln(ssid);
          pass = json["wifi_pass"].as<char*>();
          debug("Wifi password = ");
          debugln(pass);
          strcpy(mqtt_server, json["mqtt_server"]);
          debug("mqtt_server = ");
          debugln(mqtt_server);
          mqtt_port = strtol(json["mqtt_port"],NULL,10);
          debug("mqtt_port = ");
          debugln2(mqtt_port,10);
          strcpy(mqtt_user, json["mqtt_user"]);
          debug("mqtt_user = ");
          debugln(mqtt_user);
          strcpy(mqtt_password, json["mqtt_password"]);
          debug("mqtt_password = ");
          debugln(mqtt_password);
          inverterID = strtol(json["custom_inverterID"],NULL,10);
          // Create mqtt base-topic as "OpenNETek/<InverterID>"  
          char buffer[9];
          sprintf(buffer, "%x", inverterID);
          strncpy(mqtt_topic, "OpenNETek/", sizeof("OpenNETek/"));
          strncat(mqtt_topic, buffer, sizeof(buffer));
          debug("inverterID = ");
          debugln2(inverterID,16);

        } else {
          debugln("failed to load json config");
        }
      }
    } else {
      // there is no configfile in the filesystem
      // lets get wifimanager to get us some config

      // start WifiManager Config Portal without timeout
      do_wifi_config(0);
    }

  } else {
    debugln("failed to mount FS");
    debugln("Trying to format...");
    if (SPIFFS.format()) {
      debugln("Formatting successful! Restarting...");
      ESP.restart();
    } else {
      debugln("Formatting not successful!");
    }
  }
  
  // Connect to Wifi with ssid with pass
  WiFi.begin(ssid.c_str(), pass.c_str());
  debug("Connecting");

  // If still not connected after 30 seconds, offer a WiFi reconfig through WifiManager with a 10 Minute timeout
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++) {
    debug('.');
    
    if (i==30){
      // waited already 30 seconds, maybe WiFI-config is just not working. Offering a change through WiFiManager, rebooting afterwards

      #ifdef ESP8266
        // prevent: error: 'class ESP8266WiFiClass' has no member named 'removeEvent'
      #else
        WiFi.removeEvent(WiFiEvent);
      #endif
      WiFi.disconnect(false);
      do_wifi_config(600);
    }
    delay(1000);
  }
  // we get here only when WiFi is connected. yay.
  wifi_connected = true; // wasn't set by the event yet, since that's not yet started
  debugln('! Connected !');
  // Start WiFi Monitoring
  WiFi.onEvent(WiFiEvent);

  #ifdef ESP8266
    // prevent: error: 'WIFI_MODE_STA' was not declared in this scope; did you mean 'WIFI_AP_STA'?
  #else
    // only STA mode needed
    WiFi.mode(WIFI_MODE_STA);
  #endif
  

  strcpy(hostname,mqtt_topic);
  
  // convert OpenNETek/<inverterID> to OpenNETek-<inverterID>
  hostname[9]='-';
  debug("hostname is: \"");
  debug(hostname);
  debugln("\"");

  if (!MDNS.begin(hostname)) {
    debugln("Error setting up MDNS responder!");
    // crashes ESP8266
    //webdebugln("Error setting up MDNS responder!");
  }
  debugln("mDNS responder started");
  // crashes ESP8266
  //webdebugln("mDNS responder started");


  #if WEBSERIAL == 1
    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    WebSerial.msgCallback(recvMsg);
  #endif

  // Handle Web Server for /
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });


  // Handle Web Server for /config
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/html", config_html);
    request->send_P(200, "text/html", config_html, config_processor);
  });

  // Handle Web Server for /config
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST Wifi SSID value
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          debug("SSID set to: ");
          debugln(ssid);
        }
        // HTTP POST Wifi password value
        if (p->name() == PARAM_INPUT_2) {
          String nochange = "nochange";
          nochange = p->value().c_str();
          if (nochange != "nochange") {
            pass = nochange;
            debug("Password set to: ");
            debugln(pass);
          }
          
        }
        // HTTP POST MQTT server value
        if (p->name() == PARAM_INPUT_3) {
          strcpy(mqtt_server, p->value().c_str());
          debug("MQTT server set to: ");
          debugln(mqtt_server);
        }
        // HTTP POST MQTT port value
        if (p->name() == PARAM_INPUT_4) {
          mqtt_port = strtol(p->value().c_str(),NULL,10);
          debug("MQTT port set to: ");
          debugln2(mqtt_port,10);
        }
        // HTTP POST MQTT username value
        if (p->name() == PARAM_INPUT_5) {
          strcpy(mqtt_user, p->value().c_str());
          debug("MQTT username set to: ");
          debugln(mqtt_user);
        }
        // HTTP POST MQTT password value
        if (p->name() == PARAM_INPUT_6) {
          strcpy(mqtt_password, p->value().c_str());
          debug("MQTT password set to: ");
          debugln(mqtt_password);
        }
        // HTTP POST inverter-ID value
        if (p->name() == PARAM_INPUT_7) {
          inverterID = strtol(p->value().c_str(),NULL,16);
          debug("Inverter ID set to: ");
          debugln2(inverterID,16);
        }
        // HTTP POST via Factory Reset button
        if (p->name() == PARAM_INPUT_8) {
          debugln("Factory Reset initiated...");
          SPIFFS.remove("/config.json");
          SPIFFS.end();
          request->send(200, "text/plain", "Resetting to factory defaults and rebooting ESP. Connect to SSID OpenNETek to start initial setup.");
          delay(1000);
          ESP.restart();
        }
      }
    }
    // save the custom parameters to FS
    if (SPIFFS.begin()) {

      debugln("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["wifi_ssid"] = ssid;
      json["wifi_pass"] = pass;
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["mqtt_user"] = mqtt_user;
      json["mqtt_password"] = mqtt_password;
      json["custom_inverterID"] = inverterID;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        // config file cannot be opened for writing
        debugln("failed to open config file for writing");
        request->send(200, "text/plain", "Failed to open config file for writing. Cannot save new configuration!");
      } else {
        // config file is opened for writing
        json.printTo(configFile);
        configFile.close();
        if(DEBUGING){
          json.printTo(Serial);
        } 
        SPIFFS.end();
          
        //end save
        request->send(200, "text/plain", "Done. ESP will restart with the new settings");
        // delay(3000); //think we don't need to wait here
        ESP.restart();
      }
    }

  });




  // Start MDNS-SD Dienst
  MDNS.addService("http", "tcp", 80);

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      if(DEBUGING){
        Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
      }
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // OTA via /update
  AsyncElegantOTA.begin(&server);

  server.begin();

  if ( strlen(mqtt_server) ) {
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(callback);
   (mqtt.connect(hostname, mqtt_user, mqtt_password));

  }
  


  lastSendMillis = millis();

  debugln("Setup finished ...");

}




void loop()
{

  #ifdef ESP8266
    // otherwise mdns didn'twork on ESP8266
    MDNS.update();
  #endif


const uint32_t currentMillis = millis();
if (currentMillis - lastSendMillis > 4000)
  {
  lastSendMillis = currentMillis;
  
  #if DEBUGING == 1
  if (ledpinon) {
    digitalWrite(LED,LOW);
    ledpinon = false;  
  } else {
    digitalWrite(LED,HIGH);
    ledpinon = true;  
  }
  #endif

  debug("Sending request now to InverterID ");
  debugln2(inverterID,16);
  webdebug("Sending request now to InverterID (shown as as decimal) ");
  webdebugln(inverterID);

  const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);
  
  if (status.valid)
    {
    webdebugln("*********************************************");
    webdebugln("Received Inverter Status");
    webdebug("Device (shown as decimal): ");
    webdebugln(inverterID);
    webdebugln("Status: " + String(status.state));
    webdebugln("DC_Voltage: " + String(status.dcVoltage) + "V");
    webdebugln("DC_Current: " + String(status.dcCurrent) + "A");
    webdebugln("DC_Power: " + String(status.dcPower) + "W");
    webdebugln("AC_Voltage: " + String(status.acVoltage) + "V");
    webdebugln("AC_Current: " + String(status.acCurrent) + "A");
    webdebugln("AC_Power: " + String(status.acPower) + "W");
    webdebugln("Power gen total: " + String(status.totalGeneratedPower));
    webdebugln("Temperature: " + String(status.temperature));
    // print Status to Serial for Debug
    debugln("*********************************************");
    debugln("Received Inverter Status");
    debug("Device: ");
    debugln2(status.deviceID, 16);
    debugln("Status: " + String(status.state));
    debugln("DC_Voltage: " + String(status.dcVoltage) + "V");
    debugln("DC_Current: " + String(status.dcCurrent) + "A");
    debugln("DC_Power: " + String(status.dcPower) + "W");
    debugln("AC_Voltage: " + String(status.acVoltage) + "V");
    debugln("AC_Current: " + String(status.acCurrent) + "A");
    debugln("AC_Power: " + String(status.acPower) + "W");
    debugln("Power gen total: " + String(status.totalGeneratedPower));
    debugln("Temperature: " + String(status.temperature));
    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(status.dcVoltage).c_str(),"DC_Voltage",millis());
    events.send(String(status.dcCurrent).c_str(),"DC_Current",millis());
    events.send(String(status.dcPower).c_str(),"DC_Power",millis());
    events.send(String(status.acVoltage).c_str(),"AC_Voltage",millis());
    events.send(String(status.acCurrent).c_str(),"AC_Current",millis());
    events.send(String(status.acPower).c_str(),"AC_Power",millis());
    events.send(String(status.temperature).c_str(),"Temperature",millis());
    events.send(String(status.totalGeneratedPower).c_str(),"Power_gen_total",millis());
    events.send(String(status.state).c_str(),"Status",millis());
    

    // update global variables for the webserver (processor)
    dcVoltage1 = (status.dcVoltage);
    dcCurrent1 = (status.dcCurrent);
    dcPower1 = (status.dcPower);
    acVoltage1 = (status.acVoltage);
    acCurrent1 = (status.acCurrent);
    acPower1 = (status.acPower);
    temperature = (status.temperature);
    totalGeneratedPower1 = (status.totalGeneratedPower);
    state1 = (status.state);


    // do the mqtt thing if MQTT server is given and Wifi is connected
    if ( strlen(mqtt_server) && wifi_connected ) 
      {
      webdebugln("MQTT part beginning...");

      if (!mqtt.connected()) 
        {
        debugln("MQTT not connected. Reconecting...");
        webdebugln("MQTT not connected. Reconecting...");

        reconnect();
        }
      mqtt.loop();

      long now = millis();
      if (now - lastMsg > 5000) 
        {
        lastMsg = now;
      
        // Temperature in Celsius
        // Uncomment the next line to set temperature in Fahrenheit 
        // (and comment the previous temperature line)
        //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit
        

        char dcVoltage[8];
        dtostrf(dcVoltage1, 1, 2, dcVoltage);
        debug("Mqtt-dcVoltage: ");
        debugln(dcVoltage);
        mqtt.publish(get_mqtt_topic("/dcVoltage"), dcVoltage);

        char dcCurrent[8];
        dtostrf(dcCurrent1, 1, 2, dcCurrent);
        debug("Mqtt-dcCurrent: ");
        debugln(dcCurrent);
        mqtt.publish(get_mqtt_topic("/dcCurrent"), dcCurrent);

        char dcPower[8];
        dtostrf(dcPower1, 1, 2, dcPower);
        debug("Mqtt-dcPower: ");
        debugln(dcPower);
        mqtt.publish(get_mqtt_topic("/dcPower"), dcPower);

        char acVoltage[8];
        dtostrf(acVoltage1, 1, 2, acVoltage);
        debug("Mqtt-acVoltage: ");
        debugln(acVoltage);
        mqtt.publish(get_mqtt_topic("/acVoltage"), acVoltage);

        char acCurrent[8];
        dtostrf(acCurrent1, 1, 2, acCurrent);
        debug("Mqtt-acCurrent: ");
        debugln(acCurrent);
        mqtt.publish(get_mqtt_topic("/acCurrent"), acCurrent);

        char acPower[8];
        dtostrf(acPower1, 1, 2, acPower);
        debug("Mqtt-acPower: ");
        debugln(acPower);
        mqtt.publish(get_mqtt_topic("/acPower"), acPower);

        char tempString[8];
        dtostrf(temperature, 1, 2, tempString);
        debug("Mqtt-Temperature: ");
        debugln(tempString);
        mqtt.publish(get_mqtt_topic("/temperature"), tempString);

        char totalGeneratedPower[8];
        dtostrf(totalGeneratedPower1, 1, 2, totalGeneratedPower);
        debug("Mqtt-totalGeneratedPower: ");
        debugln(totalGeneratedPower);
        mqtt.publish(get_mqtt_topic("/totalGeneratedPower"), totalGeneratedPower);

        char state[8];
        dtostrf(state1, 1, 2, state);
        debug("Mqtt-state: ");
        debugln(state);
        mqtt.publish(get_mqtt_topic("/state"), state);
        
        }
      }

    }
  else
    {
    debug("Received no Inverter Status from ");
    debugln2(inverterID,16);

    webdebug("Received no Inverter Status from (ID printed with base10) ");
    webdebugln(inverterID);
    
    }
  }
}