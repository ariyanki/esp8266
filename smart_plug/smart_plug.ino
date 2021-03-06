// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>


#define RELAY_NO    false
#define TIMER_LIMIT 24 // for 24 times setting, each time 9 char *24

// #### Network Configuration ####
// Access Point network credentials
#define NUM_RELAYS 4 // this number impact to eeprom size max 4096, for 24 times setting, each time 9 char *24 * num relays 
String html_title = "Saklar 12 v";
const char* ap_hostname     = "saklar12v";
const char* ap_ssid     = "saklar12v";
const char* ap_password = "esp826612345";

HTTPClient httpClient;

WiFiClient espClient;
PubSubClient client(espClient);

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
int portLength=4;

// Address Position setting
int ssidAddr = 0;
int pwdAddr = ssidAddr+ssidLength;
int ipAddr = pwdAddr+pwdLength;
int ipSubnetAddr = ipAddr+ipLength;
int ipGatewayAddr = ipSubnetAddr+ipLength;
int ipDNSAddr = ipGatewayAddr+ipLength;
int gpioAddr = ipDNSAddr+ipLength;
int gpioStateAddr = gpioAddr+gpioLength;
int timeOnAddr = gpioStateAddr+gpioLength;
int timeOffAddr = timeOnAddr+(NUM_RELAYS*timeLength);
int mqttServerAddr = timeOffAddr+(NUM_RELAYS*timeLength);
int mqttPortAddr = mqttServerAddr+ipLength;
int mqttUsernameAddr = mqttPortAddr+portLength;
int mqttPasswordAddr = mqttUsernameAddr+ssidLength;
int mqttTopicAddr = mqttPasswordAddr+pwdLength;
int httpApiHostAddr = mqttTopicAddr+ssidLength;

int eepromSize= httpApiHostAddr+ssidLength;


//Variables
const char* mqtt_server = "";
int mqtt_port = 0;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_topic = "";
String http_api_host = "";


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
  "<title>"+html_title+"</title>"
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

String relayStateMqtt(int relayno){
  if(RELAY_NO){
    if(digitalRead(eeprom_read_single(gpioAddr+relayno))){
      return "1";
    }
    else {
      return "0";
    }
  }
  else {
    if(digitalRead(eeprom_read_single(gpioAddr+relayno))){
      return "0";
    }
    else {
      return "1";
    }
  }
  return "0";
}

void handleRoot() {
  String buttons ="";
  for(int i=0; i<NUM_RELAYS; i++){
    buttons+= "<h4>Plug #" + String(i+1) + " - GPIO " + eeprom_read_single(gpioAddr+i) + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" "+ relayState(i) +"><span class=\"slider\"></span></label>";
  }
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">"+html_title+"</h1>"
    "<h3 style=\"margin-bottom: 20px;\">"+currentDay+", "+currentDate+" "+hours+":"+minutes+":"+seconds+"</h3><hr>"
    "<p>"+logStr+"</p>"
    "<p>"+buttons+"</p>"
    "<p>To make this timer work, please make sure your wifi connected to the internet to get time from NTP Server.</p>"
    "<p>If the datetime above correct then your wifi configuration is correct.</p>"
    "<p style=\"margin-top: 40px;\"><a href=\"/settings\"><button class=\"button button2\"><i class=\"fas fa-cogs\"></i> Settings</button></a></p>"
    "<script>"
    "function toggleCheckbox(element) {"
    "var xhr = new XMLHttpRequest();"
    "if(element.checked){ xhr.open(\"GET\", \"/updaterelay?relay=\"+element.id+\"&state=1\", true); }"
    "else { xhr.open(\"GET\", \"/updaterelay?relay=\"+element.id+\"&state=0\", true); }"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleSettings() {
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Settings</h1>"
    "<h3 style=\"margin-bottom: 20px;\">"+currentDay+", "+currentDate+" "+hours+":"+minutes+":"+seconds+"</h3><hr>"
    "<p><a href=\"/wificonfig\"><button class=\"button button2\"><i class=\"fas fa-wifi\"></i> Network Config</button></a></p>"
    "<p><a href=\"/gpioconfig\"><button class=\"button button2\"><i class=\"fas fa-plug\"></i> Plug GPIO Config</button></a></p>"
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
    "}"
    "function synctime(element) {"
    "var xhr = new XMLHttpRequest();"
    "xhr.open(\"GET\", \"/synctime\", true);"
    "xhr.send();"
    "}"
    "</script>"
    "</body>"+footerHtml;
            
  server.send(200, "text/html", htmlRes);
}

void handleWifiConfigForm() {
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Network Config</h1>"
    "<form method=post action=\"/savewificonfig\" style=\"margin: 20px\">"
    "<p><h2>Wifi Config</h2></p>"
    "<p><b>SSID</b><br/><input type=text class=\"form-control\" name=ssid id=ssid value=\""+eeprom_read(ssidAddr, ssidLength)+"\"><br/>(max 32 character)</p>"
    "<p><b>Password</b><br/><input type=text class=\"form-control\" name=password id=password value=\""+eeprom_read(pwdAddr, pwdLength)+"\"><br/>(max 32 character)</p>"
    "<p><h2>IP Config</h2></p>"
    "<p>(leave empty if you want to use DHCP)</p>"
    "<p><b>IP Address</b><br/><input type=text class=\"form-control\" name=ip id=ip value=\""+eeprom_read(ipAddr, ipLength)+"\"></p>"
    "<p><b>Subnet</b><br/><input type=text class=\"form-control\" name=subnet id=subnet value=\""+eeprom_read(ipSubnetAddr, ipLength)+"\"></p>"
    "<p><b>Gateway</b><br/><input type=text class=\"form-control\" name=gateway id=gateway value=\""+eeprom_read(ipGatewayAddr, ipLength)+"\"></p>"
    "<p><b>DNS</b><br/><input type=text class=\"form-control\" name=dns id=dns value=\""+eeprom_read(ipDNSAddr, ipLength)+"\"></p>"
    "<p><h2>MQTT Config</h2></p>"
    "<p><b>Server Address</b><br/><input type=text class=\"form-control\" name=mqttserver id=mqttserver value=\""+eeprom_read(mqttServerAddr, ipLength)+"\"></p>"
    "<p><b>port</b><br/><input type=text class=\"form-control\" name=mqttport id=mqttport value=\""+eeprom_read(mqttPortAddr, portLength)+"\"><br/>(max 4 character)</p>"
    "<p><b>Username</b><br/><input type=text class=\"form-control\" name=mqttusername id=mqttusername value=\""+eeprom_read(mqttUsernameAddr, ssidLength)+"\"><br/>(max 32 character)</p>"
    "<p><b>Password</b><br/><input type=text class=\"form-control\" name=mqttpassword id=mqttpassword value=\""+eeprom_read(mqttPasswordAddr, pwdLength)+"\"><br/>(max 32 character)</p>"
    "<p><b>Topic</b><br/><input type=text class=\"form-control\" name=mqtttopic id=mqtttopic value=\""+eeprom_read(mqttTopicAddr, ssidLength)+"\"><br/>(max 32 character)</p>"
    "<p><b>HTTP Api Host</b><br/><input type=text class=\"form-control\" name=httpapihost id=httpapihost value=\""+eeprom_read(httpApiHostAddr, ssidLength)+"\"><br/>(max 32 character)</p>"
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
  eeprom_write(server.arg("mqttserver"), mqttServerAddr,ipLength);
  eeprom_write(server.arg("mqttport"), mqttPortAddr,portLength);
  eeprom_write(server.arg("mqttusername"), mqttUsernameAddr,ssidLength);
  eeprom_write(server.arg("mqttpassword"), mqttPasswordAddr,pwdLength);
  eeprom_write(server.arg("mqtttopic"), mqttTopicAddr,ssidLength);
  eeprom_write(server.arg("httpapihost"), httpApiHostAddr,ssidLength);
  
  server.send(200, "text/html", savedNotifHtml);
}

void handleGPIOConfigForm() {
  String inputForm="";
  for(int i=0; i<NUM_RELAYS; i++){
    String defaultStateCheck = "";
    if(eeprom_read_single(gpioStateAddr+i)==1){
      defaultStateCheck = "checked";
    }
    inputForm += "<p><b>Plug #"+String(i+1)+" GPIO Number</b></br><input type=text class=\"form-control\" name=gpio"+i+" id=gpio"+i+" value=\""+eeprom_read_single(gpioAddr+i)+"\"></p>"
    "<p><b>Default State</b> <input type=checkbox class=\"form-control\" name=defaultstate"+(i)+" id=defaultstate"+(i)+" "+defaultStateCheck+" value=\"1\" style=\"box-shadow: 0 0 black;\"> On </p>";
  }
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Plug GPIO Config</h1>"
    "<form method=post action=\"/savegpioconfig\" style=\"margin: 20px\">"+inputForm+""
    "<p><button type=submit value=Save class=\"button button2\"><i class=\"fas fa-save\"></i> Save</button> <button type=\"button\" onclick=\"window.location.href = '/';\" class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Cancel</button></p>"
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
    inputForm += "<p><b>Timer Plug #"+String(i+1)+" On</b></br><input type=text class=\"form-control\" name=gpioon"+i+" id=gpioon"+i+" value=\""+eeprom_read(timeOnAddr+(i*timeLength), timeLength)+"\"></p>";
    inputForm += "<p><b>Timer Plug #"+String(i+1)+" Off</b></br><input type=text class=\"form-control\" name=gpiooff"+i+" id=gpiooff"+i+" value=\""+eeprom_read(timeOffAddr+(i*timeLength), timeLength)+"\"></p>";
  }
  
  String htmlRes  = headerHtml + "<body><h1 class=\"header\">Timer Config</h1>"
    "<form method=post action=\"/savetimerconfig\" style=\"margin: 20px\">"+inputForm+""
    "<p>Format hour:minute:second without \"0\"<br/>"
    "For multiple time input with \";\" delimitier<br/>"
    "To save memory it has each limit "+String(TIMER_LIMIT)+" times setting maximum<br/>"
    "<b>Example:</b> 1:30:0;6:8:0;18:7:12</p>"
    "<p><button type=submit value=Save class=\"button button2\"><i class=\"fas fa-save\"></i> Save</button> <button type=\"button\" onclick=\"window.location.href = '/';\" class=\"button button2\"><i class=\"fas fa-arrow-left\"></i> Cancel</button></p>"
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

void handleSyncTime() {
  syncTime();
  server.send(200, "text/plain", "OK");
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

    if(WiFi.status() != WL_CONNECTED) {
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
      
      //Set ap_hostname
      if (!MDNS.begin(ap_hostname)) {
          Serial.println("Error setting up MDNS responder!");
      }
      
      timeClient.begin();
      syncTime();
    }
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void syncTime(){
  if (WiFi.status() == WL_CONNECTED){
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

void mqttConnect() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    if (client.connect(ap_hostname, mqtt_user, mqtt_password)) {
      client.subscribe(mqtt_topic);
    } else {
      delay(5000);
    }
  }
}

void callback(char* mqtt_topic, byte* payload, unsigned int length) {
  String relayno = "";
  String deviceid = "";
  int nextDataIndex = 2;
  // GET Switch Number
  for (int i = nextDataIndex; i < length; i++) {
    if ((char)payload[i] == '|') {
      nextDataIndex = i+1;
      break;
    }
    relayno += (char)payload[i];
  }

  // GET Device ID
  for (int i = nextDataIndex; i < length; i++) {
    if ((char)payload[i] == '|') {
      nextDataIndex = i+1;
      break;
    }
    deviceid += (char)payload[i];
  }

  if ((char)payload[0] == '1') {
    String datastate = "";
    for(int i=0; i<NUM_RELAYS; i++){
      datastate+= String(i)+"-"+relayStateMqtt(i)+";";
    }
    httpPostCall(http_api_host+"/device/action/state?id="+deviceid,datastate);
  }else if ((char)payload[0] == '2') {
    if ((char)payload[1] == '1') {
      updateRelay(relayno.toInt(),1);
    }else{
      updateRelay(relayno.toInt(),0);
    }
  }
}

void httpPostCall(String url, String postData){
  httpClient.begin(url);
  httpClient.addHeader("Content-Type", "text/plain");
  int httpCode = httpClient.POST(postData);
  httpClient.end();
}


void httpGetCall(String url){
  httpClient.begin(url);
  int httpCode = httpClient.GET();
  httpClient.end();
}

void setup() {
  Serial.begin(115200);
  delay(100);

//  mqtt_server = eeprom_read(mqttServerAddr, ipLength).c_str();
//  mqtt_port = eeprom_read(mqttPortAddr, portLength).toInt();
//  mqtt_user = eeprom_read(mqttUsernameAddr, ssidLength).c_str();
//  mqtt_password = eeprom_read(mqttPasswordAddr, pwdLength).c_str();
//  mqtt_topic = eeprom_read(mqttTopicAddr, ssidLength).c_str();
//  http_api_host = eeprom_read(httpApiHostAddr, ssidLength);

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
  server.on("/settings", handleSettings);
  server.on("/wificonfig", handleWifiConfigForm);
  server.on("/savewificonfig", HTTP_POST, handleSaveWifiConfigForm);
  server.on("/gpioconfig", handleGPIOConfigForm);
  server.on("/savegpioconfig", HTTP_POST, handleSaveGPIOConfigForm);
  server.on("/timerconfig", handleTimerConfigForm);
  server.on("/savetimerconfig", HTTP_POST, handleSaveTimerConfigForm);
  server.on("/updaterelay", HTTP_GET, handleUpdateRelay);
  server.on("/restart", HTTP_GET, handleRestart);
  server.on("/synctime", HTTP_GET, handleSyncTime);
  server.begin();

  // Arduino OTA Setup
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
    
    // retry wifi connection
    if (seconds == 30 and WiFi.status() != WL_CONNECTED){
      connectToWifi();
    }

    // MQTT
    if(mqtt_topic!=""){
      if(WiFi.status() == WL_CONNECTED){
        if (!client.connected()) {
          mqttConnect();
        }
        client.loop();
      }
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
