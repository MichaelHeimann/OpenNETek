#include <FS.h>
#include <string.h>
#include <SPIFFS.h>
#include <NETSGPClient.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <WebSerial.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#define LED 2

//Optionen Start

// Add your MQTT Broker Data:
char mqtt_server[40];     //mqtt Broker ip
uint16_t mqtt_port;       //mqtt Broker port
char mqtt_user[32];       //mqtt Username
char mqtt_password[32];   //mqtt Password
char mqtt_topic[20];      //mqtt main topic of this Inverter

uint32_t inverterID = 0; // will contain the identifier of your inverter (see label on inverter, but be aware that its in hex!)

// Hostname of the ESP32 itself
char hostname[20];

// WiFi Variablen in denen die Werte vom Config POST gespeichert werden
String ssid;
String pass;

// Proper Wifi Managment starts here. 
// Copied with small changes from https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiIPv6/WiFiIPv6.ino
// Kudos!

static volatile bool wifi_connected = false;

void wifiOnDisconnect(){
    Serial.println("STA Disconnected");
    delay(1000);
    WiFi.begin(ssid.c_str(), pass.c_str());
}


void WiFiEvent(WiFiEvent_t event){
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
            Serial.println("Connected to Wifi.");
            WiFi.enableIpV6();
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            Serial.print("STA IPv6: ");
            Serial.println(WiFi.localIPv6());
            wifi_connected = true; // IPv6 is sufficient to be "connected"
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            Serial.print("AP IPv6: ");
            Serial.println(WiFi.softAPIPv6());
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("STA IPv4: ");
            Serial.println(WiFi.localIP());
            wifi_connected = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifi_connected = false;
            wifiOnDisconnect();
            break;
        default:
            break;
    }
}


// Proper Wifi Managment functions end here


//Optionen End

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





// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

WiFiClient espClient;
PubSubClient mqtt(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Variable für mqtt
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


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
 //     digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
//      digitalWrite(ledPin, LOW);
    }
  }
}
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      mqtt.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void recvMsg(uint8_t *data, size_t len){
  //WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  //WebSerial.println(d);
  if (d == "ON"){
    digitalWrite(LED, HIGH);
  }
  if (d=="OFF"){
    digitalWrite(LED, LOW);
  }
}

constexpr const uint8_t PROG_PIN = 4; /// Programming enable pin of RF module (needed?)
constexpr const uint8_t RX_PIN = 16; /// RX pin (of RF module)
constexpr const uint8_t TX_PIN = 17; /// TX pin (of RF module)
//constexpr const uint32_t inverterID = 0x38004044; /// Identifier of your inverter (see label on inverter)





NETSGPClient pvclient(Serial2, PROG_PIN); /// NETSGPClient instance


// Start WifiManager Config Portal. unsigned long as a Parameter, zero for no timeout.
void do_wifi_config(const unsigned long &timeout){
  bool config_changed = false;
  
  WiFiManager wifiManager;
  // uncomment to delete WiFi settings after each Reset.
  wifiManager.resetSettings();
  wifiManager.setDebugOutput(true);

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", "1883", 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 32);
  WiFiManagerParameter custom_inverterID("InverterID", "InverterID",String(inverterID, 16).c_str() , 8); // as printed on the label of the inverter which is in Hex
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
      Serial.print("Renewing configuration wasn't successful, either because it couldn't connect to the given WiFi or because the timeout was reached.");
      }
    } else {
      // timeout is 0
      // Initial Configuration starts WiFiManager with timeout 0, a working Wifi config is mandatory.
      while (!wifiManager.autoConnect("OpenNETek", "opennetek!")) {
        Serial.print("Could not connect to specified SSID. Launching WifiManager again.");
        config_changed = true;
        }
    }

  
  
  if (ssid == wifiManager.getWiFiSSID() && pass == wifiManager.getWiFiPass()) {
      // didn't get new Wifi credentials
      Serial.print("Saved credentials are the same as the set from WifiManager");
    }
    else {
      // got new Wifi config
      Serial.print("Saved credentials are different from the set from WifiManager");
      config_changed = true;
      ssid = wifiManager.getWiFiSSID();
      pass = wifiManager.getWiFiPass();
    }
  

  strcpy (mqtt_server, custom_mqtt_server.getValue() );
  mqtt_port = strtol(custom_mqtt_port.getValue(),NULL,10);
  strcpy (mqtt_user, custom_mqtt_user.getValue() );
  strcpy (mqtt_password, custom_mqtt_password.getValue() );
  inverterID = strtol(custom_inverterID.getValue(),NULL,16);
  Serial.print("saving inverterID as ");
  Serial.print(inverterID,16);
  // didn't help with releasing the socket on port 80
  // wifiManager.server.release();

  // save the custom parameters to FS
  Serial.println("saving config");
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
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  delay(1000);

  //end save
  // wanted to only restart if config_changed, but need to
  // restart to free up port 80 and avoid AsyncTCP.cpp:1268] begin(): bind error: -8
  ESP.restart();

}


// Return OpenNETek/<inverterID>/<parameter
char* get_mqtt_topic(const char* subdir){
  //memset(&subdir[0], 0, sizeof(subdir));

  char* topic_tmp = new char[50];
  strncpy(topic_tmp, mqtt_topic, sizeof(mqtt_topic));
  strncat(topic_tmp, subdir, sizeof(subdir));

  return topic_tmp;
}


String processor(const String& var){
  {
  Serial.println("processor called");

  //Serial.println("Inverter function returned");
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

  //Serial.println("default catchall return is coming...");
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

  //Serial.println("default catchall return is coming...");
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
                  <input type="password" id="pass" name="pass"><br>
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
    <h1>OpenNETek</h1>
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
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(1000);
  Serial.println("Setup ...");
  // saving Power (startup was unreliable without that)
  setCpuFrequencyMhz(160);

  esp_err_t err;
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err == !ESP_OK) {
    Serial.println("ERROR: Could not prevent persistant storage of Wifi Credentials in NVS.");
  }

  // Config for all bug wifi-ssid and wifi-password is saved in filesystem
  // If no config file is there, wifimanager is started to create one and restart the ESP.
  // So: Wifi will not connect if the config file is missing
  
  //clean FS, for testing
  //SPIFFS.format();

  Serial.println("mounting FS...");
  
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          ssid = json["wifi_ssid"].as<char*>();
          Serial.print("wifi SSID = ");
          Serial.println(ssid);
          pass = json["wifi_pass"].as<char*>();
          Serial.print("Wifi password = ");
          Serial.println(pass);
          strcpy(mqtt_server, json["mqtt_server"]);
          Serial.print("mqtt_server = ");
          Serial.println(mqtt_server);
          mqtt_port = strtol(json["mqtt_port"],NULL,10);
          Serial.print("mqtt_port = ");
          Serial.println(mqtt_port,10);
          strcpy(mqtt_user, json["mqtt_user"]);
          Serial.print("mqtt_user = ");
          Serial.println(mqtt_user);
          strcpy(mqtt_password, json["mqtt_password"]);
          Serial.print("mqtt_password = ");
          Serial.println(mqtt_password);
          inverterID = strtol(json["custom_inverterID"],NULL,10);
          // Create mqtt base-topic as "OpenNETek/<InverterID>"  
          char buffer[9];
          sprintf(buffer, "%x", inverterID);
          strncpy(mqtt_topic, "OpenNETek/", sizeof("OpenNETek/"));
          strncat(mqtt_topic, buffer, sizeof(buffer));
          Serial.print("inverterID = ");
          Serial.println(inverterID,16);

        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      // there is no configfile in the filesystem
      // lets get wifimanager to get us some config

      // start WifiManager Config Portal without timeout
      do_wifi_config(0);
    }

  } else {
    Serial.println("failed to mount FS");
    Serial.println("Trying to format...");
    if (SPIFFS.format()) {
      Serial.println("Formatting successful! Restarting...");
      ESP.restart();
    } else {
      Serial.println("Formatting not successful!");
    }
  }
  // connect to saved Wifi
  // WiFi.disconnect(true);

  
  // Connect to Wifi with ssid with pass
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting");

  // If still not connected after 30 seconds, offer a WiFi reconfig through WifiManager with a 10 Minute timeout
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++) {
    Serial.print('.');
    
    if (i==30){
      // waited already 30 seconds, maybe WiFI-config is just not working. Offering a change through WiFiManager, rebooting afterwards
      WiFi.removeEvent(WiFiEvent);
      WiFi.disconnect(false);
      do_wifi_config(600);
    }
    delay(1000);
  }
  // we get here only when WiFi is connected. yay.

  // Start WiFi Monitoring
  WiFi.onEvent(WiFiEvent);
  // only STA mode needed
  WiFi.mode(WIFI_MODE_STA);

  strcpy(hostname,mqtt_topic);
  
  // convert OpenNETek/<inverterID> to OpenNETek-<inverterID>
  hostname[9]='-';

  if (!MDNS.begin(hostname)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");



  // Serial.println("Welcome to Micro Inverter Interface by ATCnetz.de and enwi.one");

  // WebSerial is accessible at "<IP Address>/webserial" in browser

  // disabled for release, maybe reenable for troubleshooting?
  //WebSerial.begin(&server);
  //WebSerial.msgCallback(recvMsg);

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
          Serial.print("SSID set to: ");
          Serial.println(ssid);
        }
        // HTTP POST Wifi password value
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(pass);
        }
        // HTTP POST MQTT server value
        if (p->name() == PARAM_INPUT_3) {
          strcpy(mqtt_server, p->value().c_str());
          Serial.print("MQTT server set to: ");
          Serial.println(mqtt_server);
        }
        // HTTP POST MQTT port value
        if (p->name() == PARAM_INPUT_4) {
          mqtt_port = strtol(p->value().c_str(),NULL,10);
          Serial.print("MQTT port set to: ");
          Serial.println(mqtt_port,10);
        }
        // HTTP POST MQTT username value
        if (p->name() == PARAM_INPUT_5) {
          strcpy(mqtt_user, p->value().c_str());
          Serial.print("MQTT username set to: ");
          Serial.println(mqtt_user);
        }
        // HTTP POST MQTT password value
        if (p->name() == PARAM_INPUT_6) {
          strcpy(mqtt_password, p->value().c_str());
          Serial.print("MQTT password set to: ");
          Serial.println(mqtt_password);
        }
        // HTTP POST inverter-ID value
        if (p->name() == PARAM_INPUT_7) {
          inverterID = strtol(p->value().c_str(),NULL,16);
          Serial.print("Inverter ID set to: ");
          Serial.println(inverterID,16);
        }
        // HTTP POST via Factory Reset button
        if (p->name() == PARAM_INPUT_8) {
          Serial.println("Factory Reset initiated...");
          SPIFFS.remove("/config.json");
          SPIFFS.end();
          request->send(200, "text/plain", "Resetting to factory defaults and rebooting ESP. Connect to SSID OpenNETek to start initial setup.");
          delay(1000);
          ESP.restart();
        }
        //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    // save the custom parameters to FS
    if (SPIFFS.begin()) {

      Serial.println("saving config");
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
        Serial.println("failed to open config file for writing");
        request->send(200, "text/plain", "Failed to open config file for writing. Cannot save new configuration!");
      } else {
        // config file is opened for writing
        json.printTo(configFile);
        configFile.close();
        json.printTo(Serial);
        SPIFFS.end();
          
        //end save
        request->send(200, "text/plain", "Done. ESP will restart with the new settings");
        delay(3000);
        ESP.restart();
      }
    }

  });




  // Start MDNS-SD Dienst
  MDNS.addService("http", "tcp", 80);

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // OTA via /update
  AsyncElegantOTA.begin(&server);

  server.begin();

  if ( sizeof(mqtt_server) ) {
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(callback);
   (mqtt.connect("PV", mqtt_user, mqtt_password));

  }
  


    // Make sure the RF module is set to the correct settings
//    if (!pvclient.setDefaultRFSettings())
//    {
//        Serial.println("Could not set RF module to default settings");
//    }
}

uint32_t lastSendMillis = 0;



void loop()
{
const uint32_t currentMillis = millis();
if (currentMillis - lastSendMillis > 4000)
  {
  lastSendMillis = currentMillis;
    
  //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  Serial.print("Sending request now to InverterID ");
  Serial.println(inverterID,16);
  //WebSerial.print("Sending request now to InverterID (shown as as decimal) ");
  //WebSerial.println(inverterID);

  const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);
  
  if (status.valid)
    {
    //WebSerial.println("*********************************************");
    //WebSerial.println("Received Inverter Status");
    //WebSerial.print("Device (shown as decimal): ");
    //WebSerial.println(inverterID);
    //WebSerial.println("Status: " + String(status.state));
    //WebSerial.println("DC_Voltage: " + String(status.dcVoltage) + "V");
    //WebSerial.println("DC_Current: " + String(status.dcCurrent) + "A");
    //WebSerial.println("DC_Power: " + String(status.dcPower) + "W");
    //WebSerial.println("AC_Voltage: " + String(status.acVoltage) + "V");
    //WebSerial.println("AC_Current: " + String(status.acCurrent) + "A");
    //WebSerial.println("AC_Power: " + String(status.acPower) + "W");
    //WebSerial.println("Power gen total: " + String(status.totalGeneratedPower));
    //WebSerial.println("Temperature: " + String(status.temperature));
    // print Status to Serial for Debug
    Serial.println("*********************************************");
    Serial.println("Received Inverter Status");
    Serial.print("Device: ");
    Serial.println(status.deviceID, HEX);
    Serial.println("Status: " + String(status.state));
    Serial.println("DC_Voltage: " + String(status.dcVoltage) + "V");
    Serial.println("DC_Current: " + String(status.dcCurrent) + "A");
    Serial.println("DC_Power: " + String(status.dcPower) + "W");
    Serial.println("AC_Voltage: " + String(status.acVoltage) + "V");
    Serial.println("AC_Current: " + String(status.acCurrent) + "A");
    Serial.println("AC_Power: " + String(status.acPower) + "W");
    Serial.println("Power gen total: " + String(status.totalGeneratedPower));
    Serial.println("Temperature: " + String(status.temperature));
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
    if ( sizeof(mqtt_server) && wifi_connected ) 
      {
      if (!mqtt.connected()) 
        {
        Serial.println("MQTT not connected. Reconecting...");
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
        //Serial.print("Mqtt-dcVoltage: ");
        //Serial.println(dcVoltage);
        mqtt.publish(get_mqtt_topic("/dcVoltage"), dcVoltage);

        char dcCurrent[8];
        dtostrf(dcCurrent1, 1, 2, dcCurrent);
        //Serial.print("Mqtt-dcCurrent: ");
        //Serial.println(dcCurrent);
        mqtt.publish(get_mqtt_topic("/dcCurrent"), dcCurrent);

        char dcPower[8];
        dtostrf(dcPower1, 1, 2, dcPower);
        //Serial.print("Mqtt-dcPower: ");
        //Serial.println(dcPower);
        mqtt.publish(get_mqtt_topic("/dcPower"), dcPower);

        char acVoltage[8];
        dtostrf(acVoltage1, 1, 2, acVoltage);
        //Serial.print("Mqtt-acVoltage: ");
        //Serial.println(acVoltage);
        mqtt.publish(get_mqtt_topic("/acVoltage"), acVoltage);

        char acCurrent[8];
        dtostrf(acCurrent1, 1, 2, acCurrent);
        //Serial.print("Mqtt-acCurrent: ");
        //Serial.println(acCurrent);
        mqtt.publish(get_mqtt_topic("/acCurrent"), acCurrent);

        char acPower[8];
        dtostrf(acPower1, 1, 2, acPower);
        //Serial.print("Mqtt-acPower: ");
        //Serial.println(acPower);
        mqtt.publish(get_mqtt_topic("/acPower"), acPower);

        char tempString[8];
        dtostrf(temperature, 1, 2, tempString);
        //Serial.print("Mqtt-Temperature: ");
        //Serial.println(tempString);
        mqtt.publish(get_mqtt_topic("/temperature"), tempString);

        char totalGeneratedPower[8];
        dtostrf(totalGeneratedPower1, 1, 2, totalGeneratedPower);
        //Serial.print("Mqtt-totalGeneratedPower: ");
        //Serial.println(totalGeneratedPower);
        mqtt.publish(get_mqtt_topic("/totalGeneratedPower"), totalGeneratedPower);

        char state[8];
        dtostrf(state1, 1, 2, state);
        //Serial.print("Mqtt-state: ");
        //Serial.println(state);
        mqtt.publish(get_mqtt_topic("/state"), state);
        
        }
      }

    }
  else
    {
    Serial.print("Received no Inverter Status from ");
    Serial.println(inverterID,16);

    //WebSerial.print("Received no Inverter Status from ");
    //WebSerial.println(inverterID);
    
    }
  }
}