#include <Arduino.h>
#include "./wifi_config.h"
#include <ESPAsync_WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "za.pool.ntp.org", 2*60*60);

#define TRIGGER_PIN 0

void setupWIFI(){
  ESP_LOGI(TAG, "Attempting to connect to wifi");
  WiFi.begin();
}

void setupPins(){
  ESP_LOGI(TAG, "Setting up pins");
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
}

void setupNTP(){
  timeClient.begin();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  ESP_LOGI(TAG, "Midea AC Controller");
  setupPins();
  setupWIFI();
  setupNTP();
}

void checkIfConfigWifiActivated(){
  if ((digitalRead(TRIGGER_PIN) == LOW))
  {
    ESP_LOGI(TAG, "\nConfiguration portal requested.");
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
void wifiLoop(){
  if(!WiFi.isConnected()){
    delay(1000);
    WiFi.begin();
    wifiConnectAttempts++;
  } else {
    wifiConnectAttempts = 0;
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
  if(millis() - updateStampDebug >= updateIntervalDebug){
    updateStampDebug = millis();
    ESP_LOGI(TAG, "Time: %s", timeClient.getFormattedTime().c_str());
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  checkIfConfigWifiActivated();
  wifiLoop();
  updateNTPTime();
}
