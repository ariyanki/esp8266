// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTPIN 0
#define DHTTYPE    DHT21
DHT dht(DHTPIN, DHTTYPE);

float t = 0.0;
float h = 0.0;

// #### Network Configuration ####
// Access Point network credentials
const char* ap_ssid     = "humidy1";
const char* ap_password = "humidy12345";
bool wifiConnected = false;

// Set web server port number to 80
ESP8266WebServer server(80);

// #### Time ####
unsigned long timeNow = 0;
unsigned long timeLast = 0;
int startingHour = 0;
int seconds = 0;
int minutes = 0;
int hours = startingHour;
String currentDate = "";
String currentDay = "";


// #### Timer Configuration ####
#define TIMER_LIMIT 24 // for 24 times setting, each time 9 char *24
String timer[TIMER_LIMIT];

// #### EEPROM to store Data ####
// character length setting
int singleLength = 1; 
int ssidLength = 32; 
int pwdLength = 32;
int ipLength=15;
int timeLength=9*TIMER_LIMIT;

// Address Position setting
int ssidAddr = 0;
int pwdAddr = ssidAddr+ssidLength;
int ipAddr = pwdAddr+pwdLength;
int ipSubnetAddr = ipAddr+ipLength;
int ipGatewayAddr = ipSubnetAddr+ipLength;
int ipDNSAddr = ipGatewayAddr+ipLength;

int eepromSize=ipDNSAddr+ipLength;

void eeprom_write(String buffer, int addr, int length) {
  String curVal = eeprom_read(addr, length);
  // Check before write to minimize eeprom write operation
  if(curVal!=buffer){
    int bufferLength = buffer.length();
    EEPROM.begin(eepromSize);
    delay(10);
    for (int L = addr; L < addr+bufferLength; ++L) {
      EEPROM.write(L, buffer[L-addr]);
    }
    //set empty 
    for (int L = addr+bufferLength; L < addr+length; ++L) {
      EEPROM.write(L, 255);
    }
    EEPROM.commit();
  }
}

String eeprom_read(int addr, int length) {
  EEPROM.begin(eepromSize); 
  String buffer="";
  delay(10);
  for (int L = addr; L < addr+length; ++L){
    if (isAscii(EEPROM.read(L)))
      buffer += char(EEPROM.read(L));
  }
  return buffer;
}

void eeprom_write_single(int value, int addr) {
  int curVal = eeprom_read_single(addr);
  // Check before write to minimize eeprom write operation
  if(curVal!=value){
    EEPROM.begin(eepromSize);
    EEPROM.write(addr, value);
    EEPROM.commit();
  }
}

int eeprom_read_single(int addr) {
  EEPROM.begin(eepromSize); 
  return EEPROM.read(addr);
}

// #### HTTP Configuration ####
String headerHtml = "<!DOCTYPE html><html>"
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">"
  "<style>"
  "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
  "body {margin:0px}"
  ".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
  ".button2 {background-color: #77878A;border-radius: 15px;}"
  ".header {background-color: black; color: white;padding: 20px;margin: 0px;}"
  "</style>"
  "</head>";
String footerHtml = "<html>";

String redirectToRootHtml  = "<!DOCTYPE html><html>"
  "<head><script>window.location.href = \"/\";</script></head>"            
  "<body></body></html>";

String savedNotifHtml  = headerHtml + "<body><h1 class=\"header\">Settings</h1><br/><br/>"
    "<p>Your configuration has been saved, if you are sure with your configuration then please restart your device</p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"restart()\"><i class=\"fas fa-redo\"></i> Restart</button></a></p>"
    "<p><a href=\"/\"><button class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Back to home</button></a></p>"
    "<script>"
    "function restart(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/restart\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;

void handleRoot() {
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Temperature & Humidity</h1>"
    "<p style=\"margin-top: 40px;\"><a href=\"/settings\"><button class=\"button button2\"><i class=\"fas fa-cogs\"></i> Settings</button></a></p>"
    "<p></p>"
    "<h1><i class=\"fas fa-thermometer-half\" style=\"color:#059e8a;\"></i> <span>"+String(t)+"</span> <sup>&deg;C</sup></h1>"
    "<h1><i class=\"fas fa-tint\" style=\"color:#00add6;\"></i> <span>"+String(h)+"</span> <sup>%</sup></h1>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleSettings() {
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Settings</h1>"
    "<p><a href=\"/wificonfig\"><button class=\"button button2\"><i class=\"fas fa-wifi\"></i> Wifi Config</button></a></p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"restart()\"><i class=\"fas fa-redo\"></i> Restart</button></a></p>"
    "<p><a href=\"/\"><button class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Back to home</button></a></p>"
    "<p></p>"
    "<script>"
    "function restart(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/restart\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleWifiConfigForm() {
  String ssid = eeprom_read(ssidAddr, ssidLength);
  String password = eeprom_read(pwdAddr, pwdLength);
  String strIp = eeprom_read(ipAddr, ipLength);
  String strSubnet = eeprom_read(ipSubnetAddr, ipLength);
  String strGateway = eeprom_read(ipGatewayAddr, ipLength);
  String strDNS = eeprom_read(ipDNSAddr, ipLength);
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Wifi Config</h1>"
    "<form method=post action=\"/savewificonfig\">"
    "<p><b>SSID</b><br/><input type=text name=ssid id=ssid value=\""+ssid+"\"><br/>(max 32 character)</p>"
    "<p><b>Password</b><br/><input type=text name=password id=password value=\""+password+"\"><br/>(max 32 character)</p>"
    "<p>Manual Setting IP<br/>(leave empty if you want to use DHCP)</p>"
    "<p><b>IP Address</b><br/><input type=text name=ip id=ip value=\""+strIp+"\"></p>"
    "<p><b>Subnet</b><br/><input type=text name=subnet id=subnet value=\""+strSubnet+"\"></p>"
    "<p><b>Gateway</b><br/><input type=text name=gateway id=gateway value=\""+strGateway+"\"></p>"
    "<p><b>DNS</b><br/><input type=text name=dns id=dns value=\""+strDNS+"\"></p>"
    "<p><button type=submit value=Save class=\"button button2\"><i class=\"fas fa-save\"></i> Save</button> <button type=\"button\" onclick=\"window.location.href = '/';\" class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Cancel</button></p>"
    "</form>"
    "</body>"+footerHtml;
    
  server.send(200, "text/html", htmlRes);
}

void handleSaveWifiConfigForm() {
  eeprom_write(server.arg("ssid"), ssidAddr,ssidLength);
  eeprom_write(server.arg("password"), pwdAddr,pwdLength);
  eeprom_write(server.arg("ip"), ipAddr,ipLength);
  eeprom_write(server.arg("subnet"), ipSubnetAddr,ipLength);
  eeprom_write(server.arg("gateway"), ipGatewayAddr,ipLength);
  eeprom_write(server.arg("dns"), ipDNSAddr,ipLength);
  
  server.send(200, "text/html", savedNotifHtml);
}

void handleRestart() {
  ESP.restart();
}

void connectToWifi(){
  // GET Wifi Config from EEPROM
  String ssid = eeprom_read(ssidAddr, ssidLength);
  if(ssid.length()>0){
    String password = eeprom_read(pwdAddr, pwdLength);
      
    // Connect to Wi-Fi network with SSID and password
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      i++;
      if(i>20) break;
      delay(500);
    }

    if(i<=20) {
      //Set Static IP
      String strIp = eeprom_read(ipAddr, ipLength);
      IPAddress local_IP;
      
      if(local_IP.fromString(strIp)){
        String strSubnet = eeprom_read(ipSubnetAddr, ipLength);
        String strGateway = eeprom_read(ipGatewayAddr, ipLength);
        String strDNS = eeprom_read(ipDNSAddr, ipLength);

        IPAddress gateway;
        IPAddress subnet;
        IPAddress dns;
  
        if(gateway.fromString(strSubnet)){
          Serial.println("Gateway Parsed");
        }
        if(subnet.fromString(strGateway)){
          Serial.println("Subnet Parsed");
        }
        if(dns.fromString(strDNS)){
          Serial.println("DNS Parsed");
        }
      
        if (!WiFi.config(local_IP, dns, gateway, subnet)) {
          Serial.println("STA Failed to configure");
        }
      }
      
      wifiConnected = true;
      dht.begin();
    }
  }
}

void updateTime(){
  timeNow = millis();
  if (timeNow >= timeLast+1000){
    timeLast=timeNow;
    seconds = seconds + 1;
    
    if (seconds == 60) {
      seconds = 0;
      minutes = minutes + 1;
    }
  
    if (minutes == 60){ 
      minutes = 0;
      hours = hours + 1;
    }
  
    if (hours == 24){
      hours = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize Access Point
  WiFi.softAP(ap_ssid, ap_password);

  // Connect to wifi
  connectToWifi();

  // Start web server
  server.on("/", handleRoot);
  server.on("/wificonfig", handleWifiConfigForm);
  server.on("/settings", handleSettings);
  server.on("/savewificonfig", HTTP_POST, handleSaveWifiConfigForm);
  server.on("/restart", HTTP_GET, handleRestart);
  server.begin();

  // OTA Setup
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

long lastMilis = 0; 

void loop(){
  server.handleClient();
  updateTime();
  ArduinoOTA.handle();
  
  // execute in second
  if (millis() >= lastMilis+1000){
    lastMilis=millis();

    //wifi connnect retry
    if (seconds == 30 and !wifiConnected){
      connectToWifi();
    }

    // Read Temp
    float newT = dht.readTemperature();
    if (!isnan(newT)) {
      t = newT;
    }
    
    // Read Humidity
    float newH = dht.readHumidity();
    if (!isnan(newH)) {
      h = newH;
    }
  }
  
  
}
