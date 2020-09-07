// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>


#define RELAY_NO    false
#define NUM_RELAYS 4 // this number impact to eeprom size max 4096, for 24 times setting, each time 9 char *24 * num relays 
#define TIMER_LIMIT 24 // for 24 times setting, each time 9 char *24

// #### Network Configuration ####
// Access Point network credentials
const char* ap_ssid     = "esp8266plug";
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


// #### Feeding Timer Configuration ####
String timerOn[NUM_RELAYS][TIMER_LIMIT];
String timerOff[NUM_RELAYS][TIMER_LIMIT];

// #### EEPROM to store Data ####
// character length setting
int singleLength = 1; 
int ssidLength = 32; 
int pwdLength = 32;
int ipLength=15;
int gpioLength=NUM_RELAYS;
int timeLength=9*TIMER_LIMIT;

// Address Position setting
int ssidAddr = 0;
int pwdAddr = ssidAddr+ssidLength;
int ipAddr = pwdAddr+pwdLength;
int ipSubnetAddr = ipAddr+ipLength;
int ipGatewayAddr = ipSubnetAddr+ipLength;
int ipDNSAddr = ipGatewayAddr+ipLength;
int gpioAddr = ipDNSAddr+ipLength;
int gpioStateAddr = gpioAddr+gpioLength;
int timeOnAddr = gpioAddr+gpioLength;
int timeOffAddr = timeOnAddr+(NUM_RELAYS*timeLength);

int eepromSize=timeOffAddr+(NUM_RELAYS*timeLength);


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

String logStr = "";

String headerHtml = "<!DOCTYPE html><html>"
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<link rel=\"icon\" href=\"data:,\">"
  "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
  ".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;"
  "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
  ".button2 {background-color: #77878A;}"
  ".switch {position: relative; display: inline-block; width: 120px; height: 68px} "
  ".switch input {display: none}"
  ".slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}"
  ".slider:before {position: absolute; content: \"\"; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}"
  "input:checked+.slider {background-color: #2196F3}"
  "input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}"
  "</style></head>";
String footerHtml = "<html>";

String redirectToRootHtml  = "<!DOCTYPE html><html>"
  "<head><script>window.location.href = \"/\";</script></head>"            
  "<body></body></html>";

String savedNotifHtml  = headerHtml + "<body><br/><br/>"
    "<p>Your configuration has been saved, if you are sure with your configuration then please restart your device</p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"restart()\">Restart</button></a></p>"
    "<p><a href=\"/\"><button class=\"button button2\">Back to home</button></a></p>"
    "<script>"
    "function restart(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/restart\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;

String relayState(int relayno){
  if(RELAY_NO){
    if(digitalRead(eeprom_read_single(gpioAddr+relayno))){
      return "checked";
    }
    else {
      return "";
    }
  }
  else {
    if(digitalRead(eeprom_read_single(gpioAddr+relayno))){
      return "";
    }
    else {
      return "checked";
    }
  }
  return "";
}

void handleRoot() {
  String buttons ="";
  for(int i=0; i<NUM_RELAYS; i++){
    buttons+= "<h4>Plug #" + String(i+1) + " - GPIO " + eeprom_read_single(gpioAddr+i) + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" "+ relayState(i) +"><span class=\"slider\"></span></label>";
  }
  
  syncTime();
  
  String htmlRes  = headerHtml + "<body><h1>Smart Plug</h1>"
    "<p>"+currentDay+", "+currentDate+" "+hours+":"+minutes+":"+seconds+"</p>"
    "<p>To make this timer work, please make sure wifi configuration connected to internet because it is connected to NTP Server.</p>"
    "<p>If the datetime above correct then your wifi configuration is correct.</p>"
    "<p>"+logStr+"</p>"
    "<p><a href=\"/wificonfig\"><button class=\"button button2\">Wifi Config</button></a></p>"
    "<p><a href=\"/gpioconfig\"><button class=\"button button2\">Plug GPIO Config</button></a></p>"
    "<p><a href=\"/timerconfig\"><button class=\"button button2\">Timer Config</button></a></p>"
    "<p><a href=\"#\"><button class=\"button button2\" onclick=\"restart()\">Restart</button></a></p>"
    "<p>"+buttons+"</p>"
    "<script>"
    "function toggleCheckbox(element) {"
    "var xhr = new XMLHttpRequest();"
    "if(element.checked){ xhr.open(\"GET\", \"/updaterelay?relay=\"+element.id+\"&state=1\", true); }"
    "else { xhr.open(\"GET\", \"/updaterelay?relay=\"+element.id+\"&state=0\", true); }"
    "xhr.send();"
    "}"
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
  
  String htmlRes  = headerHtml + "<body><h1>Wifi Config</h1>"
    "<form method=post action=\"/savewificonfig\">"
    "<p><b>SSID</b><br/><input type=text name=ssid id=ssid value=\""+ssid+"\"><br/>(max 32 character)</p>"
    "<p><b>Password</b><br/><input type=text name=password id=password value=\""+password+"\"><br/>(max 32 character)</p>"
    "<p>Manual Setting IP<br/>(leave empty if you want to use DHCP)</p>"
    "<p><b>IP Address</b><br/><input type=text name=ip id=ip value=\""+strIp+"\"></p>"
    "<p><b>Subnet</b><br/><input type=text name=subnet id=subnet value=\""+strSubnet+"\"></p>"
    "<p><b>Gateway</b><br/><input type=text name=gateway id=gateway value=\""+strGateway+"\"></p>"
    "<p><b>DNS</b><br/><input type=text name=dns id=dns value=\""+strDNS+"\"></p>"
    "<p><input type=submit value=Save> <input type=button value=Cancel onclick=\"window.location.href = '/';\"></p>"
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

void handleGPIOConfigForm() {
  String inputForm="";
  for(int i=0; i<NUM_RELAYS; i++){
    String defaultStateCheck = "";
    if(eeprom_read_single(gpioStateAddr+i)==1){
      defaultStateCheck = "checked";
    }
    inputForm += "<p><b>Plug #"+String(i+1)+" GPIO Number</b></br><input type=text name=gpio"+i+" id=gpio"+i+" value=\""+eeprom_read_single(gpioAddr+i)+"\"></p>"
    "<p><b>Default State</b> <input type=checkbox name=defaultstate"+(i)+" id=defaultstate"+(i)+" "+defaultStateCheck+" value=\"1\"> On </p>";
  }
  
  String htmlRes  = headerHtml + "<body><h1>Plug GPIO Config</h1>"
    "<form method=post action=\"/savegpioconfig\">"+inputForm+""
    "<p><input type=submit value=Save> <input type=button value=Cancel onclick=\"window.location.href = '/';\"></p>"
    "</form>"
    "</body>"+footerHtml;
  
  server.send(200, "text/html", htmlRes);
}

void handleSaveGPIOConfigForm() {
  for(int i=0; i<NUM_RELAYS; i++){
    eeprom_write_single(server.arg("gpio"+String(i)).toInt(), gpioAddr+i);
    eeprom_write_single(server.arg("defaultstate"+String(i)).toInt(), gpioStateAddr+i);
  }
  
  server.send(200, "text/html", savedNotifHtml);
}


void handleTimerConfigForm() {
  String inputForm="";
  for(int i=0; i<NUM_RELAYS; i++){
    String strTimerOn = eeprom_read(timeOnAddr+(i*timeLength), timeLength);
    String strTimerOff = eeprom_read(timeOffAddr+(i*timeLength), timeLength);
    inputForm += "<p><b>Timer Plug #"+String(i+1)+" On</b></br><input type=text name=gpioon"+i+" id=gpioon"+i+" value=\""+strTimerOn+"\"></p>";
    inputForm += "<p><b>Timer Plug #"+String(i+1)+" Off</b></br><input type=text name=gpiooff"+i+" id=gpiooff"+i+" value=\""+strTimerOff+"\"></p>";
  }
  
  String htmlRes  = headerHtml + "<body><h1>Timer Config</h1>"
    "<form method=post action=\"/savetimerconfig\">"
    "<p>Format hour:minute:second without \"0\"<br/>"
    "For multiple time input with \";\" delimitier<br/>"
    "To save memory it has each limit "+String(TIMER_LIMIT)+" times setting maximum<br/>"
    "<b>Example:</b> 1:30:0;6:8:0;18:7:12</p>"+inputForm+""
    "<p><input type=submit value=Save> <input type=button value=Cancel onclick=\"window.location.href = '/';\"></p>"
    "</form>"
    "</body>"+footerHtml;
  
  server.send(200, "text/html", htmlRes);
}

void handleSaveTimerConfigForm() {
  for(int i=0; i<NUM_RELAYS; i++){
    eeprom_write(server.arg("gpioon"+String(i)), timeOnAddr+(i*timeLength),timeLength);
    eeprom_write(server.arg("gpiooff"+String(i)), timeOffAddr+(i*timeLength),timeLength);
  }
  readTimer();
  
  server.send(200, "text/html", redirectToRootHtml);
}

void handleUpdateRelay() {
  updateRelay(server.arg("relay").toInt(),server.arg("state").toInt());
  
  server.send(200, "text/plain", "OK");
}

void handleRestart() {
  ESP.restart();
}

void updateRelay(int relayno, int relaystate){
  if(RELAY_NO){
    digitalWrite(eeprom_read_single(gpioAddr+relayno), relaystate);
  }
  else{
    digitalWrite(eeprom_read_single(gpioAddr+relayno), !relaystate);
  }
}

void readTimer(){
  // GET timer for EEPROM
  for(int i=0; i<NUM_RELAYS; i++){
    String strTime = eeprom_read(timeOnAddr+(i*timeLength), timeLength);
    int f = 0, r=0;
    for (int j=0; j < strTime.length(); j++)
    { 
     if(strTime.charAt(j) == ';') 
      { 
        timerOn[i][f] = strTime.substring(r, j); 
        r=(j+1); 
        f++;
      }
    }
    strTime = eeprom_read(timeOffAddr+(i*timeLength), timeLength);
    f = 0, r=0;
    for (int j=0; j < strTime.length(); j++)
    { 
     if(strTime.charAt(j) == ';') 
      { 
        timerOff[i][f] = strTime.substring(r, j); 
        r=(j+1); 
        f++;
      }
    }
  }
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

  // Initialize Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("visit: \n"); 
  Serial.println(WiFi.softAPIP());

  //  in case want to try write manually
//    eeprom_write("ssid", ssidAddr,ssidLength);
//    eeprom_write("password", pwdAddr,pwdLength);
//    eeprom_write("192.168.1.113", ipAddr,ipLength);
//    eeprom_write("255.255.255.0", ipSubnetAddr,ipLength);
//    eeprom_write("192.168.1.1", ipGatewayAddr,ipLength);
//    eeprom_write("192.168.1.1", ipDNSAddr,ipLength);

//  in case something wrong,  depend on NUM_RELAYS
// eeprom_write_single(255, gpioAddr);
// eeprom_write_single(255, gpioAddr+1);
// eeprom_write_single(255, gpioAddr+2);
// eeprom_write_single(255, gpioAddr+3);
// eeprom_write_single(255, gpioAddr+4);
// eeprom_write_single(255, gpioAddr+5);

  connectToWifi();

  // Initialize PIN
  for(int i=0; i<NUM_RELAYS; i++){
    pinMode(eeprom_read_single(gpioAddr+i), OUTPUT);
    int defaultState = eeprom_read_single(gpioStateAddr+i);
    if(RELAY_NO){
      if (defaultState == 1){
        digitalWrite(eeprom_read_single(gpioAddr+i), HIGH);
      }else{
        digitalWrite(eeprom_read_single(gpioAddr+i), LOW);
      }
    }
    else{
      if (defaultState == 1){
        digitalWrite(eeprom_read_single(gpioAddr+i), LOW);
      }else{
        digitalWrite(eeprom_read_single(gpioAddr+i), HIGH);
      }
    }
  }

  readTimer();
  
  // start web server
  server.on("/", handleRoot);
  server.on("/wificonfig", handleWifiConfigForm);
  server.on("/savewificonfig", HTTP_POST, handleSaveWifiConfigForm);
  server.on("/gpioconfig", handleGPIOConfigForm);
  server.on("/savegpioconfig", HTTP_POST, handleSaveGPIOConfigForm);
  server.on("/timerconfig", handleTimerConfigForm);
  server.on("/savetimerconfig", HTTP_POST, handleSaveTimerConfigForm);
  server.on("/updaterelay", HTTP_GET, handleUpdateRelay);
  server.on("/restart", HTTP_GET, handleRestart);
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
  ArduinoOTA.handle();
  updateTime();

  // execute in second
  if (millis() >= lastMilis+1000){
    lastMilis=millis();
    
    if (seconds == 30 and !wifiConnected){
      connectToWifi();
    }
    // Sync to NTP
    if (seconds == 0 and minutes == 30){
        syncTime();
    }
    // Execute Timer
    String checkTime = String(hours)+":"+String(minutes)+":"+String(seconds);    
    for(int i=0; i<NUM_RELAYS; i++){
      for (int j=0;j<TIMER_LIMIT;j++){
        if (timerOn[i][j] == checkTime) {
          updateRelay(i,1);
        }
        if (timerOff[i][j] == checkTime) {
          updateRelay(i,0);
        }
      }
    }
  }
}
