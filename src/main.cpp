#include <Arduino.h>
#include "./wifi_config.h"
#include "./sinric_config.h"
#include <ESPAsync_WiFiManager.h>
#include <NTPClient.h>
#include <AsyncElegantOTA.h>
#include <SinricPro.h>
#include <SinricProWindowAC.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "za.pool.ntp.org", 2*60*60);

#define TRIGGER_PIN 0

#define TIME_ON_DEPTHS  2
#define TIME_OFF_DEPTHS 2
#define TIMES_VALUES 3

uint8_t timesOn[TIME_ON_DEPTHS][TIMES_VALUES] = {{22,0,0}, {6,0,0}};
uint8_t timesOff[TIME_OFF_DEPTHS][TIMES_VALUES] = {{23,30,0}, {7,30,0}};

AsyncWebServer server(80);

void setupWIFI(){
  ESP_LOGI(TAG, "Attempting to connect to wifi");
  WiFi.begin();
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    ESP_LOGD(TAG, "WiFi Failed!");
    return;
  }
}

void setupPins(){
  ESP_LOGI(TAG, "Setting up pins");
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
}

void setupNTP(){
  timeClient.begin();
}

void setupOTA(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is the MIDEA AC Controller.\nGo to /update to update this device using a binary");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  ESP_LOGI(TAG, "Midea AC Controller");
  setupPins();
  setupWIFI();
  setupNTP();
  setupOTA();
  Serial.println(APP_KEY);
  Serial.println(APP_SECRET);
  Serial.println(AC_ID);
}

void checkIfConfigWifiActivated(){
  if ((digitalRead(TRIGGER_PIN) == LOW))
  {
    ESP_LOGI(TAG, "Stopping server");
    server.end();
    delay(1000);
    ESP_LOGI(TAG, "Disconnecting WIFI");
    WiFi.disconnect();
    delay(1000);
    ESP_LOGI(TAG, "Configuration portal requested.");
    // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
    digitalWrite(LED_BUILTIN, LED_ON);

    //Local initialization. Once its business is done, there is no need to keep it around
    AsyncWebServer webServer(HTTP_PORT);
    DNSServer dnsServer;

    ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "ESP_AC_CNTR");

    ESPAsync_wifiManager.setMinimumSignalQuality(-1);
    ESPAsync_wifiManager.setConfigPortalChannel(0);

    //Check if there is stored WiFi router/password credentials.
    //If not found, device will remain in configuration mode until switched off via webserver.
    ESP_LOGI(TAG, "Opening configuration portal. ");

    Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
    Router_Pass = ESPAsync_wifiManager.WiFi_Pass();

    //Remove this line if you do not want to see WiFi password printed
    String ssidNpass = Router_SSID + ", Pass = " + Router_Pass;
    ESP_LOGI(TAG, "ESP Self-Stored: SSID = %s",  ssidNpass.c_str());

    // From v1.1.0, Don't permit NULL password
    if ( (Router_SSID != "") && (Router_Pass != "") )
    {
      LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
      wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

      ESPAsync_wifiManager.setConfigPortalTimeout(300);
      //If no access point name has been previously entered disable timeout.
      ESP_LOGI(TAG, "Got ESP Self-Stored Credentials. Timeout 120s for Config Portal");
    } else {
      // Enter CP only if no stored SSID on flash and file
      ESP_LOGI(TAG, "Open Config Portal without Timeout: No stored Credentials.");
    }

    //Starts an access point
    //and goes into a blocking loop awaiting configuration
    if (!ESPAsync_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    {
      ESP_LOGI(TAG, "Not connected to WiFi but continuing anyway.");
    } else {
      //if you get here you have connected to the WiFi
      ESP_LOGI(TAG, "connected...yeey :");
      ESP_LOGI(TAG, "Local IP: %i.%i.%i.%i",
                WiFi.localIP()[0],
                WiFi.localIP()[1],
                WiFi.localIP()[2],
                WiFi.localIP()[3]);
    }

    // Only clear then save data if CP entered and with new valid Credentials
    // No CP => stored getSSID() = ""
    if ( String(ESPAsync_wifiManager.getSSID(0)) != "" && String(ESPAsync_wifiManager.getSSID(1)) != "" )
    {
      // Stored  for later usage, from v1.1.0, but clear first
      memset(&WM_config, 0, sizeof(WM_config));

      for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
      {
        String tempSSID = ESPAsync_wifiManager.getSSID(i);
        String tempPW   = ESPAsync_wifiManager.getPW(i);

        if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1){
          strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
        } else{
          strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);
        }

        if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1){
          strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
        } else{
          strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);
        }

        // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
        if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "")
            && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
        {
          LOGERROR3(F("* Add SSID = "),
                      WM_config.WiFi_Creds[i].wifi_ssid,
                      F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
          wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
        }
      }
    }
  }
}

int wifiConnectAttempts = 0;
bool comingFromDisconnect = false;
void wifiLoop(){
  if(!WiFi.isConnected()){
    comingFromDisconnect = true;
    server.end();
    delay(1000);
    WiFi.begin();
    wifiConnectAttempts++;
  } else {
    wifiConnectAttempts = 0;
  }

  if(WiFi.isConnected() && comingFromDisconnect){
    comingFromDisconnect = true;
    setupOTA();
  }

  // if too many attempts take along break
  if(wifiConnectAttempts >= 10){
    delay(60*1000);
  }
}

// timer stamps
uint32_t updateStamp = 0;
const uint32_t updateInterval = 60*1000;
// debug stamps
uint32_t updateStampDebug = 0;
const uint32_t updateIntervalDebug = 1000;
// first update may need to be forced
bool firstUpdateSuccess = false;
void updateNTPTime(){
  // if there has been no update since boot, force an update
  if(!firstUpdateSuccess){
    firstUpdateSuccess = timeClient.update();
  }
  // update only eveery 60 seconds
  if(millis() - updateStamp >= updateInterval){
    updateStamp = millis();
    timeClient.update();
  }
  // print time every seconds
  #ifdef DEBUG_NTP
  if(millis() - updateStampDebug >= updateIntervalDebug){
    updateStampDebug = millis();
    ESP_LOGI(TAG, "Time: %s", timeClient.getFormattedTime().c_str());
  }
  #endif
}

uint8_t hourTemp = 0;
uint8_t minutesTemp = 0;
bool alreadyTurnedOn = false;
bool alreadyTurnedOff = false;
uint32_t debugStamp = 0;
void handleTimers(){
  // grab time from time client
  // only compare hours and minutes
  uint8_t hours = hourTemp;//timeClient.getHours();
  uint8_t minutes = minutesTemp;//timeClient.getMinutes();
  // compare time to see if must call turn on
  for(int i = 0; i < TIME_ON_DEPTHS; i++){
    if(hours == timesOn[i][0] && minutes == timesOn[i][1]){
      if(!alreadyTurnedOn){
        alreadyTurnedOn = true;
        ESP_LOGV(TAG, "Turn on time found: %i:%i", timesOn[i][0], timesOn[i][1]);
      }
    }else{
      alreadyTurnedOn = false;
    }
  }
  // compare time to see if must call turn off
  for(int i = 0; i < TIME_OFF_DEPTHS; i++){
    if(hours == timesOff[i][0] && minutes == timesOff[i][1]){
      if(!alreadyTurnedOn){
        alreadyTurnedOff = true;
        ESP_LOGV(TAG, "Turn off time found: %i:%i", timesOff[i][0], timesOff[i][1]);
      }
    }else{
      alreadyTurnedOff = false;
    }
  }
  // debug timer
  #ifdef DEBUG_TIMER_HANDLE
  if(millis() - debugStamp >= 10){
    debugStamp = millis();
    minutesTemp++;
    if(minutesTemp >= 60){
      hourTemp++;
      minutesTemp = 0;
    }
    if(hourTemp >= 24){
      hourTemp = 0;
      minutesTemp = 0;
    }
    ESP_LOGV(TAG, "MOCK TIME: %i:%i", hourTemp, minutesTemp);
  }
  #endif
}

void loop() {
  // put your main code here, to run repeatedly:
  checkIfConfigWifiActivated();
  wifiLoop();
  updateNTPTime();
  handleTimers();
  vTaskDelay(1);
  
}
