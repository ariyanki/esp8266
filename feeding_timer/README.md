# Smart Feeder

I made this code to control my fish feeder


For the first time, before you upload your code to the board, please see this configuration:

```
// Access Point network credentials
const char* ap_ssid     = "nodemcu";
const char* ap_password = "nodemcu12345";
```

You need that Access Point Credential (you can change it) to connect for the first time before you can configure the wifi connection. 

After you connect to that Access point, go to **http://192.168.4.1** (default AP ip address) in your browser.

**Homepage**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/feeding_timer/home.png" width="250" title="hover text">
</p>


**Wifi Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/feeding_timer/wifi-config.png" width="250" title="hover text">
</p>

**Saved Notification**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/feeding_timer/saved-notif.png" width="250" title="hover text">
</p>

**Servo Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/feeding_timer/servo-config.png" width="250" title="hover text">
</p>

**Timer Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/feeding_timer/timer-config.png" width="250" title="hover text">
</p>

To change number of timer, modify this code:
```
// #### Timer Configuration ####
#define TIMER_LIMIT 24
```

#### Notes ####
Timer limit impacted to eeprom size which only has max 4096 bytes for nodeMCU. Every time need 9 char, so the calculation = 9 * [TIMER_LIMIT]

