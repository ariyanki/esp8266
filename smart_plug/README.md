# Smart Plug

I made this code to control my electric plugs


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
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/smart_plug/home.png" width="250" title="hover text">
</p>


**Wifi Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/smart_plug/wifi-config.png" width="250" title="hover text">
</p>

**Saved Notification**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/smart_plug/saved-notif.png" width="250" title="hover text">
</p>

**Plug GPIO Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/smart_plug/plug-config.png" width="250" title="hover text">
</p>
To set number of Plug or Relay Mode (Normally Open/Normally Close), modify this code:

```
// #### Relay Configuration ####
#define RELAY_NO    true
#define NUM_RELAYS 6
```

**Timer Config**
<p align="center">
  <img src="https://raw.githubusercontent.com/ariyanki/esp8266/master/Docs/image/smart_plug/timer-config.png" width="250" title="hover text">
</p>

To change number of timer, modify this code:
```
// #### Feeding Timer Configuration ####
#define TIMER_LIMIT 24
```

#### Notes ####
Number of Plug and timer limit impacted to eeprom size which only has max 4096 bytes for nodeMCU. Every time need 9 char, so the calculation = 9 * [TIMER_LIMIT] * [NUM_RELAYS]

