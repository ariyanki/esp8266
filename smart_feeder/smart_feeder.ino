// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

Servo servo;
int servoFrom = 0;
int servoTo = 0;

// #### Network Configuration ####
// Access Point network credentials
const char* hostname     = "pakanikan";
const char* ap_ssid     = "pakanikan";
const char* ap_password = "esp826612345";
bool wifiConnected = false;

// Set web server port number to 80
ESP8266WebServer server(80);


// #### NTP Configuration ####
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", (7*3600));

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
int gpioAddr = ipDNSAddr+ipLength;
int servoWriteFromAddr = gpioAddr+singleLength;
int servoWriteToAddr = servoWriteFromAddr+singleLength;
int timeAddr = servoWriteToAddr+singleLength;

int eepromSize=timeAddr+timeLength;

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


void readTimer(){
  // GET timer for EEPROM
  String strTime = eeprom_read(timeAddr, timeLength);
  int f = 0, r=0;
  for (int i=0; i < strTime.length(); i++)
  { 
   if(strTime.charAt(i) == ';') 
    { 
      timer[f] = strTime.substring(r, i); 
      r=(i+1); 
      f++;
    }
  }
}

// #### HTTP Configuration ####

String logStr = "";

String headerHtml = "<!DOCTYPE html><html>"
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css\" integrity=\"sha384-HSMxcRTRxnN+Bdg0JdbxYKrThecOKuH5zCYotlSAcp1+c8xmyTe9GYg1l9a69psu\" crossorigin=\"anonymous\">"
  "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">"
  "<style>"
  "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
  "body {margin:0px}"
  ".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;"
  "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
  ".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
  ".button2 {background-color: #77878A;border-radius: 15px; display: inline-grid;text-decoration: none;}"
  ".header {background-color: black; color: white;padding: 20px;margin: 0px; margin-bottom: 20px;}"
  ".switch {position: relative; display: inline-block; width: 120px; height: 68px} "
  ".switch input {display: none}"
  ".slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}"
  ".slider:before {position: absolute; content: \"\"; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}"
  "input:checked+.slider {background-color: #2196F3}"
  "input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}"
  "</style></head>";
String footerHtml = "<script src=\"https://stackpath.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js\" integrity=\"sha384-aJ21OjlMXNL5UyIl/XNwTMqvzeRMZH2w8c5cRVpzpU8Y5bApTppSuUkhZXN0VxHd\" crossorigin=\"anonymous\"></script>"
   "<html>";

String redirectToRootHtml  = "<!DOCTYPE html><html>"
  "<head><script>window.location.href = \"/\";</script></head>"            
  "<body></body></html>";

String savedNotifHtml  = headerHtml + "<body><br/><br/>"
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
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Smart Feeder</h1>"
    "<h3 style=\"margin-bottom: 20px;\">"+currentDay+", "+currentDate+" "+hours+":"+minutes+":"+seconds+"</h3><hr>"
    "<p>"+logStr+"</p>"
    "<p><button class=\"button button2\" onclick=\"testFeed()\">Feeding Test</button></p>"
    "<p>To make this timer work, please make sure your wifi connected to the internet to get time from NTP Server.</p>"
    "<p>If the datetime above correct then your wifi configuration is correct.</p>"
    "<p style=\"margin-top: 40px;\"><a href=\"/settings\"><button class=\"button button2\"><i class=\"fas fa-cogs\"></i> Settings</button></a></p>"
    "<script>function testFeed() {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/feeding\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleSettings() {
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Settings</h1>"
    "<h3 style=\"margin-bottom: 20px;\">"+currentDay+", "+currentDate+" "+hours+":"+minutes+":"+seconds+"</h3><hr>"
    "<p><a href=\"/wificonfig\"><button class=\"button button2\"><i class=\"fas fa-wifi\"></i> Wifi Config</button></a></p>"
    "<p><a href=\"/servoconfig\"><button class=\"button button2\"><i class=\"fas fa-plug\"></i> Servo Config</button></a></p>"
    "<p><a href=\"/timerconfig\"><button class=\"button button2\"><i class=\"fas fa-clock\"></i> Timer Config</button></a></p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"synctime()\"><i class=\"fas fa-clock\"></i> Sync Time</button></a></p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"restart()\"><i class=\"fas fa-redo\"></i> Restart</button></a></p>"
    "<p><a href=\"/\"><button class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Back to home</button></a></p>"
    "<p></p>"
    "<script>"
    "function restart(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/restart\", true);"
    "xhr.send();"
    "function synctime(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/synctime\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleFeeding() {
  servoWrite();
  server.send(200, "text/plain", "OK");
}

void handleWifiConfigForm() {
  String ssid = eeprom_read(ssidAddr, ssidLength);
  String password = eeprom_read(pwdAddr, pwdLength);
  String strIp = eeprom_read(ipAddr, ipLength);
  String strSubnet = eeprom_read(ipSubnetAddr, ipLength);
  String strGateway = eeprom_read(ipGatewayAddr, ipLength);
  String strDNS = eeprom_read(ipDNSAddr, ipLength);
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Wifi Config</h1>"
    "<form method=post action=\"/savewificonfig\" style=\"margin: 20px\">"
    "<p><b>SSID</b><br/><input type=text class=\"form-control\" name=ssid id=ssid value=\""+ssid+"\"><br/>(max 32 character)</p>"
    "<p><b>Password</b><br/><input type=text class=\"form-control\" name=password id=password value=\""+password+"\"><br/>(max 32 character)</p>"
    "<p>Manual Setting IP<br/>(leave empty if you want to use DHCP)</p>"
    "<p><b>IP Address</b><br/><input type=text class=\"form-control\" name=ip id=ip value=\""+strIp+"\"></p>"
    "<p><b>Subnet</b><br/><input type=text class=\"form-control\" name=subnet id=subnet value=\""+strSubnet+"\"></p>"
    "<p><b>Gateway</b><br/><input type=text class=\"form-control\" name=gateway id=gateway value=\""+strGateway+"\"></p>"
    "<p><b>DNS</b><br/><input type=text class=\"form-control\" name=dns id=dns value=\""+strDNS+"\"></p>"
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

void handleServoConfigForm() {
  String strGPIO = String(eeprom_read_single(gpioAddr));
  String strWriteFrom = String(eeprom_read_single(servoWriteFromAddr));
  String strWriteTo = String(eeprom_read_single(servoWriteToAddr));
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Servo Config</h1>"
    "<form method=post action=\"/saveservoconfig\" style=\"margin: 20px\">"
    "<p><b>GPIO Number</b></br><input type=text class=\"form-control\" name=gpio id=gpio value=\""+strGPIO+"\"></p>"
    "<p><b>Write From</b></br><input type=text class=\"form-control\" name=write_from id=write_from value=\""+strWriteFrom+"\"></p>"
    "<p><b>Write To</b></br><input type=text class=\"form-control\" name=write_to id=write_to value=\""+strWriteTo+"\"></p>"
    "<p><button type=submit value=Save class=\"button button2\"><i class=\"fas fa-save\"></i> Save</button> <button type=\"button\" onclick=\"window.location.href = '/';\" class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Cancel</button></p>"
    "</form>"
    "</body>"+footerHtml;
  
  server.send(200, "text/html", htmlRes);
}

void handleSaveServoConfigForm() {
  eeprom_write_single(server.arg("gpio").toInt(), gpioAddr);
  eeprom_write_single(server.arg("write_from").toInt(), servoWriteFromAddr);
  eeprom_write_single (server.arg("write_to").toInt(), servoWriteToAddr);
  
  server.send(200, "text/html", savedNotifHtml);
}


void handleTimerConfigForm() {
  String strTimer = eeprom_read(timeAddr, timeLength);
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Timer Config</h1>"
    "<form method=post action=\"/savetimerconfig\" style=\"margin: 20px\">"
    "<p>Format hour:minute:second without \"0\"<br/>"
    "For multiple time input with \";\" delimitier<br/>"
    "To save memory it has limit "+String(TIMER_LIMIT)+" times setting maximum<br/>"
    "<b>Example:</b> 1:30:0;6:8:0;18:7:12</p>"
    "<p><b>Timer</b></br><input type=text class=\"form-control\" name=timer id=timer value=\""+strTimer+"\"></p>"
    "<p><button type=submit value=Save class=\"button button2\"><i class=\"fas fa-save\"></i> Save</button> <button type=\"button\" onclick=\"window.location.href = '/';\" class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Cancel</button></p>"
    "</form>"
    "</body>"+footerHtml;
  
  server.send(200, "text/html", htmlRes);
}

void handleSaveTimerConfigForm() {
  eeprom_write(server.arg("timer"), timeAddr,timeLength);
  readTimer();
  
  server.send(200, "text/html", redirectToRootHtml);
}

void handleRestart() {
  ESP.restart();
}

void handleSyncTime() {
  syncTime();
}

void servoWrite() {
  // Total delay must be more than 1000 milisecond when using timer
  servo.write(servoTo);
  delay(500);
  servo.write(servoFrom);
  delay(2000);
}

void connectToWifi(){
  // GET Wifi Config from EEPROM
  String ssid = eeprom_read(ssidAddr, ssidLength);
  Serial.println("EEPROM "+String(eepromSize)+" Config:");
  Serial.println(ssid);
  if(ssid.length()>0){
    String password = eeprom_read(pwdAddr, pwdLength);
    Serial.println(password);
      
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      i++;
      if(i>20) break;
      delay(500);
      Serial.print(".");
      
    }

    if(i>20) {
      Serial.println("WiFi not connected. Please use \""+String(ap_ssid)+"\" AP to config");
    }else{
      //Set Static IP
      String strIp = eeprom_read(ipAddr, ipLength);
      Serial.println(strIp);
      IPAddress local_IP;
      
      if(local_IP.fromString(strIp)){
        Serial.println("IP Parsed");
        String strSubnet = eeprom_read(ipSubnetAddr, ipLength);
        String strGateway = eeprom_read(ipGatewayAddr, ipLength);
        String strDNS = eeprom_read(ipDNSAddr, ipLength);

        Serial.println(strSubnet);
        Serial.println(strGateway);
        Serial.println(strDNS);
  
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
      Serial.println("WiFi connected.");
      
      //Set hostname
      if (!MDNS.begin(hostname)) {
          Serial.println("Error setting up MDNS responder!");
      }
      
      wifiConnected = true;
      timeClient.begin();
      syncTime();
    }
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void syncTime(){
  if (wifiConnected){
    timeClient.update();
    seconds = timeClient.getSeconds();
    minutes = timeClient.getMinutes();
    hours = timeClient.getHours();
    timeLast = millis();
    
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon+1;
    String currentMonthName = months[currentMonth-1];
    int currentYear = ptm->tm_year+1900;
    
    currentDay = String(daysOfTheWeek[timeClient.getDay()]);
    currentDate = String(monthDay) + " " + String(currentMonthName) + " " + String(currentYear);
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
  // Initialize servo pin
  servo.attach(eeprom_read_single(gpioAddr));
  servoFrom = eeprom_read_single(servoWriteFromAddr);
  servoTo = eeprom_read_single(servoWriteToAddr);
  servo.write(servoFrom);

  // Initialize Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("visit: \n"); 
  Serial.println(WiFi.softAPIP());

//      in case want to try write manually
//    eeprom_write("ssid", ssidAddr,ssidLength);
//    eeprom_write("password", pwdAddr,pwdLength);
//    eeprom_write("192.168.1.113", ipAddr,ipLength);
//    eeprom_write("255.255.255.0", ipSubnetAddr,ipLength);
//    eeprom_write("192.168.1.1", ipGatewayAddr,ipLength);
//    eeprom_write("192.168.1.1", ipDNSAddr,ipLength);

  connectToWifi();
  readTimer();

  // start web server
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/feeding", handleFeeding);
  server.on("/wificonfig", handleWifiConfigForm);
  server.on("/savewificonfig", HTTP_POST, handleSaveWifiConfigForm);
  server.on("/servoconfig", handleServoConfigForm);
  server.on("/saveservoconfig", HTTP_POST, handleSaveServoConfigForm);
  server.on("/timerconfig", handleTimerConfigForm);
  server.on("/savetimerconfig", HTTP_POST, handleSaveTimerConfigForm);
  server.on("/restart", HTTP_GET, handleRestart);
  server.on("/synctime", HTTP_GET, handleSyncTime);
  server.begin();

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
    
    if (seconds == 30 and !wifiConnected){
      connectToWifi();
    }
    
    // Execute Timer
    String checkTime = String(hours)+":"+String(minutes)+":"+String(seconds);   
    for (int i=0;i<TIMER_LIMIT;i++){
      if (timer[i] == checkTime) {
        servoWrite();
      }
    }
  }
  
  
}
