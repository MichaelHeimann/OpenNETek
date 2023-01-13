#include <FS.h>
#include <string.h>
#include <SPIFFS.h>
#include "NETSGPClient.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#define LED 2

//Optionen Start

// Add your MQTT Broker Data:
char mqtt_server[40];  //mqtt Broker ip
uint16_t mqtt_port;  //mqtt Broker port
char mqtt_user[32];         //mqtt Username
char mqtt_password[32];     //mqtt Password
//uint32_t inverterID = 0x38004044; /// Identifier of your inverter (see label on inverter)
uint32_t inverterID = 0x18CBA80; /// Identifier of your inverter (see label on inverter)

//Optionen End

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

    

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
    if (mqtt.connect("ESP8266Client")) {
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
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
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



String processor(const String& var){
  {
  const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);
  if (status.valid){
    //Serial.println(var);
    if(var == "DC_Voltage"){
      return String(status.dcVoltage);
      }
    if(var == "DC_Current"){
      return String(status.dcCurrent);
      }
    if(var == "DC_Power"){
      return String(status.dcPower);
      }
    if(var == "AC_Voltage"){
      return String(status.acVoltage);
      }
    if(var == "AC_Current"){
      return String(status.acCurrent);
      }
    if(var == "AC_Power"){
      return String(status.acPower);
      }
    if(var == "Temperature"){
      return String(status.temperature);
      }
    if(var == "Power_gen_total"){
      return String(status.totalGeneratedPower);
      }
    if(var == "Status"){
      return String(status.state);
      }
    if(var == "Status"){
      return String(0);
      }
  }
  return String("");
  }
}


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>PV Smart Inverter</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="20">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>PV Smart Inverter</h1>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e437;"></i> DC_Voltage</p><p><span class="reading"><span id="DC_Voltage">%DC_Voltage%</span> V</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e437;"></i> DC_Current</p><p><span class="reading"><span id="hum">%DC_Current%</span> A</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e437;"></i> DC_Power</p><p><span class="reading"><span id="pres">%DC_Power%</span> W</span></p>
      </div>
            <div class="card">
        <p><i class="fas fa-angle-double-up" style="color:#059e8a;"></i> AC_Voltage</p><p><span class="reading"><span id="temp">%AC_Voltage%</span> V</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-up" style="color:#059e8a;"></i> AC_Current</p><p><span class="reading"><span id="hum">%AC_Current%</span> A</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-up" style="color:#059e8a;"></i> AC_Power</p><p><span class="reading"><span id="pres">%AC_Power%</span> W</span></p>
      </div>
             <div class="card">
        <p><i class="<!--fas fa-thermometer-half-->" style="color:#059e8a;"></i> Temperature</p><p><span class="reading"><span id="temp">%Temperature%</span> C</span></p>
      </div>
      <div class="card">
        <p><i class="" style="color:#00add6;"></i> Power gen total</p><p><span class="reading"><span id="hum">%Power_gen_total%</span> KwH</span></p>
      </div>
      <div class="card">
        <p><i class="" style="color:#e1e437;"></i> Status</p><p><span class="reading"><span id="pres">%Status%</span> </span></p>
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
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('pressure', function(e) {
  console.log("pressure", e.data);
  document.getElementById("pres").innerHTML = e.data;
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
    
    //Config for MQTT server is saved in filesystem
    //clean FS, for testing
    //SPIFFS.format();

    //read configuration from FS json
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
            Serial.print("inverterID = ");
            Serial.println(inverterID,16);

          } else {
            Serial.println("failed to load json config");
          }
        }
      } else {
        // lets get wifimanager to get us some config

        // WifiManager stuff
        WiFiManager wifiManager;
        // uncomment to delete WiFi settings after each Reset.
        wifiManager.resetSettings();
        wifiManager.setDebugOutput(true);
        WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
        WiFiManagerParameter custom_mqtt_port("port", "mqtt port", "1883", 6);
        WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
        WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 32);
        WiFiManagerParameter custom_inverterID("InverterID", "InverterID","" , 8); // as printed on the label of the inverter which is in Hex
        wifiManager.addParameter(&custom_mqtt_server);
        wifiManager.addParameter(&custom_mqtt_port);
        wifiManager.addParameter(&custom_mqtt_user);
        wifiManager.addParameter(&custom_mqtt_password);
        wifiManager.addParameter(&custom_inverterID);
        
        wifiManager.autoConnect("OpenDTU", "opendtu!");
        strcpy (mqtt_server, custom_mqtt_server.getValue() );
        mqtt_port = strtol(custom_mqtt_port.getValue(),NULL,10);
        strcpy (mqtt_user, custom_mqtt_user.getValue() );
        strcpy (mqtt_password, custom_mqtt_password.getValue() );
        inverterID = strtol(custom_inverterID.getValue(),NULL,16);
        Serial.print("saving inverterID as ");
        Serial.print(inverterID,16);
        wifiManager.server.release();

        // save the custom parameters to FS
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
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
        //end save

        // restart to free up port 80 and avoid AsyncTCP.cpp:1268] begin(): bind error: -8
        // In Debug Scenario with config reset at the beginning this needs to be uncommented.
        ESP.restart();
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
    WiFi.begin();
    delay(2000);

    
    // Serial.println("Welcome to Micro Inverter Interface by ATCnetz.de and enwi.one");

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    WebSerial.msgCallback(recvMsg);

    // Handle Web Server - crashes when there are no values - TBD
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
    //request->send(200, "text/html", index_html);
  });

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
if (currentMillis - lastSendMillis > 2000)
  {
  lastSendMillis = currentMillis;
    
  //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  //WebSerial.println("");
  Serial.print("Sending request now to InverterID ");
  Serial.println(inverterID,16);
  WebSerial.print("Sending request now to InverterID (shown as as decimal) ");
  WebSerial.println(inverterID);

  // was 2000
  delay(2000);

  const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);
  
  if (status.valid)
    {
    WebSerial.println("*********************************************");
    WebSerial.println("Received Inverter Status");
    WebSerial.print("Device (shown as as decimal): ");
    WebSerial.println(inverterID);
    WebSerial.println("Status: " + String(status.state));
    WebSerial.println("DC_Voltage: " + String(status.dcVoltage) + "V");
    WebSerial.println("DC_Current: " + String(status.dcCurrent) + "A");
    WebSerial.println("DC_Power: " + String(status.dcPower) + "W");
    WebSerial.println("AC_Voltage: " + String(status.acVoltage) + "V");
    WebSerial.println("AC_Current: " + String(status.acCurrent) + "A");
    WebSerial.println("AC_Power: " + String(status.acPower) + "W");
    WebSerial.println("Power gen total: " + String(status.totalGeneratedPower));
    WebSerial.println("Temperature: " + String(status.temperature));
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
    delay(2000);
    }
  else
    {
    Serial.print("Received no Inverter Status from ");
    Serial.println(inverterID,16);

    WebSerial.print("Received no Inverter Status from ");
    
    WebSerial.println(inverterID);
    }
  }
  
  {
  const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);

  if (status.valid)
    {
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
    delay(5000);
    
    

    if ( sizeof(mqtt_server) ) 
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
          {
          const NETSGPClient::InverterStatus status = pvclient.getStatus(inverterID);
        
          // Uncomment the next line to set temperature in Fahrenheit 
          // (and comment the previous temperature line)
          //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit
          
          // Convert the value to a char array
          dcVoltage1 = (status.dcVoltage);
          char dcVoltage[8];
          dtostrf(dcVoltage1, 1, 2, dcVoltage);
          //Serial.print("Mqtt-dcVoltage: ");
          //Serial.println(dcVoltage);
          mqtt.publish("pv1/dcVoltage", dcVoltage);

          dcCurrent1 = (status.dcCurrent);
          char dcCurrent[8];
          dtostrf(dcCurrent1, 1, 2, dcCurrent);
          //Serial.print("Mqtt-dcCurrent: ");
          //Serial.println(dcCurrent);
          mqtt.publish("pv1/dcCurrent", dcCurrent);

          dcPower1 = (status.dcPower);
          char dcPower[8];
          dtostrf(dcPower1, 1, 2, dcPower);
          //Serial.print("Mqtt-dcPower: ");
          //Serial.println(dcPower);
          mqtt.publish("pv1/dcPower", dcPower);

          acVoltage1 = (status.acVoltage);
          char acVoltage[8];
          dtostrf(acVoltage1, 1, 2, acVoltage);
          //Serial.print("Mqtt-acVoltage: ");
          //Serial.println(acVoltage);
          mqtt.publish("pv1/acVoltage", acVoltage);

          acCurrent1 = (status.acCurrent);
          char acCurrent[8];
          dtostrf(acCurrent1, 1, 2, acCurrent);
          //Serial.print("Mqtt-acCurrent: ");
          //Serial.println(acCurrent);
          mqtt.publish("pv1/acCurrent", acCurrent);

          acPower1 = (status.acPower);
          char acPower[8];
          dtostrf(acPower1, 1, 2, acPower);
          //Serial.print("Mqtt-acPower: ");
          //Serial.println(acPower);
          mqtt.publish("pv1/acPower", acPower);

          temperature = (status.temperature);
          char tempString[8];
          dtostrf(temperature, 1, 2, tempString);
          //Serial.print("Mqtt-Temperature: ");
          //Serial.println(tempString);
          mqtt.publish("pv1/temperature", tempString);

          totalGeneratedPower1 = (status.totalGeneratedPower);
          char totalGeneratedPower[8];
          dtostrf(totalGeneratedPower1, 1, 2, totalGeneratedPower);
          //Serial.print("Mqtt-totalGeneratedPower: ");
          //Serial.println(totalGeneratedPower);
          mqtt.publish("pv1/totalGeneratedPower", totalGeneratedPower);

          state1 = (status.state);
          char state[8];
          dtostrf(state1, 1, 2, state);
          //Serial.print("Mqtt-state: ");
          //Serial.println(state);
          mqtt.publish("pv1/state", state);
          
          int len = String(inverterID).length();
          char cStr[len+1];
          itoa (inverterID, cStr, 10);
          mqtt.publish("pv1/inverterID", cStr);
          }   

        }
      }
    }
  }
}